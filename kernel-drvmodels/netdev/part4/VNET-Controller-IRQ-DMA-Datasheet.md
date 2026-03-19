<!-- SPDX-License-Identifier: GPL-2.0 -->
<!-- Copyright (c) 2024 TECH VEDA -->
<!-- Author: Raghu Bharadwaj -->

# VNET Virtual Network Controller -- IRQ & DMA Datasheet

**Applicable to**: Part 4 (Interrupts, RX Ring & Simple TX DMA)

This document describes the interrupt, DMA, and descriptor subsystems of the
VNET virtual network controller as used by the Part 4 driver.

---

## 1. System Architecture

Part 4 introduces two DMA data paths (RX and TX) and hardware interrupts.
The RX path uses coherent DMA for both descriptors and packet buffers.
The TX path uses streaming DMA with register-based writes (no descriptor ring).

```
  +-------------------------+          +-----------------------------------+
  |       CPU               |          |          Main Memory              |
  |                         |          |                                   |
  |  ioread32/iowrite32 <---+----------+--- Register File (BAR0)          |
  |                         |          |                                   |
  |  vnet_isr() <----- IRQ -+          |  +-----------------------------+ |
  |     |                   |          |  | RX Descriptor Ring          | |
  |     +-> vnet_rx_process |          |  | (dma_alloc_coherent)        | |
  |     +-> vnet_tx_complete|          |  | 64 x 16 bytes = 1024 B     | |
  |                         |          |  +-----------------------------+ |
  |  vnet_xmit() -----------+--map---->|  | RX Packet Buffers           | |
  |    dma_map_single()     |          |  | (dma_alloc_coherent)        | |
  |                         |          |  | 64 x 1518 B each            | |
  +-------------------------+          |  +-----------------------------+ |
            |                          |  | TX Packet Data (skb->data)  | |
            |                          |  | (dma_map_single, transient) | |
            |                          |  +-----------------------------+ |
            |                          +-----------------------------------+
            |
  +---------v------------------------------------------------+
  |                VNET Controller (VID:1234 DID:5678)       |
  |                                                          |
  |  +-------------+  +-----------+  +--------------------+  |
  |  | Register    |  | Interrupt |  | DMA Engines        |  |
  |  | File        |  | Logic     |  |                    |  |
  |  |             |  |           |  | RX: writes packets |  |
  |  | CTRL  0x000 |  | INT_MASK  |  |     into coherent  |  |
  |  | STATUS 0x004|  | INT_STATUS|  |     buffers        |  |
  |  | TX regs     |  |           |  |                    |  |
  |  | RX regs     |  |           |  | TX: reads packet   |  |
  |  +-------------+  +-----------+  |     from mapped    |  |
  |                                  |     DMA address     |  |
  |                                  +--------------------+  |
  +----------------------------------------------------------+
```

---

## 2. DMA Architecture: Coherent vs Streaming

Part 4 uses both DMA mapping types. This is the central teaching point.

```
  +-------------------------------------------------------------------+
  |                    DMA Mapping Comparison                          |
  +-------------------+--------------------+--------------------------+
  |                   | RX (Coherent)      | TX (Streaming)           |
  +-------------------+--------------------+--------------------------+
  | API               | dma_alloc_coherent | dma_map_single           |
  | Lifetime          | long-lived (open   | per-packet (xmit to     |
  |                   |  to stop)          |  TX_COMPLETE)            |
  | Direction         | bidirectional      | DMA_TO_DEVICE            |
  | Sync required     | no                 | unmap after completion   |
  | Who allocates     | DMA API            | driver allocs skb,       |
  |                   |                    | API maps it              |
  | Cache behavior    | uncached / write-  | normal cached, cache     |
  |                   | combined           | clean at map time        |
  | Use case          | HW writes packets  | driver writes packet,    |
  |                   | at unpredictable   | HW reads once            |
  |                   | times              |                          |
  +-------------------+--------------------+--------------------------+

  Why coherent for RX?
    The hardware writes packet data into buffers whenever a packet arrives
    on the wire. The driver has no way to predict when this will happen.
    Coherent DMA ensures the CPU always sees the latest data without
    explicit cache flush/invalidate.

  Why streaming for TX?
    The driver fills skb->data, maps it, tells HW to read it, then unmaps
    after TX_COMPLETE. This is a single-direction, one-shot transfer.
    Streaming DMA is more efficient: it uses normal cached memory and only
    requires a cache clean at map time.
```

