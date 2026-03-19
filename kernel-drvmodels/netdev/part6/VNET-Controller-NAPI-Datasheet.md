<!-- SPDX-License-Identifier: GPL-2.0 -->
<!-- Copyright (c) 2024 TECH VEDA -->
<!-- Author: Raghu Bharadwaj -->

# VNET Virtual Ethernet Controller - NAPI Polling
## Hardware Datasheet for Part 6: NAPI Polling

---

## 1. Overview

Part 6 does not add new hardware registers. All registers are the same as
Part 5. The only change is how the **driver** uses the INT_MASK register
(0x008) to implement NAPI interrupt coalescing.

This datasheet focuses on the INT_MASK register and the mask/unmask pattern
that makes NAPI work. For the full register set (TX ring, RX ring, descriptors,
control, status), refer to Part 5's `VNET-Controller-RingBuffer-Datasheet.md`.

---

## 2. The Problem: Per-Packet Interrupts

In Part 5, every received packet causes a hardware interrupt:

```
  HW RX:    PKT1      PKT2      PKT3      PKT4      PKT5
             |          |          |          |          |
  IRQ:      -|IRQ|-    -|IRQ|-    -|IRQ|-    -|IRQ|-    -|IRQ|-
             |          |          |          |          |
  CPU:    +-----+    +-----+    +-----+    +-----+    +-----+
          | ISR |    | ISR |    | ISR |    | ISR |    | ISR |
          | +RX |    | +RX |    | +RX |    | +RX |    | +RX |
          +-----+    +-----+    +-----+    +-----+    +-----+

  Interrupts: 5    Context switches: 5    Overhead: HIGH
```

At high packet rates, the CPU spends all its time entering and exiting
interrupt context. This is called **interrupt livelock**.

---

## 3. The Solution: NAPI Interrupt Coalescing

NAPI uses the INT_MASK register to disable RX interrupts after the first
one fires, then processes multiple packets in a single softirq pass:

```
  HW RX:    PKT1  PKT2  PKT3  PKT4  PKT5
             |
  IRQ:      -|IRQ|-   (RX IRQ masked)            (unmasked)
             |                                       |
  CPU:    +--+---------------------------------------+--+
          | ISR: mask RX IRQ, schedule NAPI          |  |
          +------------------------------------------+  |
          | NAPI poll:                               |  |
          |   process PKT1                           |  |
          |   process PKT2                           |  |
          |   process PKT3                           |  |
          |   process PKT4                           |  |
          |   process PKT5                           |  |
          |   work(5) < budget(64)                   |  |
          |   napi_complete_done()                   |  |
          |   unmask RX IRQ -----------------------> |  |
          +------------------------------------------+

  Interrupts: 1    Context switches: 1    Overhead: LOW
```

---

## 4. INT_MASK Register (0x008)

The INT_MASK register controls which interrupt sources can fire. A bit
set to **1** means the corresponding interrupt is **masked** (disabled).
A bit set to **0** means the interrupt is **enabled**.

```
  Bit 31:4  - Reserved
  Bit 3     - TX_COMPLETE mask   (1 = masked, 0 = enabled)
  Bit 2     - RX_PACKET mask     (1 = masked, 0 = enabled)
  Bit 1     - LINK_CHANGE mask   (1 = masked, 0 = enabled)
  Bit 0     - ERROR mask         (1 = masked, 0 = enabled)
```

### 4.1 Mask/Unmask Helper Functions

The driver uses two helpers to manipulate INT_MASK:

```c
/* Enable interrupts: clear mask bits (0 = enabled) */
static void vnet_enable_irqs(void __iomem *regs, u32 bits)
{
    iowrite32(ioread32(regs + VNET_INT_MASK) & ~bits,
              regs + VNET_INT_MASK);
}

/* Disable interrupts: set mask bits (1 = masked) */
static void vnet_disable_irqs(void __iomem *regs, u32 bits)
{
    iowrite32(ioread32(regs + VNET_INT_MASK) | bits,
              regs + VNET_INT_MASK);
}
```

### 4.2 NAPI Mask/Unmask Pattern

The NAPI coalescing pattern uses INT_MASK as follows:

**In the ISR** (when RX_PACKET fires):
```c
/* Mask RX interrupts -- no more RX IRQs until poll completes */
vnet_disable_irqs(priv->regs, VNET_INT_RX_PACKET);
/* Schedule NAPI poll */
__napi_schedule(&priv->napi);
```

This sets bit 2 in INT_MASK, preventing further RX interrupts.

**In the poll function** (when work_done < budget):
```c
/* Tell NAPI we're done */
napi_complete_done(napi, work_done);
/* Unmask RX interrupts -- ready for the next packet */
vnet_enable_irqs(priv->regs, VNET_INT_RX_PACKET);
```

This clears bit 2 in INT_MASK, allowing the next RX packet to fire an
interrupt and start a new NAPI cycle.

### 4.3 Why TX Interrupts Are Not Masked

TX_COMPLETE (bit 3) is never masked by NAPI. TX completion runs directly
in the ISR because it only frees resources (unmap DMA, free skb) and is
fast. There is no benefit to deferring it to a poll function.

