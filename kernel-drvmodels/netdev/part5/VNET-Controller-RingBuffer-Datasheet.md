<!-- SPDX-License-Identifier: GPL-2.0 -->
<!-- Copyright (c) 2024 TECH VEDA -->
<!-- Author: Raghu Bharadwaj -->

# VNET Virtual Ethernet Controller - Ring Buffer & DMA Edition
## Hardware Datasheet for Part 5: TX Ring Buffer & Full Duplex Rings

---

## 1. Overview

Part 5 introduces DMA-based packet transmission using a full 256-entry TX
descriptor ring, while carrying forward the 64-entry RX descriptor ring from
Part 4. Both TX and RX now use the same descriptor ring pattern with coherent
DMA for descriptors and either streaming DMA (TX) or coherent DMA (RX) for
packet buffers. The controller supports simultaneous TX and RX operation with
interrupt-driven completion and flow control.

### 1.1 Concepts Introduced in Part 5
- TX descriptor ring (256 entries) via `dma_alloc_coherent`
- TX streaming DMA via `dma_map_single` per packet
- Ring head/tail management with wrap-around and doorbell writes
- TX completion: walk ring, check OWN flag, unmap, free skb
- Flow control: `netif_stop_queue` / `netif_wake_queue`
- Symmetric ring structure for both TX and RX

### 1.2 Carried Forward from Part 4
- RX descriptor ring (64 entries) with coherent DMA for descriptors and buffers
- Interrupt handler with TX_COMPLETE and RX_PACKET dispatch
- RX processing in ISR with `netif_rx()`

---

## 2. DMA Architecture

```
 +----------+         PCI Bus          +---------------------------------+
 |          |<========================>|         Main Memory             |
 |   VNET   |   Bus Master DMA        |                                 |
 | Controller|                         |  +---------------------------+  |
 |          |   dma_alloc_coherent -->|  | TX Descriptor Ring        |  |
 |  TX DMA  |   (bidirectional,       |  | 256 x 16 bytes = 4096 B  |  |
 |  Engine  |    coherent mapping)    |  +---------------------------+  |
 |          |                         |                                 |
 |          |   dma_map_single ------>|  +---------------------------+  |
 |          |   (DMA_TO_DEVICE,       |  | TX Packet Data Buffer     |  |
 |          |    streaming mapping)   |  | (per-packet, up to 1518 B)|  |
 |          |                         |  +---------------------------+  |
 |          |                         |                                 |
 |  RX DMA  |   dma_alloc_coherent -->|  +---------------------------+  |
 |  Engine  |   (bidirectional,       |  | RX Descriptor Ring        |  |
 |          |    coherent mapping)    |  | 64 x 16 bytes = 1024 B   |  |
 |          |                         |  +---------------------------+  |
 |          |                         |                                 |
 |          |   dma_alloc_coherent -->|  +---------------------------+  |
 |          |   (DMA_FROM_DEVICE,     |  | RX Packet Buffers         |  |
 |          |    coherent mapping)    |  | 64 x 1518 B each          |  |
 +----------+                         |  +---------------------------+  |
                                      +---------------------------------+

 Coherent mapping:  CPU and device see writes immediately, no explicit
                    sync needed. Used for descriptors and RX buffers.

 Streaming mapping: Must be unmapped after DMA completes. Used for
                    TX packet data (large, one-direction).
```

---

## 3. Descriptor Format

Each descriptor is 16 bytes (4 x 32-bit fields). The same format is used
for both TX and RX descriptors.

```
 Offset   Field     Bits
 ------   -----     ----
  0x00    addr       31                              0
           +---------------------------------------------+
           |   Buffer DMA Address (32-bit)               |
           +---------------------------------------------+

  0x04    len        31                              0
           +---------------------------------------------+
           |   Buffer Length in bytes (32-bit)            |
           +---------------------------------------------+

  0x08    flags      31   30   29   28              0
           +----+----+----+-------------------------+
           |OWN |SOP |EOP |       Reserved           |
           +----+----+----+-------------------------+

  0x0C    status     31              16  15          0
           +------------------------+---+------------+
           |       Reserved         |OK |  LEN_MASK  |
           +------------------------+---+------------+
```