### 2.1 DMA Memory Layout

```
  Kernel Virtual Address Space           DMA / Physical Address Space
  ============================           ============================

  rx_descs (VA) -----------------+-----> rx_descs_dma
  [desc0] addr=buf0_dma          |       Descriptor Ring (coherent)
  [desc1] addr=buf1_dma          |       64 x 16 bytes = 1024 bytes
  [desc2] addr=buf2_dma          |
  ...                            |
  [desc63] addr=buf63_dma        |

  rx_bufs_va[0] ----------------+-----> rx_bufs_dma[0]
  [1518 bytes]                   |       RX Buffer 0 (coherent)
                                 |
  rx_bufs_va[1] ----------------+-----> rx_bufs_dma[1]
  [1518 bytes]                   |       RX Buffer 1 (coherent)
  ...                            |       ...
  rx_bufs_va[63] ---------------+-----> rx_bufs_dma[63]
  [1518 bytes]                           RX Buffer 63 (coherent)

  skb->data (VA) ---------------+-----> tx_dma_addr
  [skb->len bytes]                       TX packet (streaming, transient)
```

---

## 3. Interrupt Registers

### 3.1 INT_MASK Register (offset 0x008)

Controls which interrupt sources are enabled.

```
  31                               4  3  2  1  0
  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
  |        Reserved (0)            |TC|RX|LC|ER|
  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
                                    |  |  |  |
                                    |  |  |  +-- ERROR mask (bit 0)
                                    |  |  +----- LINK_CHANGE mask (bit 1)
                                    |  +-------- RX_PACKET mask (bit 2)
                                    +----------- TX_COMPLETE mask (bit 3)

  Convention:  1 = DISABLED (masked)    0 = ENABLED (unmasked)
```

| Bit | Name            | Description                          |
|-----|-----------------|--------------------------------------|
| 0   | ERROR           | 1 = masked (disabled), 0 = enabled   |
| 1   | LINK_CHANGE     | 1 = masked (disabled), 0 = enabled   |
| 2   | RX_PACKET       | 1 = masked (disabled), 0 = enabled   |
| 3   | TX_COMPLETE     | 1 = masked (disabled), 0 = enabled   |
| 4-31| Reserved        | Must be written as 0                 |

Write 0x00000000 to enable all interrupts.
Write 0xFFFFFFFF to disable all interrupts.

### 3.2 INT_STATUS Register (offset 0x00C)

Reports pending interrupt events. **Write-1-to-clear** semantics: writing a
1 to a bit clears that pending event.

```
  31                               4  3  2  1  0
  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
  |        Reserved (0)            |TC|RX|LC|ER|
  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
                                    |  |  |  |
                                    |  |  |  +-- ERROR (bit 0)
                                    |  |  +----- LINK_CHANGE (bit 1)
                                    |  +-------- RX_PACKET (bit 2)
                                    +----------- TX_COMPLETE (bit 3)
```

**Interrupt delivery condition**: An interrupt is delivered to the CPU when:

    (INT_STATUS & ~INT_MASK) != 0

That is, at least one unmasked status bit is set.

### 3.3 Interrupt Delivery Pipeline

```
  Hardware Event (packet received, TX done, link change, error)
       |
       v
  +------------------+
  | Set INT_STATUS   |     INT_STATUS register
  | bit for event    |     (latches event)
  +--------+---------+
           |
           v
  +------------------+
  | Check INT_MASK   |     Is the corresponding
  | bit clear?       |     mask bit 0 (enabled)?
  +--------+---------+
           |
      Yes  |  No
           |  +---> Event recorded but no IRQ
           v
  +------------------+
  | Fire IRQ         |     generic_handle_irq()
  | (pdev->irq)      |
  +--------+---------+
           |
           v
  +------------------+
  | Driver ISR       |     vnet_isr()
  | 1. Read STATUS   |     status = ioread32(INT_STATUS)
  | 2. Ack bits      |     iowrite32(status, INT_STATUS)
  | 3. Handle events |     dispatch per bit
  +------------------+
```

### 3.4 ISR Decision Tree

