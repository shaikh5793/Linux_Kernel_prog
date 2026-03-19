<!-- SPDX-License-Identifier: GPL-2.0 -->
<!-- Copyright (c) 2024 TECH VEDA -->
<!-- Author: Raghu Bharadwaj -->

# Part 8: PHY & MDIO Bus

Integrates the Linux PHY library (phylib) so that link state management is
driven by a PHY device over the MDIO bus, rather than by direct register
polling. Builds on Part 7 (ethtool).

**Source**: `vnet_phy.c` | **Module**: `vnet_phy.ko`

**Datasheet**: `VNET-Controller-PHY-Datasheet.md`

## What Part 8 Adds Over Part 7

- MDIO bus allocation and registration (`mdiobus_alloc` / `mdiobus_register`)
- MDIO read/write callbacks that access PHY registers through VNET MDIO control registers
- PHY connection via `phy_connect` with an `adjust_link` callback
- PHY-driven link state: `phy_start` / `phy_stop` replace manual carrier tracking
- `adjust_link` callback sets `netif_carrier_on` / `netif_carrier_off` based on `phydev->link`
- ethtool link settings now delegate to phylib (`phy_ethtool_get_link_ksettings`)

## Key Concepts

| Concept | Description |
|---------|-------------|
| `mdiobus_alloc` | Allocates an MDIO bus structure |
| `mdiobus_register` | Registers the bus, triggering PHY discovery at each address |
| `phy_connect` | Binds a PHY device to the net_device with an adjust_link callback |
| `phy_start` / `phy_stop` | Starts/stops the PHY state machine (autoneg, polling) |
| `adjust_link` | Driver callback fired by phylib on link state transitions |
| PHY-driven link state | PHY polls its own registers and notifies the MAC via adjust_link |

## Key API Calls

| Phase | API Call | Purpose |
|-------|----------|---------|
| probe | `mdiobus_alloc()` | Allocate MDIO bus |
| probe | `mdiobus_register(bus)` | Register bus, discover PHYs |
| probe | `phy_connect(ndev, addr, adjust_link, iface)` | Attach PHY to net_device |
| open | `phy_start(phydev)` | Start PHY state machine |
| adjust_link | `netif_carrier_on/off` | Reflect PHY link state to stack |
| stop | `phy_stop(phydev)` | Stop PHY state machine |
| remove | `phy_disconnect(phydev)` | Detach PHY from net_device |
| remove | `mdiobus_unregister(bus)` | Unregister MDIO bus |
| remove | `mdiobus_free(bus)` | Free MDIO bus |

## Build and Run

```bash
make
sudo insmod ../vnet-platform/vnet_hw_platform.ko
sudo insmod vnet_phy.ko
sudo ip link set vnet0 up
ethtool vnet0                  # PHY-driven link settings
ethtool -S vnet0               # statistics
sudo rmmod vnet_phy
sudo rmmod vnet_hw_platform
```

Or use `run-demo.sh` for a guided walkthrough.

## What to Observe

- PHY manages link state -- `adjust_link` fires automatically on link transitions
- `ethtool vnet0` shows PHY-driven settings (speed, duplex reported by phylib)
- `dmesg` shows PHY attach, link up/down messages from phylib
- No manual `ioread32(STATUS) & LINK_UP` polling in the driver

## Files

| File | Description |
|------|-------------|
| `vnet_phy.c` | Driver source |
| `Makefile` | Kernel module build |
| `run-demo.sh` | Demo script |
| `VNET-Controller-PHY-Datasheet.md` | Hardware datasheet for this part |

## Next

Part 9 continues the curriculum with additional driver features.
