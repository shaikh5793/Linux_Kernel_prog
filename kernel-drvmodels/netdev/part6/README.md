<!-- SPDX-License-Identifier: GPL-2.0 -->
<!-- Copyright (c) 2024 TECH VEDA -->
<!-- Author: Raghu Bharadwaj -->

# Part 6: NAPI Polling

Refactors Part 5's interrupt-driven RX processing into NAPI polling. The TX
ring, RX ring allocation, and all other infrastructure are unchanged -- the
**only** difference is how received packets are processed.

**Source**: `vnet_napi.c` | **Module**: `vnet_napi.ko`

**Datasheet**: `VNET-Controller-NAPI-Datasheet.md`

## The Problem NAPI Solves

Part 5 calls `vnet_rx_process()` directly in the ISR. Every received packet
triggers a hardware interrupt, and each interrupt has fixed overhead: save
registers, enter interrupt context, run ISR, restore registers, exit. At low
packet rates this is fine. Under load (thousands of packets/sec), the CPU
spends all its time bouncing in and out of interrupt context -- a condition
called **interrupt livelock**.

NAPI converts per-packet interrupts into batched polling:

1. The **first** RX interrupt fires and the ISR disables further RX interrupts.
2. The ISR schedules a NAPI poll via `__napi_schedule()`.
3. The kernel calls the poll function in softirq context with a **budget**.
4. The poll function processes packets from the RX ring (up to budget).
5. When the ring is drained (`work_done < budget`), the driver calls
   `napi_complete_done()` and re-enables RX interrupts.
6. The next arriving packet triggers a new interrupt and the cycle repeats.

## Before vs After: ISR Comparison

### Part 5 (interrupt-driven RX)

```c
if (status & VNET_INT_RX_PACKET)
    vnet_rx_process(priv);          /* process ALL packets inline */
```

Every packet fires an interrupt. Under load, N packets = N interrupts.

### Part 6 (NAPI polling)

```c
if (status & VNET_INT_RX_PACKET) {
    if (napi_schedule_prep(&priv->napi)) {
        vnet_disable_irqs(priv->regs, VNET_INT_RX_PACKET);
        __napi_schedule(&priv->napi);
    }
}
```

First packet fires one interrupt. Remaining packets are processed in the poll
function without additional interrupts. Under load, N packets = 1 interrupt.

## NAPI Flow

```
RX packet arrives
  -> Hardware sets VNET_INT_RX_PACKET in INT_STATUS
  -> generic_handle_irq fires vnet_interrupt()
  -> ISR disables RX interrupt (sets bit in VNET_INT_MASK)
  -> ISR calls __napi_schedule()
  -> Kernel calls vnet_napi_poll() in softirq context
  -> Poll processes up to budget packets from RX ring
  -> Uses netif_receive_skb() (not netif_rx())
  -> If work_done < budget:
       napi_complete_done() + re-enable RX interrupt
  -> If work_done == budget:
       Return budget; kernel calls poll again
```

## Exact Diff from Part 5

Only 6 changes from `part4/vnet_ring.c`:

| # | What Changed | Where |
|---|-------------|-------|
| 1 | Add `struct napi_struct napi` | `struct vnet_priv` |
| 2 | `netif_napi_add()` + `napi_enable()` | `vnet_open()`, before enabling IRQs |
| 3 | `napi_disable()` + `netif_napi_del()` | `vnet_stop()`, after disabling IRQs |
| 4 | ISR RX handling: schedule NAPI instead of `vnet_rx_process()` | `vnet_interrupt()` |
| 5 | New `vnet_napi_poll()` function | Replaces `vnet_rx_process()` |
| 6 | `netif_receive_skb()` replaces `netif_rx()` | Inside poll function |

Everything else is identical: TX ring, RX ring allocation, xmit, probe/remove,
ndo_ops, platform calls.

## Key Functions

### `vnet_napi_poll(napi, budget)`

Same RX ring walk as Part 5's `vnet_rx_process()`, but:
- Processes at most `budget` packets (not the whole ring)
- Uses `netif_receive_skb(skb)` instead of `netif_rx(skb)`
- Calls `napi_complete_done(napi, work_done)` when done
- Re-enables RX interrupts after completing NAPI

### `vnet_interrupt(irq, data)`

TX completion is handled inline (unchanged). RX triggers NAPI:
```c
if (napi_schedule_prep(&priv->napi)) {
    vnet_disable_irqs(priv->regs, VNET_INT_RX_PACKET);
    __napi_schedule(&priv->napi);
}
```

## Build and Run

```bash
make
sudo insmod ../vnet-platform/vnet_hw_platform.ko
sudo insmod vnet_napi.ko
sudo ip link set vnet0 up && sudo ip addr add 10.99.0.1/24 dev vnet0
ping -c 5 10.99.0.1
sudo rmmod vnet_napi
sudo rmmod vnet_hw_platform
```

## Files

| File | Description |
|------|-------------|
| `vnet_napi.c` | Driver source (NAPI refactor of Part 5) |
| `Makefile` | Kernel module build |
| `run-demo.sh` | Demo script with interrupt monitoring |
| `VNET-Controller-NAPI-Datasheet.md` | INT_MASK register and NAPI coalescing |

## Next

Part 7 adds ethtool support for statistics, link settings, and register dumps.
