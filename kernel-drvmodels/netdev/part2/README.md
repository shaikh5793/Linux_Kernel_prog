<!-- SPDX-License-Identifier: GPL-2.0 -->
<!-- Copyright (c) 2024 TECH VEDA -->
<!-- Author: Raghu Bharadwaj -->

# Part 2: PCI Driver Skeleton

The simplest possible PCI network driver. It registers with the PCI subsystem,
creates a network interface, and handles open/stop -- but does not transmit or
receive packets. That comes in Part 3.

**Source**: `vnet_skeleton.c` | **Module**: `vnet_skeleton.ko`

## Concepts Covered

- **PCI driver registration** -- `module_pci_driver()` with PCI ID table
  matching VID `0x1234` / DID `0x5678`
- **PCI lifecycle** -- `probe()` and `remove()` callbacks
- **PCI device setup** -- `pci_enable_device()`, `pci_disable_device()`
- **Register mapping** -- `vnet_hw_map_bar0()` (equivalent to `pci_iomap`)
- **Register access** -- `ioread32()` / `iowrite32()` on mapped registers
- **net_device** -- `alloc_etherdev()`, `register_netdev()`, `free_netdev()`
- **Device hierarchy** -- `SET_NETDEV_DEV(ndev, &pdev->dev)`
- **Carrier detection** -- `netif_carrier_on/off` based on `VNET_STATUS_LINK_UP`

## Key Structures

**`struct vnet_priv`** -- per-device private data via `netdev_priv()`:

```c
struct vnet_priv {
    struct net_device *ndev;
    struct pci_dev    *pdev;
    void __iomem      *regs;   /* register base from vnet_hw_map_bar0 */
};
```

## Driver Operations

| Function | Role |
|----------|------|
| `vnet_probe` | `pci_enable_device` -> `alloc_etherdev` -> `vnet_hw_map_bar0` -> `eth_hw_addr_random` -> `register_netdev` |
| `vnet_remove` | `unregister_netdev` -> `vnet_hw_unmap_bar0` -> `free_netdev` -> `pci_disable_device` |
| `vnet_open` | Writes `VNET_CTRL_ENABLE` to CTRL register, checks link, starts queue |
| `vnet_stop` | Stops queue, drops carrier, clears CTRL register |
| `vnet_xmit` | Stub -- drops packet and returns `NETDEV_TX_OK` |

## Hardware Access Pattern

All hardware access uses `ioread32`/`iowrite32` on the register mapping:

```c
/* Read status register */
u32 status = ioread32(priv->regs + VNET_STATUS);

/* Write control register */
iowrite32(VNET_CTRL_ENABLE, priv->regs + VNET_CTRL);

/* Program MAC address */
iowrite32(addr[0] | (addr[1] << 8) | ..., priv->regs + VNET_MAC_ADDR_LOW);
```

Register offsets are defined in `../vnet-platform/vnet_hw_interface.h`.

## Build and Run

```bash
make
sudo insmod ../vnet-platform/vnet_hw_platform.ko   # hardware first
sudo insmod vnet_skeleton.ko
ip link show vnet0
sudo rmmod vnet_skeleton
sudo rmmod vnet_hw_platform
```

Or use `run-demo.sh` for a guided walkthrough.

## Files

| File | Purpose |
|------|---------|
| `vnet_skeleton.c` | Driver source |
| `Makefile` | Kernel module build |
| `run-demo.sh` | Load and exercise the module |
| `VNET-Controller-Basic-Datasheet.md` | Hardware register reference |