### 3.1 Flag Definitions (from vnet_hw_interface.h)

| Flag              | Value     | Description                              |
|-------------------|-----------|------------------------------------------|
| VNET_DESC_OWN     | BIT(31)   | 1 = HW owns descriptor, 0 = driver owns |
| VNET_DESC_SOP     | BIT(30)   | Start of packet                          |
| VNET_DESC_EOP     | BIT(29)   | End of packet                            |

### 3.2 Status Definitions (from vnet_hw_interface.h)

| Field                    | Value    | Description                       |
|--------------------------|----------|-----------------------------------|
| VNET_DESC_STATUS_OK      | BIT(0)   | Transmission/reception completed OK |
| VNET_DESC_STATUS_LEN_MASK| 0xFFFF   | Actual bytes transferred (16-bit) |

### 3.3 TX vs RX Descriptor Usage

| Aspect          | TX Descriptor                    | RX Descriptor                     |
|-----------------|----------------------------------|-----------------------------------|
| addr field      | DMA addr from dma_map_single     | DMA addr from dma_alloc_coherent  |
| len field       | Packet length (set by driver)    | Buffer size (set by driver)       |
| OWN flag set by | Driver (before doorbell)         | Driver (when re-posting)          |
| OWN flag cleared| Hardware (after transmit)        | Hardware (after receive)          |
| status field    | Written by HW on completion      | Written by HW with received len   |

---

## 4. TX Ring Buffer

The TX ring is a circular array of 256 descriptors. The driver tracks a
software head (next descriptor to fill) and the hardware advances through
descriptors starting from where the driver wrote the tail register.

### 4.1 Ring States

```
 EMPTY (head == tail, no OWN set):
 +-----+-----+-----+-----+-----+-----+-----+-----+
 |     |     |     |     |     |     |     |     |  ...x256
 +-----+-----+-----+-----+-----+-----+-----+-----+
   ^
   head/tail

 PARTIALLY FULL (head advanced past tail):
 +-----+-----+-----+-----+-----+-----+-----+-----+
 |     | OWN | OWN | OWN |     |     |     |     |  ...x256
 +-----+-----+-----+-----+-----+-----+-----+-----+
   ^     ^                   ^
   |     tail (HW reads)    head (driver writes)
   |
   completed (OWN cleared)

 FULL (head + 1 == tail, modulo ring size):
 +-----+-----+-----+-----+-----+-----+-----+-----+
 | OWN | OWN | OWN | OWN | OWN | OWN | OWN |     |  ...x256
 +-----+-----+-----+-----+-----+-----+-----+-----+
                                               ^ ^
                                          head  tail
   --> netif_stop_queue() called
```

Ring size: **256 descriptors** (each 16 bytes = 4096 bytes total).
One slot is always kept empty to distinguish full from empty.

---

## 5. RX Ring Buffer

The RX ring is a circular array of 64 descriptors. Each descriptor points to
a pre-allocated coherent DMA buffer. The hardware fills buffers with received
packets and clears the OWN flag. The driver walks the ring from rx_tail,
processes completed descriptors, and re-posts them with OWN set.

### 5.1 RX Ring Memory Layout

The RX ring uses `dma_alloc_coherent` for both descriptors and packet buffers
(carried forward from Part 4):

```
  Descriptor Ring (dma_alloc_coherent, 64 x 16 = 1024 bytes)
  +--------+--------+--------+--------+--------+--------+
  | desc 0 | desc 1 | desc 2 | desc 3 |  ...   |desc 63 |
  |addr=d0 |addr=d1 |addr=d2 |addr=d3 |        |addr=d63|
  |len=1518|len=1518|len=1518|len=1518|        |len=1518|
  |flg=OWN |flg=OWN |flg=OWN |flg=OWN |        |flg=OWN |
  +---+----+---+----+---+----+---+----+--------+---+----+
      |        |        |        |                  |
      v        v        v        v                  v
  +------+ +------+ +------+ +------+          +------+
  |buf[0]| |buf[1]| |buf[2]| |buf[3]|          |buf[63]|
  |1518 B| |1518 B| |1518 B| |1518 B|   ...    |1518 B|
  +------+ +------+ +------+ +------+          +------+
  (each buffer allocated with dma_alloc_coherent)
```

