<!-- SPDX-License-Identifier: GPL-2.0 -->
<!-- Copyright (c) 2024 TECH VEDA -->
<!-- Author: Raghu Bharadwaj -->

# Part 9: Multi-Queue TX/RX

## Overview

Build a multi-queue network driver with 4 TX and 4 RX queues, per-queue NAPI
instances, and queue selection logic. This eliminates the single-queue bottleneck
from Part 8 and enables parallel packet processing across multiple CPUs.

## New Concepts (over Part 8)

| Concept | Purpose |
|---------|---------|
| `alloc_netdev_mqs()` | Allocate netdev with multiple TX and RX queues |
| Per-queue ring buffers | Independent TX/RX descriptor rings per queue |
| Per-queue NAPI | Separate `napi_struct` and poll function per queue |
| XPS (Transmit Packet Steering) | CPU-to-TX-queue affinity mapping |
| RPS (Receive Packet Steering) | Software RX queue distribution |
| `ndo_select_queue` | Driver-level TX queue selection override |

## Source File

- `vnet_multiqueue.c` -- multi-queue PCI network driver

## Datasheet

- `VNET-Controller-MultiQueue-Datasheet.md` -- register map and architecture

## Build and Load

```bash
make
insmod ../vnet-platform/vnet_hw_platform.ko
insmod vnet_multiqueue.ko
```

Unload in reverse order:

```bash
rmmod vnet_multiqueue
rmmod vnet_hw_platform
```

## Key API Calls

```c
alloc_netdev_mqs(sizeof_priv, "vnet%d", NET_NAME_USER, ether_setup, 4, 4);
netif_set_real_num_tx_queues(ndev, 4);
netif_set_real_num_rx_queues(ndev, 4);
netdev_get_tx_queue(ndev, queue_idx);
netif_tx_start_queue(txq);
netif_tx_stop_queue(txq);
netif_tx_wake_queue(txq);
netif_napi_add(ndev, &priv->napi[i], vnet_poll_queue);
napi_schedule(&priv->napi[queue_idx]);      /* per-queue NAPI schedule */
skb_get_queue_mapping(skb);                 /* which TX queue owns this skb */
skb_rx_queue_recorded(skb);                 /* check if RX queue is recorded */
skb_record_rx_queue(skb, queue_idx);        /* tag skb with its RX queue */
```

## What to Observe

```bash
# Verify multiple queues are created
ls /sys/class/net/vnet0/queues/
# Expected: rx-0  rx-1  rx-2  rx-3  tx-0  tx-1  tx-2  tx-3

# Check XPS CPU mapping
cat /sys/class/net/vnet0/queues/tx-0/xps_cpus

# Check RPS CPU mapping
cat /sys/class/net/vnet0/queues/rx-0/rps_cpus

# Monitor per-queue statistics
ethtool -S vnet0

# Watch NAPI scheduling across CPUs
cat /proc/interrupts | grep vnet
```
