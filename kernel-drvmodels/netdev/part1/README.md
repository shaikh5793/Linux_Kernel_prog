<!-- SPDX-License-Identifier: GPL-2.0 -->
<!-- Copyright (c) 2024 TECH VEDA -->
<!-- Author: Raghu Bharadwaj -->

# Part 1: Userspace Network Device Discovery

## Objectives

- Enumerate network interfaces from `/sys/class/net/`
- Read per-interface statistics from sysfs and `/proc/net/dev`
- Explore TX/RX queue configuration and CPU affinity
- Use ethtool ioctl (`SIOCETHTOOL`) to query driver information

## Overview

Four userspace programs explore the network device subsystem from different
angles, building a mental model of how Linux network interfaces are organized
before writing kernel drivers in subsequent parts.

## Programs

| Program | What it does |
|---------|-------------|
| `net_enum` | Lists all interfaces: type, MAC, MTU, state, speed, queues, driver |
| `net_stats` | Shows per-interface statistics (sysfs + /proc/net/dev) |
| `net_queues` | Explores TX/RX queues, XPS/RPS CPU maps, queue lengths |
| `net_ethtool` | Queries ethtool info: driver, link, features/offloads |

## Key Sysfs Paths

| Path | Content |
|------|---------|
| `/sys/class/net/<if>/type` | Interface type (1=Ethernet, 772=Loopback) |
| `/sys/class/net/<if>/address` | MAC address |
| `/sys/class/net/<if>/mtu` | Maximum transmission unit |
| `/sys/class/net/<if>/operstate` | Link operational state |
| `/sys/class/net/<if>/speed` | Link speed in Mbps |
| `/sys/class/net/<if>/statistics/` | Per-counter stats files |
| `/sys/class/net/<if>/queues/` | TX/RX queue directories |
| `/proc/net/dev` | All interfaces, one line per interface |

## Build and Run

```bash
cd kernel-drvmodels/netdev/part1
make
./net_enum
./net_stats
./net_queues
sudo ./net_ethtool    # needs root for SIOCETHTOOL ioctl
```

## Key Takeaways

1. `/sys/class/net/` is the primary discovery path for network interfaces
2. Each interface has type, MAC, MTU, state, and statistics in sysfs
3. Multi-queue NICs have `queues/tx-N` and `queues/rx-N` subdirectories
4. `SIOCETHTOOL` ioctl provides driver info not available in sysfs
5. `/proc/net/dev` gives a compact all-in-one stats view

## Hardware Notes

- **RPi5**: `eth0` (BCM54213PE Gigabit, driver: bcmgenet)
- **BBB**: `eth0` (TI CPSW, driver: cpsw, 100Mbps)
- **BeagleAI**: `eth0` (TI CPSW, driver: am65-cpsw-nuss, Gigabit)