```
  vnet_isr(irq, priv)
       |
       v
  status = ioread32(INT_STATUS)
       |
       +---> status == 0? ---> return IRQ_NONE (not our interrupt)
       |
       v
  iowrite32(status, INT_STATUS)   // acknowledge all pending bits
       |
       +---> TX_COMPLETE? ---> vnet_tx_complete(priv)
       |                       dma_unmap_single, dev_kfree_skb_irq,
       |                       netif_wake_queue
       |
       +---> RX_PACKET?  ---> vnet_rx_process(priv)
       |                       walk ring, copy to skb, netif_rx
       |
       +---> LINK_CHANGE? --> read STATUS.LINK_UP
       |                       netif_carrier_on/off()
       |
       +---> ERROR?       --> stats.rx_errors++
       |                       log error
       |
       v
  return IRQ_HANDLED
```

---

## 4. RX Ring Registers

### 4.1 RX_RING_ADDR (offset 0x140)

DMA address of the RX descriptor ring (array of `vnet_hw_desc`).

### 4.2 RX_RING_SIZE (offset 0x144)

Number of descriptors in the ring. Part 4 uses 64 (`VNET_RING_SIZE`).

### 4.3 RX_RING_HEAD (offset 0x148)

**Hardware write pointer**. Points to the next descriptor the hardware will
fill with a received packet. Updated by hardware after writing a packet.

### 4.4 RX_RING_TAIL (offset 0x14C)

**Software read pointer**. The driver updates this as it processes received
packets (tracked in software as `priv->rx_tail`).

---

## 5. RX Ring Setup

### 5.1 Memory Allocation Sequence

The driver allocates three things in `vnet_rx_alloc_ring()`:

```
  Step 1: Descriptor Ring (dma_alloc_coherent)
  +--------------------------------------------------------------+
  | dma_alloc_coherent(dev, 64 * sizeof(vnet_hw_desc),           |
  |                    &rx_descs_dma, GFP_KERNEL)                |
  |                                                              |
  | Returns:  rx_descs     = kernel VA pointer                   |
  |           rx_descs_dma = DMA address for hardware            |
  |                                                              |
  | Size: 64 descriptors x 16 bytes = 1024 bytes                |
  +--------------------------------------------------------------+

  Step 2: Tracking Arrays (kcalloc, normal kernel memory)
  +--------------------------------------------------------------+
  | rx_bufs_va  = kcalloc(64, sizeof(void *))    // VA per slot  |
  | rx_bufs_dma = kcalloc(64, sizeof(dma_addr_t))// DMA per slot |
  +--------------------------------------------------------------+

  Step 3: Per-Slot Buffers (dma_alloc_coherent, 64 times)
  +--------------------------------------------------------------+
  | for each slot i = 0..63:                                     |
  |   rx_bufs_va[i] = dma_alloc_coherent(dev, 1518,             |
  |                                       &rx_bufs_dma[i],      |
  |                                       GFP_KERNEL)            |
  |                                                              |
  | Each returns a 1518-byte coherent buffer that the hardware   |
  | can write into at any time without explicit CPU sync.        |
  +--------------------------------------------------------------+
```

### 5.2 Descriptor Initialization

After allocation, each descriptor is initialized to give the buffer to hardware:

```
  for i = 0..63:

    rx_descs[i].addr   = rx_bufs_dma[i]    // DMA addr of buffer
    rx_descs[i].len    = 1518              // buffer capacity
    rx_descs[i].flags  = VNET_DESC_OWN     // hardware owns this slot
    rx_descs[i].status = 0                 // no packet yet


  Descriptor Ring after initialization:
  +------+------+------+------+------+------+------+------+
  | OWN  | OWN  | OWN  | OWN  | OWN  | OWN  | OWN  | OWN  | ...x64
  |desc0 |desc1 |desc2 |desc3 |desc4 |desc5 |desc6 |desc7 |
  +--+---+--+---+--+---+--+---+--+---+--+---+--+---+--+---+
     |      |      |      |      |      |      |      |
     v      v      v      v      v      v      v      v
  +------+------+------+------+------+------+------+------+
  |buf[0]|buf[1]|buf[2]|buf[3]|buf[4]|buf[5]|buf[6]|buf[7]| ...x64
  |1518 B|1518 B|1518 B|1518 B|1518 B|1518 B|1518 B|1518 B|
  +------+------+------+------+------+------+------+------+
  (coherent DMA -- hardware can write to any buffer at any time)
```

