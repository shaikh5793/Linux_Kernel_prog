<!-- SPDX-License-Identifier: GPL-2.0 -->
<!-- Copyright (c) 2024 TECH VEDA -->
<!-- Author: Raghu Bharadwaj -->

# VNET Virtual Ethernet Controller -- Hardware Datasheet

## 1. Overview

The VNET controller is a 1 Gbps Ethernet controller with a 32-bit register
interface, descriptor-based TX/RX ring buffers, and interrupt-driven operation.

This datasheet describes the complete hardware interface. Drivers program the
controller through `ioread32()`/`iowrite32()` on register mappings, using the
offsets and bit definitions from `vnet_hw_interface.h`.

### 1.1 System Integration

```
  +------------------------------------------------------------------+
  |                        Host System                                |
  |                                                                   |
  |   +------------------+         +------------------------------+   |
  |   |   CPU            |         |      Main Memory             |   |
  |   |                  |         |                              |   |
  |   |  ioread32()  <---+---------+--- Register File (BAR0)     |   |
  |   |  iowrite32() ----+---------+--> mapped via vnet_hw_map_   |   |
  |   |                  |         |    bar0() / pci_iomap()      |   |
  |   |  request_irq()   |         |                              |   |
  |   |       |          |         |  +-------------------------+ |   |
  |   +-------+----------+         |  | TX Descriptor Ring      | |   |
  |           |                    |  | (dma_alloc_coherent)     | |   |
  |           |   IRQ              |  +-------------------------+ |   |
  |           v                    |  | RX Descriptor Ring      | |   |
  |   +------------------+        |  | (dma_alloc_coherent)     | |   |
  |   | Interrupt        |        |  +-------------------------+ |   |
  |   | Controller       |        |  | Packet Buffers          | |   |
  |   +--------+---------+        |  | (dma_map_single)        | |   |
  |            |                   |  +-------------------------+ |   |
  |            |                   +------------------------------+   |
  |            |                                                      |
  |   +--------v---------+                                            |
  |   | PCI Bus          |                                            |
  |   +--------+---------+                                            |
  |            |                                                      |
  +------------+------------------------------------------------------+
               |
  +------------v------------------------------------------------------+
  |                     VNET Controller (VID:1234 DID:5678)           |
  |                                                                   |
  |   +-------------+  +---------------+  +------------------------+ |
  |   | PCI Config  |  | Register File |  | DMA Engines            | |
  |   | Space       |  |               |  |                        | |
  |   | - VID/DID   |  | - CTRL        |  | TX: reads descriptors, | |
  |   | - BAR0      |  | - STATUS      |  |     "transmits" packet | |
  |   | - IRQ line  |  | - INT_MASK    |  |                        | |
  |   |             |  | - INT_STATUS  |  | RX: writes packet data | |
  |   |             |  | - MAC_ADDR    |  |     into buffers       | |
  |   |             |  | - Ring regs   |  |                        | |
  |   |             |  | - Stats regs  |  |                        | |
  |   +-------------+  +-------+-------+  +------------------------+ |
  |                             |                                     |
  |                    +--------v---------+                           |
  |                    | Interrupt Logic  |                           |
  |                    | INT_STATUS &     |                           |
  |                    |   ~INT_MASK != 0 |                           |
  |                    |   => fire IRQ    |                           |
  |                    +------------------+                           |
  +-------------------------------------------------------------------+
```

### 1.2 Features

- 32-bit register file (control, status, interrupt, ring, statistics)
- TX/RX descriptor ring buffers with DMA
- Four interrupt sources (TX complete, RX packet, link change, error)
- Interrupt masking for selective enable/disable
- 48-bit MAC address
- TX/RX checksum offload capability
- NAPI-compatible interrupt model

---

## 2. Register Map

All registers are 32 bits wide, accessed at 4-byte aligned offsets.

