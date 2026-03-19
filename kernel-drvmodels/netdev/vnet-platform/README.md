<!-- SPDX-License-Identifier: GPL-2.0 -->
<!-- Copyright (c) 2024 TECH VEDA -->
<!-- Author: Raghu Bharadwaj -->

# Part 0: Virtual PCI Hardware Platform

This module creates the virtual PCI device that all driver parts (2-6+) bind to.
It must be loaded first before any driver module.

```bash
sudo insmod vnet_hw_platform.ko    # creates virtual PCI device
sudo insmod ../part2/vnet_skeleton.ko   # driver binds via PCI ID match
```

## What It Does

`vnet_hw_platform.ko` creates a virtual PCI host bridge with:

- **PCI config space** -- VID `0x1234`, DID `0x5678`, class Ethernet (0x0200)
- **Register file** -- 256 x 32-bit registers accessible via `ioread32`/`iowrite32`
- **Software IRQ** -- allocated via `irq_alloc_descs`, fired via `generic_handle_irq`
- **TX completion engine** -- polls descriptors and fires `VNET_INT_TX_COMPLETE` (ring-based and simple register-based modes)
- **RX simulation engine** -- generates synthetic packets into driver's RX buffers and fires `VNET_INT_RX_PACKET`

The PCI core discovers this device during bus scanning. Driver modules match it
via their PCI ID table and bind through standard `probe()`/`remove()`.

## Hardware Summary

```
Type:           Ethernet controller (PCI, VID 0x1234, DID 0x5678)
Speed:          1 Gbps
Features:       TX/RX checksum, NAPI, ring buffers
Registers:      Control, Status, Interrupt, TX/RX Ring, Statistics
Interrupts:     TX complete, RX packet, link change, error
Access:         ioread32 / iowrite32 via vnet_hw_map_bar0()
```

## Key Interface

Drivers include `vnet_hw_interface.h` for register offsets, descriptor format,
and platform function declarations:

```c
/* Register access (equivalent to pci_iomap + ioread32/iowrite32) */
void __iomem *regs = vnet_hw_map_bar0(pdev);
u32 status = ioread32(regs + VNET_STATUS);
iowrite32(VNET_CTRL_ENABLE, regs + VNET_CTRL);

/* TX ring setup (tells hardware where descriptors live -- Part 4+) */
vnet_hw_set_tx_ring(pdev, ring_va, count);
vnet_hw_clear_tx_ring(pdev);

/* Simple register-based TX (Part 4 -- no descriptor ring) */
vnet_hw_start_simple_tx(pdev);
vnet_hw_stop_simple_tx(pdev);

/* RX ring setup (Part 4+ -- registers ring and buffer VAs) */
vnet_hw_set_rx_ring(pdev, ring_va, count, bufs_va, buf_size);
vnet_hw_clear_rx_ring(pdev);
```

## Files

| File | Purpose |
|------|---------|
| `vnet_hw_platform.c` | Platform module source (virtual PCI host bridge) |
| `vnet_hw_interface.h` | Public hardware interface (registers, descriptors, constants) |
| `vnet_hw_sim.h` | Legacy simulator (unused in PCI track, used by platform track) |
| `VNET-Controller-Datasheet.md` | Complete hardware datasheet |

## Where to Start

1. Read `VNET-Controller-Datasheet.md` -- this is your hardware specification
2. Read `vnet_hw_interface.h` -- these are the C definitions you'll use