### 5.3 Register Programming (in open)

```
  iowrite32(rx_descs_dma, regs + 0x140)  // RX_RING_ADDR = ring DMA addr
  iowrite32(64,           regs + 0x144)  // RX_RING_SIZE = 64 descriptors
  iowrite32(0,            regs + 0x148)  // RX_RING_HEAD = 0
  iowrite32(0,            regs + 0x14C)  // RX_RING_TAIL = 0
```

### 5.4 Platform Registration

```
  vnet_hw_set_rx_ring(pdev, rx_descs, 64, rx_bufs_va, 1518)

  Tells the platform simulator where the descriptors and buffers live
  (kernel VAs) so it can write synthetic packets into the coherent buffers.
```

### 5.5 Complete RX Setup Sequence in open()

```
  vnet_open()
       |
       v
  vnet_rx_alloc_ring()
       |
       +-- dma_alloc_coherent (descriptor ring)
       +-- kcalloc (tracking arrays)
       +-- for each slot:
       |       dma_alloc_coherent (buffer)
       |       init descriptor: addr, len, flags=OWN
       |
       v
  vnet_hw_set_rx_ring(pdev, descs, 64, bufs, 1518)
       |
       v
  Program RX ring registers (ADDR, SIZE, HEAD=0, TAIL=0)
       |
       v
  vnet_irq_enable()   // unmask all interrupts
       |
       v
  iowrite32(ENABLE | TX_ENABLE | RX_ENABLE | RING_ENABLE, CTRL)
       |
       v
  netif_start_queue()
       |
       v
  [Hardware begins generating RX packets into ring]
```

---

## 6. RX Receive Flow

### 6.1 Packet Reception Sequence

```
  Hardware                                   Driver
  --------                                   ------

  1. Packet arrives on wire
       |
  2. Read descriptor at ring[HEAD]
     Check OWN bit (must be set)
       |
  3. Copy packet data into buffer
     at desc[HEAD].addr
     (writes into coherent memory)
       |
  4. Update descriptor:
     desc.status = OK | pkt_len
     desc.flags &= ~OWN
     (clear OWN = return to driver)
       |
  5. Advance HEAD:
     HEAD = (HEAD + 1) % 64
       |
  6. Set INT_STATUS.RX_PACKET
     Fire IRQ (if unmasked)
       |                                     7. ISR: read INT_STATUS
       |                                        see RX_PACKET bit set
       |                                        ack: write status back
       |                                              |
       |                                     8. vnet_rx_process():
       |                                        walk from rx_tail:
       |                                              |
       |                                        +-->  check desc[tail].flags
       |                                        |     OWN set? -> stop
       |                                        |     OWN clear? -> process:
       |                                        |       - read status & len
       |                                        |       - alloc skb
       |                                        |       - memcpy from buffer
       |                                        |       - netif_rx(skb)
       |                                        |       - re-post: flags=OWN
       |                                        |       - tail = (tail+1)%64
       |                                        +---  loop
```

### 6.2 RX Ring State Transitions

```
  INITIAL STATE (all slots owned by hardware):
  +-----+-----+-----+-----+-----+-----+-----+-----+
  | OWN | OWN | OWN | OWN | OWN | OWN | OWN | OWN | ...x64
  +-----+-----+-----+-----+-----+-----+-----+-----+
    ^
    rx_tail = 0     (HEAD = 0, both start at same position)


  AFTER 3 PACKETS RECEIVED (HW advanced HEAD to 3, cleared OWN):
  +-----+-----+-----+-----+-----+-----+-----+-----+
  | pkt | pkt | pkt | OWN | OWN | OWN | OWN | OWN | ...x64
  +-----+-----+-----+-----+-----+-----+-----+-----+
    ^                  ^
    rx_tail=0          HEAD=3
    (driver reads      (HW writes
     from here)         from here)


  AFTER DRIVER PROCESSES 3 PACKETS (re-posts with OWN):
  +-----+-----+-----+-----+-----+-----+-----+-----+
  | OWN | OWN | OWN | OWN | OWN | OWN | OWN | OWN | ...x64
  +-----+-----+-----+-----+-----+-----+-----+-----+
                       ^
                       rx_tail=3, HEAD=3  (caught up, ring empty)


  WRAP-AROUND (HEAD wraps past end of ring):
  +-----+-----+-----+-----+-----+-----+-----+-----+
  | pkt | pkt | OWN | OWN | OWN | OWN | pkt | pkt |
  +-----+-----+-----+-----+-----+-----+-----+-----+
               ^                         ^
               HEAD=2                    rx_tail=6
               (HW writes next)          (driver reads next)
```