```
  Register Address Space (BAR0)
  +--------+------------------+--------+----------------------------+
  | Offset | Name             | Access | Description                |
  +--------+------------------+--------+----------------------------+
  | 0x000  | CTRL             | R/W    | Control register           |
  | 0x004  | STATUS           | R      | Status register            |
  | 0x008  | INT_MASK         | R/W    | Interrupt mask             |
  | 0x00C  | INT_STATUS       | R/W1C  | Interrupt status           |
  | 0x010  | MAC_ADDR_LOW     | R/W    | MAC address [3:0]          |
  | 0x014  | MAC_ADDR_HIGH    | R/W    | MAC address [5:4]          |
  | 0x018  | MAX_FRAME_REG    | R/W    | Max frame size (def: 1518) |
  +--------+------------------+--------+----------------------------+
  | 0x100  | TX_RING_ADDR     | R/W    | TX ring DMA base address   |
  | 0x104  | TX_RING_SIZE     | R/W    | TX ring descriptor count   |
  | 0x108  | TX_RING_HEAD     | R/W    | TX ring head (driver fills)|
  | 0x10C  | TX_RING_TAIL     | R/W    | TX ring tail (HW completes)|
  +--------+------------------+--------+----------------------------+
  | 0x140  | RX_RING_ADDR     | R/W    | RX ring DMA base address   |
  | 0x144  | RX_RING_SIZE     | R/W    | RX ring descriptor count   |
  | 0x148  | RX_RING_HEAD     | R/W    | RX ring head               |
  | 0x14C  | RX_RING_TAIL     | R/W    | RX ring tail               |
  +--------+------------------+--------+----------------------------+
  | 0x200  | STATS_TX_PACKETS | R      | Total transmitted packets  |
  | 0x204  | STATS_TX_BYTES   | R      | Total transmitted bytes    |
  | 0x240  | STATS_RX_PACKETS | R      | Total received packets     |
  | 0x244  | STATS_RX_BYTES   | R      | Total received bytes       |
  +--------+------------------+--------+----------------------------+
```

---

## 3. Register Bit Fields

### 3.1 Control Register (CTRL -- 0x000)

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

Typical enable sequence: write `ENABLE | TX_ENABLE | RX_ENABLE | RING_ENABLE`.

### 3.2 Status Register (STATUS -- 0x004)

```
  31                              8  7  6  5  4  3  2  1  0
  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
  |        Reserved (0)           |TA|RA|LU|     Rsvd     |
  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
                                   |  |  |
                                   |  |  +-- LINK_UP (bit 5)
                                   |  +----- RX_ACTIVE (bit 6)
                                   +-------- TX_ACTIVE (bit 7)
```

Read-only. LINK_UP is set by hardware during initialization.

### 3.3 Interrupt Mask Register (INT_MASK -- 0x008)

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

To enable interrupts, CLEAR mask bits:
```c
  // Enable RX and TX interrupts
  u32 mask = ioread32(regs + VNET_INT_MASK);
  iowrite32(mask & ~(VNET_INT_RX_PACKET | VNET_INT_TX_COMPLETE),
            regs + VNET_INT_MASK);
```

To disable interrupts, SET mask bits:
```c
  // Disable RX interrupt (for NAPI)
  u32 mask = ioread32(regs + VNET_INT_MASK);
  iowrite32(mask | VNET_INT_RX_PACKET, regs + VNET_INT_MASK);
```

### 3.4 Interrupt Status Register (INT_STATUS -- 0x00C)

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

An interrupt fires when: `INT_STATUS & ~INT_MASK != 0`

The driver acknowledges by writing 0 to INT_STATUS after handling.

---

## 4. Descriptor Format

Each descriptor is 16 bytes (4 x 32-bit words):

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
```

### 4.1 Descriptor Flags

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

**TX:** Driver sets `OWN | SOP | EOP`. Hardware clears OWN on completion.
**RX:** Hardware sets OWN on filled descriptors. Driver clears OWN after processing.

### 4.2 Descriptor Status

```
  31                        16 15                     1   0
  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
  |     Reserved (0)         |      Packet Length       |OK|
  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
                                                         |
                               STATUS_LEN_MASK (15:0) ---+-- STATUS_OK (bit 0)
