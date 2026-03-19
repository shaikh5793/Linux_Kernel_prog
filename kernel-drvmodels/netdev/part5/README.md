<!-- SPDX-License-Identifier: GPL-2.0 -->
<!-- Copyright (c) 2024 TECH VEDA -->
<!-- Author: Raghu Bharadwaj -->

# Part 5: TX Ring Buffer & Full Duplex Rings

Upgrades from Part 4's single-slot TX to a full 256-entry TX descriptor ring,
while carrying forward the RX ring and interrupt-driven RX processing. Both
directions now use descriptor rings -- a symmetric "full duplex rings" design.

**Source**: `vnet_ring.c` | **Module**: `vnet_ring.ko`

**Datasheet**: `VNET-Controller-RingBuffer-Datasheet.md`

## What's New Over Part 4 (TX Ring)

- TX descriptor ring (256 entries) via `dma_alloc_coherent` -- matches the RX ring pattern
- TX streaming DMA via `dma_map_single` per packet (mapped into ring descriptors)
- Ring head/tail management with wrap-around
- Doorbell write (`iowrite32` to TX_RING_HEAD) to notify hardware
- TX completion: walk ring from tail, check `VNET_DESC_OWN` cleared, `dma_unmap_single`, free skb
- Flow control: `netif_stop_queue` when ring full, `netif_wake_queue` on completion
- `struct vnet_ring` for ring management (head, tail, count, desc array, skb tracking, dma_addrs)
- `vnet_ring_full()` / `vnet_ring_empty()` helper functions
- `vnet_alloc_tx_ring()` / `vnet_free_tx_ring()` with coherent DMA for descriptors

## What's Carried Forward from Part 4 (RX Ring)

- RX ring (64 entries) with `dma_alloc_coherent` for descriptors and buffers
- RX processing in ISR (`vnet_rx_process`) -- walk ring, copy to skb, `netif_rx()`
- Interrupt handler dispatches TX_COMPLETE, RX_PACKET, LINK_CHANGE, and ERROR
- Platform calls: `vnet_hw_set_rx_ring()` / `vnet_hw_clear_rx_ring()`

## Data Structures

**`struct vnet_ring`** -- TX descriptor ring management:

```c
struct vnet_ring {
    struct vnet_hw_desc *desc;     /* coherent DMA descriptor array */
    dma_addr_t desc_dma;           /* bus address of descriptor array */
    struct sk_buff **skbs;         /* parallel skb tracking array */
    dma_addr_t *dma_addrs;        /* per-descriptor DMA address for unmap */
    u32 head;                      /* next descriptor to fill */
    u32 tail;                      /* next descriptor to reclaim */
    u32 count;                     /* total descriptors (256) */
};
```

**`struct vnet_priv`** contains `tx_ring` (struct vnet_ring), `tx_stopped`,
and the RX ring fields (`rx_descs`, `rx_bufs_va`, `rx_bufs_dma`, etc.).

## Ring Buffer Diagram

```
 EMPTY (head == tail):
 +-----+-----+-----+-----+-----+-----+-----+-----+
 |     |     |     |     |     |     |     |     |  ...x256
 +-----+-----+-----+-----+-----+-----+-----+-----+
   ^
   head/tail

 PARTIALLY FULL (head advanced past tail):
 +-----+-----+-----+-----+-----+-----+-----+-----+
 |     | OWN | OWN | OWN |     |     |     |     |  ...x256
 +-----+-----+-----+-----+-----+-----+-----+-----+
   ^     ^                  ^
   |     tail (HW reads)   head (driver writes)
   completed (OWN cleared)

 FULL (head + 1 == tail, modulo ring size):
 +-----+-----+-----+-----+-----+-----+-----+-----+
 | OWN | OWN | OWN | OWN | OWN | OWN | OWN |     |  ...x256
 +-----+-----+-----+-----+-----+-----+-----+-----+
                                          ^   ^
                                     head     tail
   --> netif_stop_queue() called

 WRAP-AROUND (head wraps past end of array):
 +-----+-----+-----+-----+-----+-----+-----+-----+
 | OWN | OWN |     |     |     | OWN | OWN | OWN |  ...x256
 +-----+-----+-----+-----+-----+-----+-----+-----+
               ^                  ^
               head               tail
```

One slot is always kept empty to distinguish full from empty.

## TX Flow

```
xmit(skb)
  |
  v
Ring full? --YES--> netif_stop_queue(), return NETDEV_TX_BUSY
  |NO
  v
dma_map_single(skb->data, DMA_TO_DEVICE)
  |
  v
Fill desc[head]: addr=dma_addr, len=skb->len, flags=OWN|SOP|EOP
Store skb in skbs[head], dma_addr in dma_addrs[head]
  |
  v
Advance head: head = (head + 1) % 256
  |
  v
Doorbell: iowrite32(head, TX_RING_HEAD)
  |
  v
Ring now full? --YES--> netif_stop_queue()

--- Hardware processes descriptor, clears OWN, fires TX_COMPLETE IRQ ---

ISR -> vnet_tx_complete():
  while desc[tail] has OWN cleared:
    dma_unmap_single(dma_addrs[tail])
    dev_kfree_skb_any(skbs[tail])
    advance tail
  if queue was stopped and ring has space:
    netif_wake_queue()
```

## Key Functions

| Function | Role |
|---|---|
| `vnet_alloc_tx_ring` | `dma_alloc_coherent` for descriptors + `kcalloc` for skb/addr arrays |
| `vnet_free_tx_ring` | Unmaps pending DMA, frees skbs, frees coherent DMA and arrays |
| `vnet_rx_alloc_ring` | `dma_alloc_coherent` for RX descriptors and per-slot buffers |
| `vnet_rx_free_ring` | Frees all RX coherent DMA allocations |
| `vnet_xmit` | `dma_map_single` -> fill descriptor -> advance head -> doorbell -> stop queue if full |
| `vnet_tx_complete` | Walk ring from tail, `dma_unmap_single`, free skb, wake stopped queue |
| `vnet_rx_process` | Walk RX ring, copy data to skb, `netif_rx()`, re-post descriptor |
| `vnet_open` | Allocates TX + RX rings, programs registers, enables interrupts |
| `vnet_stop` | Disables interrupts, clears rings, frees all DMA resources |

## Build and Run

```bash
make
sudo insmod ../vnet-platform/vnet_hw_platform.ko
sudo insmod vnet_ring.ko
sudo ip link set vnet0 up && sudo ip addr add 10.99.0.1/24 dev vnet0
ping -c 5 10.99.0.1
ip -s link show vnet0     # Both TX and RX stats visible
sudo rmmod vnet_ring
sudo rmmod vnet_hw_platform
```

## Files

| File | Description |
|------|-------------|
| `vnet_ring.c` | Driver source |
| `Makefile` | Kernel module build |
| `run-demo.sh` | Demo script |
| `VNET-Controller-RingBuffer-Datasheet.md` | Hardware register reference |