---

## 7. TX Registers (Simple Register-Based Mode)

For Part 4, the TX path does **NOT** use a descriptor ring. Instead, the driver
writes the DMA address and packet length directly to TX registers and rings
a doorbell. This is the simplest possible DMA TX model -- one packet at a time.

### 7.1 TX_RING_ADDR (offset 0x100)

In simple TX mode, this holds the **DMA address of the packet data** (not a
descriptor ring). The driver writes the address returned by `dma_map_single()`
here before ringing the doorbell.

### 7.2 TX_RING_SIZE (offset 0x104)

In simple TX mode, this holds the **packet length** in bytes. The hardware
reads this many bytes from the address in TX_RING_ADDR.

### 7.3 TX_RING_HEAD (offset 0x108)

**Doorbell register**. Writing a non-zero value tells the hardware to
transmit the packet described by TX_RING_ADDR and TX_RING_SIZE.

- Write 0: no packet pending
- Write 1: transmit the packet (doorbell)

The hardware resets this to 0 after completing the transmission.

### 7.4 TX_RING_TAIL (offset 0x10C)

Not used in simple TX mode. Reserved for ring-based TX.

---

## 8. TX Simple Operation

### 8.1 TX Data Flow

```
  Driver (vnet_xmit)                     Hardware
  ------------------                     --------

  1. dma_map_single(skb->data,
       skb->len, DMA_TO_DEVICE)
     --> dma_addr
           |
  2. Save skb, dma_addr, len
     (for completion handler)
           |
  3. iowrite32(dma_addr,
       regs + TX_RING_ADDR)
           |
  4. iowrite32(skb->len,
       regs + TX_RING_SIZE)
           |
  5. netif_stop_queue()
     (only 1 TX slot)
           |
  6. iowrite32(1,                        7. Detect doorbell
       regs + TX_RING_HEAD)                 (TX_RING_HEAD != 0)
     [doorbell]                                   |
                                         8. Read len bytes from
                                            DMA addr
                                                  |
                                         9. Update TX stats
                                                  |
                                        10. Reset TX_RING_HEAD = 0
                                                  |
                                        11. Fire TX_COMPLETE IRQ
                                                  |
  12. ISR: vnet_tx_complete()  <---------+
       |
  13. dma_unmap_single(dma_addr,
       len, DMA_TO_DEVICE)
       |
  14. dev_kfree_skb_irq(skb)
       |
  15. tx_busy = false
       |
  16. netif_wake_queue()
     (accept next packet)
```

### 8.2 TX Flow Control (Single Slot)

```
  netif_start_queue()             (open: ready to accept packets)
         |
         v
  +---> xmit() called with skb
  |      |
  |      v
  |  dma_map_single()
  |  write TX_RING_ADDR
  |  write TX_RING_SIZE
  |  netif_stop_queue() <-----   BLOCKED: only 1 slot, can't send more
  |  write doorbell (HEAD=1)
  |      |
  |  [waiting for hardware...]
  |      |
  |  TX_COMPLETE interrupt
  |      |
  |  vnet_tx_complete():
  |      dma_unmap_single()
  |      dev_kfree_skb_irq()
  |      netif_wake_queue() ---> UNBLOCKED: ready for next packet
  |      |
  +------+  (loop: stack sends next packet)
```

### 8.3 DMA Ownership Timeline (TX)

```
  Time --->

  CPU writes        CPU maps DMA      HW transmits       CPU unmaps       CPU frees
  skb->data         (cache clean)     (reads buffer)     DMA              skb
  |                 |                 |                  |                |
  v                 v                 v                  v                v
  [CPU owns]  -->  [HW owns buffer]  -->  [TX_COMPLETE]  -->  [CPU owns]  -->  done
              map                    IRQ              unmap            kfree
```

---

## 9. Descriptor Format

