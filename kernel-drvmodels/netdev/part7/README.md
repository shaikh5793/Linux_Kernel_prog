<!-- SPDX-License-Identifier: GPL-2.0 -->
<!-- Copyright (c) 2024 TECH VEDA -->
<!-- Author: Raghu Bharadwaj -->

# Part 7: ethtool Integration

Adds `ethtool_ops` so userspace tools can query driver info, link settings,
custom statistics, and register dumps. Builds on Part 6 (NAPI + ring buffers).

**Source**: `vnet_ethtool.c` | **Module**: `vnet_ethtool.ko`

**Datasheet**: `VNET-Controller-ethtool-Datasheet.md`

## What Part 7 Adds Over Part 6

- `ethtool_ops` structure registered via `ndev->ethtool_ops` in probe
- Custom statistics with enum, string table, and counters
- `napi_polls` and `tx_ring_full_count` fields in `struct vnet_priv`

## ethtool Callbacks

| Callback | Purpose |
|---|---|
| `get_drvinfo` | Driver name, version (`6.0.0`), bus info via `pci_name(priv->pdev)` |
| `get_link` | Reads `VNET_STATUS` register via `ioread32`, checks `VNET_STATUS_LINK_UP` |
| `get_link_ksettings` | Reports 1000baseT_Full, Autoneg, PORT_TP |
| `set_link_ksettings` | Accepts only SPEED_1000 / DUPLEX_FULL |
| `get_strings` | Returns names for 10 custom statistics |
| `get_sset_count` | Returns `VNET_STAT_COUNT` (10) for `ETH_SS_STATS` |
| `get_ethtool_stats` | Fills stat array from `priv->stats` plus NAPI/ring counters |
| `get_regs_len` | Returns `64 * sizeof(u32)` (64 registers) |
| `get_regs` | Reads 64 registers via `ioread32(priv->regs + i * 4)` |

## Custom Statistics

```
tx_packets, tx_bytes, tx_errors, tx_dropped,
rx_packets, rx_bytes, rx_errors, rx_dropped,
napi_polls, tx_ring_full
```

The first 8 come from `priv->stats` (standard `net_device_stats`). The last 2
(`napi_polls`, `tx_ring_full_count`) are dedicated `u64` fields in `struct vnet_priv`.

## What Changed from Part 6

| Part 6 (NAPI) | Part 7 (ethtool) |
|---------------|-----------------|
| No ethtool support | Full `ethtool_ops` |
| Basic stats via `get_stats64` | Custom stats: `napi_polls`, `tx_ring_full` |
| No register visibility | Raw register dump via `ethtool -d` |

## ethtool Commands

```bash
ethtool -i vnet0          # driver info (name, version, PCI bus info)
ethtool vnet0             # link settings (speed, duplex, autoneg)
ethtool -S vnet0          # custom statistics (10 counters)
ethtool -d vnet0          # register dump (64 registers via ioread32)
```

## Build and Run

```bash
make
sudo insmod ../vnet-platform/vnet_hw_platform.ko
sudo insmod vnet_ethtool.ko
sudo ip link set vnet0 up
ethtool -i vnet0
ethtool -S vnet0
ethtool -d vnet0
sudo rmmod vnet_ethtool
sudo rmmod vnet_hw_platform
```

Or use `run-demo.sh` for a guided walkthrough.

## Files

| File | Description |
|------|-------------|
| `vnet_ethtool.c` | Driver source |
| `Makefile` | Kernel module build |
| `run-demo.sh` | Demo script |
| `VNET-Controller-ethtool-Datasheet.md` | Hardware datasheet for this part |

## Next

Part 8: PHY & MDIO Bus -- connect to a virtual PHY via `phy_connect()` and
drive link state through phylib.