---

## 5. NAPI State Machine

```
                     +------------------+
                     |  IDLE (IRQ mode) |
                     |  RX IRQ enabled  |
                     |  INT_MASK bit2=0 |
                     +--------+---------+
                              |
                      RX packet arrives,
                      HW fires interrupt
                              |
                              v
                     +------------------+
                     |   ISR fires      |
                     |                  |
                     | 1. Read INT_STATUS
                     | 2. Set INT_MASK  |
                     |    bit2=1 (mask) |
                     | 3. napi_schedule |
                     +--------+---------+
                              |
                              v
                     +------------------+
                     |  NAPI POLL MODE  |
                     |                  |
                     | Poll fn called   |
                     | by softirq with  |
                     | budget (e.g. 64) |
                     +--------+---------+
                              |
                      Process packets
                      from RX ring
                              |
                              v
                   +---------------------+
                   | work_done < budget?  |
                   +---+-----------+-----+
                       |           |
                  YES  |           |  NO
                       v           v
              +--------------+  +-------------------+
              | napi_complete|  | Return budget      |
              | _done()      |  | (stay scheduled,   |
              |              |  |  poll called again) |
              | Clear INT_MASK  +-------------------+
              | bit2=0 (unmask)|
              +-------+------+
                      |
                      v
             +------------------+
             |  IDLE (IRQ mode) |
             |  Wait for next   |
             |  RX interrupt    |
             +------------------+
```

---

## 6. INT_STATUS Register (0x00C) -- Reference

```
  Bit 31:4  - Reserved
  Bit 3     - TX_COMPLETE   (VNET_INT_TX_COMPLETE = BIT(3))
                Set by HW when a TX descriptor is consumed.
  Bit 2     - RX_PACKET     (VNET_INT_RX_PACKET = BIT(2))
                Set by HW when a packet is placed in the RX ring.
  Bit 1     - LINK_CHANGE   (VNET_INT_LINK_CHANGE = BIT(1))
                Set by HW when link status changes.
  Bit 0     - ERROR         (VNET_INT_ERROR = BIT(0))
                Set by HW on error conditions.
```

Write-1-to-clear: writing a 1 to a bit clears that interrupt source.

---

## 7. Register Map Summary

All registers are inherited from Part 5. No new registers in Part 6.

| Offset | Name         | Type | Description                     |
|--------|--------------|------|---------------------------------|
| 0x000  | CTRL         | R/W  | Device control                  |
| 0x004  | STATUS       | R    | Device status                   |
| 0x008  | INT_MASK     | R/W  | Interrupt mask (**NAPI key**)   |
| 0x00C  | INT_STATUS   | R/W1C| Interrupt status                |
| 0x010  | MAC_ADDR_LOW | R/W  | MAC address bytes 0-3           |
| 0x014  | MAC_ADDR_HIGH| R/W  | MAC address bytes 4-5           |
| 0x100  | TX_RING_ADDR | R/W  | TX ring base DMA address        |
| 0x104  | TX_RING_SIZE | R/W  | TX ring descriptor count        |
| 0x108  | TX_RING_HEAD | R/W  | TX ring head index              |
| 0x10C  | TX_RING_TAIL | R    | TX ring tail index              |
| 0x140  | RX_RING_ADDR | R/W  | RX ring base DMA address        |
| 0x144  | RX_RING_SIZE | R/W  | RX ring descriptor count        |
| 0x148  | RX_RING_HEAD | R/W  | RX ring head index              |
| 0x14C  | RX_RING_TAIL | R    | RX ring tail index              |

For detailed descriptions of each register, see Part 5's datasheet:
`../part5/VNET-Controller-RingBuffer-Datasheet.md`

---

## 8. RX Ring Setup (Reference)

The RX ring is identical to Part 5 (carried forward from Part 4). For the
complete RX ring memory layout, setup sequence, and state diagrams, see:

- Part 4: `../part4/VNET-Controller-IRQ-DMA-Datasheet.md` (Sections 5-6)
- Part 5: `../part5/VNET-Controller-RingBuffer-Datasheet.md` (Section 5)

### 8.1 RX Ring in NAPI Context

The only difference from Part 5 is **where** the ring walk happens:

```
  Part 5 (interrupt-driven):          Part 6 (NAPI):
  ==========================          ==============

  Hardware fires RX_PACKET IRQ        Hardware fires RX_PACKET IRQ
         |                                   |
         v                                   v
  ISR: vnet_rx_process()              ISR: mask RX IRQ
       walk ring inline                    napi_schedule()
       netif_rx(skb)                         |
       re-post descriptors                   v
                                      Softirq: vnet_napi_poll()
                                           walk ring with budget
                                           netif_receive_skb(skb)
                                           re-post descriptors
                                           if done: napi_complete_done()
                                                    unmask RX IRQ
```

The ring structure, descriptor format, OWN protocol, and re-posting logic
are all unchanged. Only the calling context changes (hardirq -> softirq)
and the packet delivery API (`netif_rx` -> `netif_receive_skb`).

---

*Document Number: VNET-DS-NAPI-001*
*Part 6: NAPI Polling*