```

Set by hardware on completion. Driver checks STATUS_OK and reads length.

---

## 5. TX Operation

### 5.1 Ring Setup

1. Allocate a descriptor array with `dma_alloc_coherent`
2. Write the DMA address to `TX_RING_ADDR` (0x100)
3. Write the descriptor count to `TX_RING_SIZE` (0x104)
4. Head and tail start at 0

### 5.2 Transmit Flow

```
  Driver                                     Hardware
  ------                                     --------

  1. dma_map_single(skb->data)
  2. Fill desc[head]:
     addr = dma_addr
     len  = skb->len
     flags = OWN|SOP|EOP
  3. head = (head+1) % size
  4. iowrite32(head, TX_RING_HEAD)
     (doorbell write)
                                             5. TX poll engine sees
                                                head != tail
                                             6. Reads desc[tail]
                                             7. "Transmits" packet
                                             8. Clears OWN flag
                                             9. Sets status = OK|len
                                             10. tail = (tail+1) % size
                                             11. Sets INT_STATUS.TX_COMPLETE
                                             12. Fires IRQ (if unmasked)
```

### 5.3 TX Ring State Diagram

```
  Initial state (empty):
    head = 0, tail = 0

    +----+----+----+----+----+----+----+----+
    |    |    |    |    |    |    |    |    |
    +----+----+----+----+----+----+----+----+
     ^
     head, tail


  After driver submits 3 packets:
    head = 3, tail = 0

    +----+----+----+----+----+----+----+----+
    |OWN |OWN |OWN |    |    |    |    |    |
    +----+----+----+----+----+----+----+----+
     ^              ^
     tail           head
     (HW processes  (driver fills
      from here)     from here)


  After hardware completes 2:
    head = 3, tail = 2

    +----+----+----+----+----+----+----+----+
    |done|done|OWN |    |    |    |    |    |
    +----+----+----+----+----+----+----+----+
               ^    ^
               tail head

     Driver runs vnet_tx_complete():
       - unmaps DMA, frees skb for desc[0] and desc[1]
       - advances tail to 2


  Ring full (1 slot always empty as sentinel):
    head = 7, tail = 0

    +----+----+----+----+----+----+----+----+
    |    |OWN |OWN |OWN |OWN |OWN |OWN |OWN |
    +----+----+----+----+----+----+----+----+
     ^                                  ^
     tail                               head
     ((head+1)%size == tail => FULL)
     => netif_stop_queue()
```

### 5.4 TX Completion

In the interrupt handler:

1. Walk ring from tail toward head
2. For each descriptor where OWN is clear: unmap DMA, free skb
3. Advance tail
4. If ring was full and space is now available: `netif_wake_queue`

---

## 6. RX Operation

### 6.1 Ring Setup

1. Allocate a descriptor array with `dma_alloc_coherent`
2. Write the DMA address to `RX_RING_ADDR` (0x140)
3. Write the descriptor count to `RX_RING_SIZE` (0x144)
4. Pre-fill descriptors with allocated buffers, set OWN flag

### 6.2 Receive Flow (NAPI)

```
  Hardware                                   Driver
  --------                                   ------

  1. Packet arrives
  2. Copies data to buffer
     at desc[head].addr
  3. Clears OWN flag
  4. Sets status = OK | len
  5. Sets INT_STATUS.RX_PACKET
  6. Fires IRQ (if unmasked)
                                             7. ISR: disable RX interrupt
                                                mask |= VNET_INT_RX_PACKET

                                             8. ISR: schedule NAPI
                                                napi_schedule_prep()
                                                __napi_schedule()

                                             -- softirq context --

                                             9. NAPI poll: walk ring
                                                while (work < budget):
                                                  check desc[tail].flags
                                                  if OWN set: break
                                                  alloc skb, copy data
                                                  netif_receive_skb()
                                                  refill desc, set OWN
                                                  advance tail

                                             10. if work < budget:
                                                   napi_complete_done()
                                                   re-enable RX interrupt
                                                   mask &= ~VNET_INT_RX_PACKET