### 5.2 RX Ring States

```
 ALL OWNED BY HARDWARE (initial state):
 +-----+-----+-----+-----+-----+-----+-----+-----+
 | OWN | OWN | OWN | OWN | OWN | OWN | OWN | OWN |  ...x64
 +-----+-----+-----+-----+-----+-----+-----+-----+
   ^
   rx_tail = 0


 SOME PACKETS RECEIVED (OWN cleared by HW):
 +-----+-----+-----+-----+-----+-----+-----+-----+
 | pkt | pkt | pkt | OWN | OWN | OWN | OWN | OWN |  ...x64
 +-----+-----+-----+-----+-----+-----+-----+-----+
   ^                  ^
   rx_tail=0          HEAD=3
   (driver reads)     (HW writes)


 AFTER DRIVER PROCESSES AND RE-POSTS:
 +-----+-----+-----+-----+-----+-----+-----+-----+
 | OWN | OWN | OWN | OWN | OWN | OWN | OWN | OWN |  ...x64
 +-----+-----+-----+-----+-----+-----+-----+-----+
                      ^
                      rx_tail=3, HEAD=3 (caught up)
```

Ring size: **64 descriptors** (each 16 bytes = 1024 bytes total).
Buffer size: **1518 bytes** per RX buffer slot (coherent DMA).

---

## 6. TX Data Flow

```
  Driver (CPU)                        Hardware (VNET Controller)
  ------------                        ----------------------------

  1. Map packet buffer
     dma_map_single(skb->data)
           |
  2. Fill descriptor:
     desc[head].addr   = dma_addr
     desc[head].len    = skb->len
     desc[head].status = 0
     desc[head].flags  = OWN | SOP | EOP
           |
  3. Advance head:
     head = (head + 1) % 256
           |
  4. Ring doorbell:                   5. Read descriptor at tail
     iowrite32(head,                     (OWN bit is set)
       base + TX_RING_HEAD)                    |
           |                          6. DMA-read packet data
           |                             from desc.addr
           |                                   |
           |                          7. Transmit packet on wire
           |                                   |
           |                          8. Write completion:
           |                             desc.status = OK | len
           |                             desc.flags &= ~OWN
           |                                   |
           |                          9. Advance tail
           |                                   |
           |                         10. Fire TX_COMPLETE IRQ
           |                                   |
 11. ISR reads INT_STATUS             <--------+
     sees TX_COMPLETE bit
           |
 12. tx_complete() runs:
     while (desc[tail].flags & OWN == 0
            && tail != head):
       - dma_unmap_single(desc[tail].addr)
       - dev_kfree_skb(skb_array[tail])
       - tail = (tail + 1) % 256
           |
 13. If ring was full and now has space:
       netif_wake_queue(ndev)
```

---

## 7. RX Data Flow

```
  Driver (CPU)                        Hardware (VNET Controller)
  ------------                        ----------------------------

  Setup (open):
  1. Allocate RX descriptors
     (dma_alloc_coherent)
  2. Allocate RX buffers
     (dma_alloc_coherent per slot)
  3. Fill all descriptors:
     desc[i].addr = buf_dma[i]
     desc[i].len  = 1518
     desc[i].flags = OWN
           |
  4. Register with platform:          5. Hardware begins watching
     vnet_hw_set_rx_ring(...)             for incoming packets
                                               |
                                      6. Packet arrives on wire
                                               |
                                      7. Copy data into buf at
                                         desc[head].addr
                                               |
                                      8. Write completion:
                                         desc.status = OK | pkt_len
                                         desc.flags &= ~OWN
                                               |
                                      9. Fire RX_PACKET IRQ
                                               |
 10. ISR reads INT_STATUS             <--------+
     sees RX_PACKET bit
           |
 11. rx_process() runs:
     while (desc[rx_tail].flags & OWN == 0):
       - alloc skb
       - memcpy from coherent buffer
       - skb->protocol = eth_type_trans()
       - netif_rx(skb)
       - re-post: desc.flags = OWN
       - rx_tail = (rx_tail + 1) % 64
```

