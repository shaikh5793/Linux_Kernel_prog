<!-- SPDX-License-Identifier: GPL-2.0 -->
<!-- Copyright (c) 2024 TECH VEDA -->
<!-- Author: Raghu Bharadwaj -->

# Part 3: Net Device Ops & Calling Context

This is the reference map for all `net_device_ops` callbacks. Each operation
has a minimal but real implementation with detailed documentation of its
calling context -- when the kernel holds RTNL, when you are in softirq,
when you can sleep, and what the driver is expected to do.

**Source**: `vnet_basic.c` | **Module**: `vnet_basic.ko`

**Datasheet**: `VNET-Controller-Basic-Datasheet.md`

## Why This Matters

Network drivers crash most often from context violations: sleeping in
softirq, taking RTNL when already held, or accessing hardware from the
wrong context. Understanding the calling context for each op is the
foundation for writing correct drivers.

## Operations & Calling Context

| Operation | Context | RTNL? | Can Sleep? | Trigger |
|-----------|---------|-------|------------|---------|
| `ndo_open` | Process | Yes | Yes | `ip link set up` |
| `ndo_stop` | Process | Yes | Yes | `ip link set down` |
| `ndo_start_xmit` | softirq/BH, per-CPU | No | No | Stack sends a packet |
| `ndo_get_stats64` | Any (RCU, softirq, process) | No | No | `ip -s link show`, /proc/net/dev |
| `ndo_set_mac_address` | Process | Yes | Yes | `ip link set address` |
| `ndo_change_mtu` | Process | Yes | Yes | `ip link set mtu` |
| `ndo_validate_addr` | Process | Yes | Yes | Called before `ndo_open` |
| `ndo_tx_timeout` | softirq/BH (watchdog) | No | No | TX queue stuck for watchdog_timeo |
| `ndo_set_rx_mode` | Atomic (addr_lock + BH off) | No | No | Multicast/promisc change |

## Key Structures

**`struct vnet_priv`** -- private data embedded in `net_device`:

```c
struct vnet_priv {
    struct net_device      *ndev;
    struct pci_dev         *pdev;
    void __iomem           *regs;
    struct net_device_stats stats;
    unsigned long           tx_timeout_count;
};
```

## Driver Operations Summary

| Function | Role |
|----------|------|
| `vnet_probe` | PCI enable, alloc_netdev, map BAR0, set MTU limits, register_netdev |
| `vnet_remove` | unregister_netdev, unmap BAR0, free_netdev, PCI disable |
| `vnet_open` | Enable CTRL register (TX+RX), check link, start queue |
| `vnet_stop` | Stop queue, clear CTRL register, carrier off |
| `vnet_xmit` | Count packet stats, free skb, return NETDEV_TX_OK |
| `vnet_get_stats64` | Copy `priv->stats` into `rtnl_link_stats64` |
| `vnet_set_mac_address` | Validate, write to MAC_ADDR_LOW/HIGH, update dev_addr |
| `vnet_change_mtu` | Compute frame size, write MAX_FRAME_REG, update ndev->mtu |
| `vnet_validate_addr` | Check is_valid_ether_addr(dev_addr) before open |
| `vnet_tx_timeout` | Log warning, increment error count, wake queue |
| `vnet_set_rx_mode` | Read flags (IFF_PROMISC, IFF_ALLMULTI), configure filters |

## What This Part Does NOT Include

- **No interrupts** -- no `request_irq()` / `free_irq()`
- **No DMA** -- no descriptor rings or DMA mapping
- **No real TX** -- packets are counted and freed

## Build and Run

```bash
make
sudo insmod ../vnet-platform/vnet_hw_platform.ko
sudo insmod vnet_basic.ko
```

## Test the Operations

```bash
# See the interface (ndo_validate_addr checked at open time)
ip link show vnet0

# Bring up (triggers ndo_open)
sudo ip link set vnet0 up

# Change MAC address (triggers ndo_set_mac_address)
sudo ip link set vnet0 address 02:de:ad:be:ef:01

# Change MTU (triggers ndo_change_mtu)
sudo ip link set vnet0 mtu 9000

# View statistics (triggers ndo_get_stats64)
ip -s link show vnet0

# Send traffic (triggers ndo_start_xmit)
sudo ip addr add 10.99.0.1/24 dev vnet0
ping -c 3 10.99.0.1

# Bring down (triggers ndo_stop)
sudo ip link set vnet0 down
```

## Cleanup

```bash
sudo rmmod vnet_basic
sudo rmmod vnet_hw_platform
```

## Files

| File | Description |
|------|-------------|
| `vnet_basic.c` | Driver source with documented ndo_ops |
| `Makefile` | Kernel module build |
| `run-demo.sh` | Demo script exercising all ops |
| `VNET-Controller-Basic-Datasheet.md` | Hardware register reference |