```

### 6.3 NAPI Interrupt Coalescing

```
  Time --->

  Without NAPI (interrupt per packet):
  ========================================
  pkt  pkt  pkt  pkt  pkt  pkt  pkt  pkt
   |    |    |    |    |    |    |    |
   v    v    v    v    v    v    v    v
  IRQ  IRQ  IRQ  IRQ  IRQ  IRQ  IRQ  IRQ    (8 context switches)


  With NAPI (interrupt + polling):
  ========================================
  pkt  pkt  pkt  pkt  pkt  pkt  pkt  pkt
   |    |    |    |    |    |    |    |
   v    |    |    |    v    |    |    |
  IRQ--+----+----+---+   IRQ--+----+---+
       |              |        |        |
       v              v        v        v
     [  NAPI poll   ]       [ NAPI poll ]   (2 context switches)
     process 4 pkts          process 4 pkts
     napi_complete           napi_complete
     re-enable IRQ           re-enable IRQ
```

---

## 7. Interrupt Model

### 7.1 Interrupt Sources

| Source | Bit | Trigger |
|--------|-----|---------|
| ERROR | 0 | Error condition |
| LINK_CHANGE | 1 | Link up/down transition |
| RX_PACKET | 2 | Packet received |
| TX_COMPLETE | 3 | TX descriptor completed |

### 7.2 Interrupt Delivery Pipeline

```
  Hardware Event
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
  | (pdev->irq)      |     (software IRQ in simulator)
  +--------+---------+     (MSI/MSI-X in real HW)
           |
           v
  +------------------+
  | Driver ISR       |     vnet_interrupt()
  | 1. Read STATUS   |     status = ioread32(INT_STATUS)
  | 2. Handle events |     if (status & TX_COMPLETE) ...
  | 3. Acknowledge   |     iowrite32(0, INT_STATUS)
  +------------------+
```

### 7.3 ISR Decision Tree

```
  vnet_interrupt(irq, priv)
       |
       v
  status = ioread32(INT_STATUS)
       |
       +---> status == 0? ---> return IRQ_NONE (not our interrupt)
       |
       +---> TX_COMPLETE? ---> vnet_tx_complete(priv)
       |                       walk ring, unmap DMA, free skbs
       |                       wake queue if was stopped
       |
       +---> RX_PACKET?  ---> vnet_rx_process(priv)
       |
       +---> LINK_CHANGE? --> read STATUS.LINK_UP
       |                       netif_carrier_on/off()
       |
       +---> ERROR?       --> stats.tx_errors++
       |                       stats.rx_errors++
       |
       v
  iowrite32(0, INT_STATUS)    // acknowledge all
  return IRQ_HANDLED
```

---

## 8. MAC Address

The MAC address is stored in two registers:

```
  MAC Address: AA:BB:CC:DD:EE:FF

  MAC_ADDR_LOW  (0x010):               MAC_ADDR_HIGH (0x014):
  31      24 23      16 15       8 7        0     15       8 7        0
  +----------+----------+----------+----------+  +----------+----------+
  |  DD (3)  |  CC (2)  |  BB (1)  |  AA (0)  |  |  FF (5)  |  EE (4)  |
  +----------+----------+----------+----------+  +----------+----------+
        = 0xDDCCBBAA                                   = 0x0000FFEE

  Programming:
    iowrite32(addr[0] | (addr[1] << 8) |
              (addr[2] << 16) | (addr[3] << 24),
              regs + VNET_MAC_ADDR_LOW);
    iowrite32(addr[4] | (addr[5] << 8),
              regs + VNET_MAC_ADDR_HIGH);