Each descriptor is 16 bytes (4 x 32-bit words). Used for the RX ring.
(TX in Part 4 does not use descriptors.)

```
  Byte Offset    Field
  +-----------+------------------------------------------+
  |  0x00     |  addr   -- Buffer DMA address (32-bit)   |
  +-----------+------------------------------------------+
  |  0x04     |  len    -- Buffer length in bytes         |
  +-----------+------------------------------------------+
  |  0x08     |  flags  -- Descriptor flags               |
  +-----------+------------------------------------------+
  |  0x0C     |  status -- Completion status (HW sets)    |
  +-----------+------------------------------------------+

  sizeof(vnet_hw_desc) = 16 bytes
```

### 9.1 Flags Field

```
  31   30   29   28                                    0
  +----+----+----+--------------------------------------+
  |OWN |SOP |EOP |            Reserved (0)              |
  +----+----+----+--------------------------------------+
    |    |    |
    |    |    +-- End of Packet
    |    +------- Start of Packet
    +------------ Ownership: 1 = hardware owns, 0 = driver owns
```

**OWN bit protocol for RX:**

```
  Driver sets OWN=1        Hardware clears OWN=0       Driver sets OWN=1
  (give buffer to HW)      (wrote packet into buf)     (re-post for reuse)
       |                         |                          |
       v                         v                          v
  [HW owns slot] ---------> [driver owns slot] --------> [HW owns slot]
  HW can write packet       driver reads packet           ready for next
  at any time               copies to skb, netif_rx       packet
```

### 9.2 Status Field

Written by hardware upon completion.

```
  31                        16 15                     1   0
  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
  |     Reserved (0)         |      Packet Length       |OK|
  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
                                                         |
                               STATUS_LEN_MASK (15:0) ---+-- STATUS_OK (bit 0)
```

| Bits  | Name              | Description                            |
|-------|-------------------|----------------------------------------|
| 0     | STATUS_OK         | Operation completed successfully        |
| 1     | STATUS_CSUM_OK    | Checksum verified OK (RX)              |
| 2     | STATUS_CSUM_ERR   | Checksum error detected (RX)           |
| 15:0  | STATUS_LEN_MASK   | Actual packet length (lower 16 bits)   |

---

## 10. Control Register Bits (offset 0x000)

```
  31                              8 7  6  5  4  3  2  1  0
  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
  |        Reserved (0)           |TX|RX|LB|RS|  Rsvd |RE|EN|
  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
                                   |  |  |  |           |  |
                                   |  |  |  |           |  +-- ENABLE (bit 0)
                                   |  |  |  |           +----- RING_ENABLE (bit 1)
                                   |  |  |  +----------------- RESET (bit 4)
                                   |  |  +-------------------- LOOPBACK (bit 5)
                                   |  +----------------------- RX_ENABLE (bit 6)
                                   +-------------------------- TX_ENABLE (bit 7)
```

Relevant bits for Part 4:

| Bit | Name          | Description                              |
|-----|---------------|------------------------------------------|
| 0   | ENABLE        | Global controller enable                 |
| 1   | RING_ENABLE   | Enable descriptor ring processing        |
| 6   | RX_ENABLE     | Enable receive path                      |
| 7   | TX_ENABLE     | Enable transmit path                     |

The driver writes `ENABLE | TX_ENABLE | RX_ENABLE | RING_ENABLE` in open()
and 0 in stop().

---

## 11. DMA Addressing & Bus Mastering

### 11.1 DMA Mask

```c
dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32))
```

Called in probe before any DMA allocation or mapping. Declares that this
device can access 32 bits of address space (4 GB). Sets both streaming
and coherent masks.

### 11.2 Bus Mastering

```c
pci_set_master(pdev)
```

Called in probe to enable the Bus Master bit in the PCI command register.
Without this, the device cannot initiate DMA transfers on the PCI bus.

```
  PCI Command Register (config space 0x04):
  +----+----+----+----+----+----+----+----+
  |... |... |... |... | BM |MEM | IO |... |
  +----+----+----+----+----+----+----+----+
                        ^
                        Bus Master bit
                        Must be 1 for DMA
```

---

## 12. Driver Lifecycle (Part 4)