---

## 8. DMA Mapping Types

```
  +-------------------------------------------------------------------+
  |                    DMA Mapping Comparison                          |
  +-------------------+-----------------+-----------------------------+
  |                   | TX Desc Ring    | TX Packet Data              |
  +-------------------+-----------------+-----------------------------+
  | API               |dma_alloc_coherent| dma_map_single             |
  | Lifetime          | driver lifetime | per-packet                  |
  | Direction         | bidirectional   | DMA_TO_DEVICE               |
  | Sync required     | no              | unmap after completion      |
  | Size              | 4096 bytes      | up to 1518 bytes            |
  |                   | (256 x 16)      | (per packet)                |
  +-------------------+-----------------+-----------------------------+
  |                   | RX Desc Ring    | RX Packet Buffers           |
  +-------------------+-----------------+-----------------------------+
  | API               |dma_alloc_coherent| dma_alloc_coherent         |
  | Lifetime          | driver lifetime | driver lifetime             |
  | Direction         | bidirectional   | bidirectional (HW writes)   |
  | Sync required     | no              | no                          |
  | Size              | 1024 bytes      | 1518 bytes x 64             |
  |                   | (64 x 16)       | (per buffer slot)           |
  +-------------------+-----------------+-----------------------------+
```

---

## 9. Flow Control

```
  ndo_start_xmit()
        |
        v
  +--------------+    YES    +----------------------+
  |  Ring full?   |--------->|  netif_stop_queue()  |
  | (free < 1)   |          |  return NETDEV_TX_BUSY|
  +------+-------+          +----------------------+
         | NO
         v
  +--------------+
  | Fill desc,   |
  | advance head,|
  | ring doorbell|
  +------+-------+
         |
         v
  +--------------+    YES    +----------------------+
  | Ring now full?|--------->|  netif_stop_queue()  |
  | after insert  |          |  (prevent next xmit) |
  +------+-------+          +----------------------+
         | NO
         v
       return


  TX Completion (interrupt context):
        |
        v
  +------------------+
  | Clean completed  |
  | descriptors      |
  | (OWN == 0)       |
  +------+-----------+
         |
         v
  +------------------+    YES    +----------------------+
  | Queue stopped    |---------->|  netif_wake_queue()  |
  | AND ring has     |           |  (resume TX from     |
  | free space?      |           |   network stack)     |
  +------+-----------+           +----------------------+
         | NO
         v
       done
```

---

## 10. TX Ring Registers

| Offset | Name          | Type | Reset      | Description                           |
|--------|---------------|------|------------|---------------------------------------|
| 0x100  | TX_RING_ADDR  | R/W  | 0x00000000 | TX ring base DMA address (32-bit)     |
| 0x104  | TX_RING_SIZE  | R/W  | 0x00000100 | TX ring size (256 descriptors)        |
| 0x108  | TX_RING_HEAD  | R/W  | 0x00000000 | TX ring head (driver writes)          |
| 0x10C  | TX_RING_TAIL  | R    | 0x00000000 | TX ring tail (HW advances)            |

### 10.1 TX Register Access

```c
/* Write ring base address after dma_alloc_coherent */
iowrite32(ring_dma, base + 0x100);

/* Write ring size */
iowrite32(256, base + 0x104);

/* Initialize head and tail to 0 */
iowrite32(0, base + 0x108);
iowrite32(0, base + 0x10C);

/* Doorbell: write head to tell HW new descriptors are ready */
iowrite32(new_head, base + 0x108);
```