```

---

## 9. Platform Interface

The platform module (`vnet_hw_platform.ko`) exports helper functions that
bridge the gap between the virtual register file and standard PCI driver APIs:

```
  +-------------------+     +-------------------------+     +------------------+
  |  Your Driver      |     |  Platform Module        |     |  PCI Core        |
  |  (Parts 2-6)      |     |  (vnet_hw_platform.ko)  |     |  (kernel)        |
  |                   |     |                         |     |                  |
  |  vnet_hw_map_bar0 +---->+  Returns &dev->regs[0]  |     |                  |
  |  (pdev)           |     |  cast to void __iomem * |     |                  |
  |                   |     |                         |     |                  |
  |  ioread32(regs+X) +---->+  Array read: regs[X/4]  |     |                  |
  |                   |     |                         |     |                  |
  |  iowrite32(v,     +---->+  Array write: regs[X/4] |     |                  |
  |    regs+X)        |     |    = val                |     |                  |
  |                   |     |                         |     |                  |
  |  request_irq(     +---->+  Software IRQ allocated  |     |                  |
  |    pdev->irq, ..) |     |  by irq_alloc_descs()   |     |                  |
  |                   |     |                         |     |                  |
  |  pci_enable_      +-----+-------------------------+---->+  Standard PCI    |
  |    device(pdev)   |     |                         |     |  probe path      |
  +-------------------+     +-------------------------+     +------------------+
```

| Function | First Used | Purpose |
|----------|-----------|---------|
| `vnet_hw_map_bar0(pdev)` | Part 2 | Returns register base pointer for `ioread32`/`iowrite32` |
| `vnet_hw_unmap_bar0(pdev)` | Part 2 | Releases register mapping |
| `vnet_hw_set_rx_ring(pdev, descs, count, bufs, buf_size)` | Part 4 | Register RX ring and buffers with platform |
| `vnet_hw_clear_rx_ring(pdev)` | Part 4 | Stop RX packet simulation |
| `vnet_hw_start_simple_tx(pdev)` | Part 4 | Enable register-based TX completion engine |
| `vnet_hw_stop_simple_tx(pdev)` | Part 4 | Disable simple TX engine |
| `vnet_hw_set_tx_ring(pdev, va, count)` | Part 4 | Register TX descriptor ring with platform |
| `vnet_hw_clear_tx_ring(pdev)` | Part 4 | Stop ring-based TX processing |

All other hardware access uses standard `ioread32()`/`iowrite32()` on the
register mapping, with offsets defined in `vnet_hw_interface.h`.

---

## 10. Driver Lifecycle

```
  modprobe vnet_hw_platform
       |
       v
  PCI core discovers device (VID:1234, DID:5678)
       |
       v
  modprobe vnet_<part>              rmmod vnet_<part>
       |                                 |
       v                                 v
  +------------------+             +------------------+
  |  probe()         |             |  remove()        |
  |  1. pci_enable   |             |  1. unregister   |
  |  2. pci_set_     |             |     _netdev      |
  |     master       |             |  2. free_irq     |
  |  3. alloc_netdev |             |  3. unmap_bar0   |
  |  4. map_bar0     |             |  4. free_netdev  |
  |  5. request_irq  |             |  5. pci_disable  |
  |  6. register_    |             +------------------+
  |     netdev       |
  +--------+---------+
           |
           v
  ip link set vnet0 up              ip link set vnet0 down
       |                                 |
       v                                 v
  +------------------+             +------------------+
  |  open()          |             |  stop()          |
  |  1. alloc rings  |             |  1. stop queue   |
  |  2. setup HW     |             |  2. napi_disable |
  |     ring regs    |             |  3. disable IRQs |
  |  3. napi_add     |             |  4. disable ctrl |
  |  4. napi_enable  |             |  5. free rings   |
  |  5. enable IRQs  |             +------------------+
  |  6. enable ctrl  |
  |  7. start queue  |
  +------------------+
```

---

## 11. Constants

```
VNET_MAX_FRAME_SIZE_DEFAULT   1518    Maximum frame size
VNET_RING_SIZE_DEFAULT        256     Default ring size
VNET_VENDOR_ID                0x1234  PCI Vendor ID
VNET_DEVICE_ID                0x5678  PCI Device ID
```

---

## 12. Capabilities

The controller reports its capabilities:

```
Bit 0   FEAT_CSUM_TX    TX checksum offload
Bit 1   FEAT_CSUM_RX    RX checksum offload
Bit 2   FEAT_NAPI       NAPI polling support
```