```
  insmod vnet_irq_dma.ko
       |
       v
  vnet_probe(pdev)
       |
       +-- pci_enable_device(pdev)
       +-- pci_set_master(pdev)           // enable bus mastering
       +-- dma_set_mask_and_coherent()    // 32-bit DMA
       +-- alloc_netdev(sizeof(vnet_priv))
       +-- vnet_hw_map_bar0(pdev)         // map register file
       +-- request_irq(pdev->irq, vnet_isr, IRQF_SHARED)
       +-- vnet_irq_disable()             // mask until open()
       +-- register_netdev()
       |
  [device registered, interface DOWN]
       |
  ip link set vnet0 up
       |
       v
  vnet_open()
       |
       +-- vnet_rx_alloc_ring()           // coherent DMA for RX
       +-- init TX state (no alloc)       // simple register-based TX
       +-- vnet_hw_set_rx_ring()          // register with platform
       +-- vnet_hw_start_simple_tx()      // start simple TX engine
       +-- program RX ring registers
       +-- vnet_irq_enable()
       +-- iowrite32(ENABLE|TX|RX|RING, CTRL)
       +-- netif_start_queue()
       |
  [interface UP, packets flowing]
       |
  ip link set vnet0 down
       |
       v
  vnet_stop()
       |
       +-- netif_stop_queue()
       +-- vnet_irq_disable()
       +-- iowrite32(0, CTRL)             // disable hardware
       +-- vnet_hw_clear_rx_ring()        // stop platform RX sim
       +-- vnet_hw_stop_simple_tx()       // stop platform TX engine
       +-- unmap in-flight TX (if any)
       +-- vnet_rx_free_ring()            // free all coherent DMA
       |
  [interface DOWN]
       |
  rmmod vnet_irq_dma
       |
       v
  vnet_remove(pdev)
       |
       +-- unregister_netdev()
       +-- free_irq(pdev->irq)
       +-- vnet_hw_unmap_bar0()
       +-- free_netdev()
       +-- pci_clear_master()
       +-- pci_disable_device()
```

---

## 13. Register Summary Table

| Offset | Name            | R/W | Description                          |
|--------|-----------------|-----|--------------------------------------|
| 0x000  | CTRL            | RW  | Controller control                   |
| 0x004  | STATUS          | RO  | Controller status (link, active)     |
| 0x008  | INT_MASK        | RW  | Interrupt mask (1=disabled)          |
| 0x00C  | INT_STATUS      | RW  | Interrupt status (W1C)               |
| 0x010  | MAC_ADDR_LOW    | RW  | MAC address bytes 0-3                |
| 0x014  | MAC_ADDR_HIGH   | RW  | MAC address bytes 4-5                |
| 0x100  | TX_RING_ADDR    | RW  | TX packet DMA address (simple mode)  |
| 0x104  | TX_RING_SIZE    | RW  | TX packet length (simple mode)       |
| 0x108  | TX_RING_HEAD    | RW  | TX doorbell (write 1 to transmit)    |
| 0x10C  | TX_RING_TAIL    | RO  | Reserved (simple mode)               |
| 0x140  | RX_RING_ADDR    | RW  | RX descriptor ring DMA address       |
| 0x144  | RX_RING_SIZE    | RW  | RX ring descriptor count             |
| 0x148  | RX_RING_HEAD    | RO  | RX ring hardware write pointer       |
| 0x14C  | RX_RING_TAIL    | RW  | RX ring software read pointer        |

---

## 14. Platform Interface (Part 4)

| Function | Purpose |
|----------|---------|
| `vnet_hw_map_bar0(pdev)` | Map BAR0 register file |
| `vnet_hw_unmap_bar0(pdev)` | Unmap register file |
| `vnet_hw_set_rx_ring(pdev, descs, count, bufs, buf_size)` | Register RX ring with platform simulator |
| `vnet_hw_clear_rx_ring(pdev)` | Unregister RX ring |
| `vnet_hw_start_simple_tx(pdev)` | Enable simple (register-based) TX completion engine |
| `vnet_hw_stop_simple_tx(pdev)` | Disable simple TX completion engine |

Note: Part 4 does **not** call `vnet_hw_set_tx_ring()` or
`vnet_hw_clear_tx_ring()` -- those are for ring-based TX.

---

*Document Number: VNET-DS-IRQ-DMA-001*
*Part 4: Interrupts, RX Ring & Simple TX DMA*