---

## 11. RX Ring Registers

| Offset | Name          | Type | Reset      | Description                           |
|--------|---------------|------|------------|---------------------------------------|
| 0x140  | RX_RING_ADDR  | R/W  | 0x00000000 | RX ring base DMA address (32-bit)     |
| 0x144  | RX_RING_SIZE  | R/W  | 0x00000040 | RX ring size (64 descriptors)         |
| 0x148  | RX_RING_HEAD  | R    | 0x00000000 | RX ring head (HW advances)            |
| 0x14C  | RX_RING_TAIL  | R/W  | 0x00000000 | RX ring tail (driver tracks)          |

### 11.1 RX Register Access

```c
/* Write ring base address after dma_alloc_coherent */
iowrite32(rx_descs_dma, base + 0x140);

/* Write ring size */
iowrite32(64, base + 0x144);

/* Initialize head and tail to 0 */
iowrite32(0, base + 0x148);
iowrite32(0, base + 0x14C);
```

---

## 12. Interrupt Status Bits

```
 INT_STATUS Register (Offset 0x00C):
   Bit 0 - ERROR         (1 = hardware error)
   Bit 1 - LINK_CHANGE   (1 = link state changed)
   Bit 2 - RX_PACKET     (1 = one or more RX descriptors completed)
   Bit 3 - TX_COMPLETE   (1 = one or more TX descriptors completed)
```

The driver enables all four interrupt sources and dispatches in the ISR:

```c
/* Enable in open() */
vnet_enable_irqs(regs, VNET_INT_TX_COMPLETE | VNET_INT_RX_PACKET |
                       VNET_INT_LINK_CHANGE | VNET_INT_ERROR);

/* ISR dispatch */
if (status & VNET_INT_TX_COMPLETE)  vnet_tx_complete(priv);
if (status & VNET_INT_RX_PACKET)   vnet_rx_process(priv);
if (status & VNET_INT_LINK_CHANGE) /* carrier on/off */
if (status & VNET_INT_ERROR)       /* increment error stats */
```

---

## 13. Control Register

```
 CTRL Register (Offset 0x000):
   Bit 0 - ENABLE        (master enable)
   Bit 1 - RING_ENABLE   (enable ring-based DMA)
   Bit 6 - RX_ENABLE     (enable RX engine)
   Bit 7 - TX_ENABLE     (enable TX engine)
```

Set in `ndo_open`:

```c
iowrite32(VNET_CTRL_ENABLE | VNET_CTRL_TX_ENABLE |
          VNET_CTRL_RX_ENABLE | VNET_CTRL_RING_ENABLE,
          regs + VNET_CTRL);
```

Cleared in `ndo_stop`:

```c
iowrite32(0, regs + VNET_CTRL);
```

---

## 14. Platform Calls

### 14.1 vnet_hw_set_tx_ring

Called during device open to register the TX ring with the platform:

```c
vnet_hw_set_tx_ring(pdev, tx_ring.desc, tx_ring.count);
```

### 14.2 vnet_hw_clear_tx_ring

Called during device close to unregister the TX ring:

```c
vnet_hw_clear_tx_ring(pdev);
```

### 14.3 vnet_hw_set_rx_ring

Called during device open to register the RX ring and buffers with the platform:

```c
vnet_hw_set_rx_ring(pdev, rx_descs, rx_count, rx_bufs_va, VNET_MAX_PKT_LEN);
```

Parameters:
- `rx_descs` -- kernel VA of the RX descriptor ring
- `rx_count` -- number of descriptors in the ring
- `rx_bufs_va` -- array of kernel VAs, one per RX buffer slot
- `VNET_MAX_PKT_LEN` -- size of each RX buffer (1518 bytes)

### 14.4 vnet_hw_clear_rx_ring

Called during device close to unregister the RX ring:

```c
vnet_hw_clear_rx_ring(pdev);
```

---

*Document Number: VNET-DS-RING-001*
*Revision: 3.0*
