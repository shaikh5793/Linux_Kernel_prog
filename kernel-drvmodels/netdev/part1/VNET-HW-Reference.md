<!-- SPDX-License-Identifier: GPL-2.0 -->
<!-- Copyright (c) 2024 TECH VEDA -->
<!-- Author: Raghu Bharadwaj -->

# VNET Hardware Reference — Part 1: Network Subsystem Map

## 1. Linux Network Stack Layers

```
 ┌─────────────────────────────────────────────────────────┐
 │                    USER SPACE                           │
 │                                                         │
 │   socket()   ioctl(SIOCETHTOOL)   /sys/class/net/*     │
 │      │              │                    │              │
 ╞══════╪══════════════╪════════════════════╪══════════════╡
 │      │         KERNEL SPACE              │              │
 │      ▼              ▼                    ▼              │
 │  ┌────────┐  ┌───────────┐  ┌──────────────────────┐   │
 │  │ Socket │  │  ethtool  │  │      sysfs core       │   │
 │  │  Layer │  │  handler  │  │  (kobject hierarchy)  │   │
 │  └───┬────┘  └─────┬─────┘  └──────────┬───────────┘   │
 │      │             │                    │               │
 │      ▼             ▼                    ▼               │
 │  ┌─────────────────────────────────────────────────┐    │
 │  │           Network Core (net/core/)              │    │
 │  │                                                 │    │
 │  │  struct net_device     net_device_ops            │    │
 │  │  struct sk_buff        ethtool_ops               │    │
 │  │  NAPI subsystem        rtnl_link_ops             │    │
 │  └────────────────────┬────────────────────────────┘    │
 │                       │                                 │
 │                       ▼                                 │
 │  ┌─────────────────────────────────────────────────┐    │
 │  │         Network Device Driver (e.g. vnet.ko)     │    │
 │  │                                                  │    │
 │  │  .ndo_open        .ndo_start_xmit               │    │
 │  │  .ndo_stop        .ndo_get_stats                │    │
 │  └────────────────────┬─────────────────────────────┘    │
 │                       │                                 │
 │                       ▼                                 │
 │  ┌─────────────────────────────────────────────────┐    │
 │  │              Hardware / Bus Layer                │    │
 │  │     PCI / Platform / USB / SPI / etc.            │    │
 │  └──────────────────────────────────────────────────┘    │
 └─────────────────────────────────────────────────────────┘
```

## 2. Sysfs Hierarchy for a Network Interface

```
 /sys/class/net/
 │
 ├── lo/                          ◄─── Loopback (type=772)
 │   ├── type ............. 772
 │   ├── address .......... 00:00:00:00:00:00
 │   ├── mtu .............. 65536
 │   └── operstate ........ unknown
 │
 ├── eth0/                        ◄─── Physical NIC (type=1)
 │   ├── type ............. 1            (ARPHRD_ETHER)
 │   ├── address .......... aa:bb:cc:dd:ee:ff
 │   ├── mtu .............. 1500
 │   ├── operstate ........ up
 │   ├── speed ............ 1000         (Mbps)
 │   ├── duplex ........... full
 │   ├── carrier .......... 1
 │   ├── flags ............ 0x1003       (IFF_UP|IFF_BROADCAST|IFF_MULTICAST)
 │   │
 │   ├── statistics/              ◄─── Per-interface counters
 │   │   ├── rx_packets
 │   │   ├── rx_bytes
 │   │   ├── rx_errors
 │   │   ├── rx_dropped
 │   │   ├── tx_packets
 │   │   ├── tx_bytes
 │   │   ├── tx_errors
 │   │   ├── tx_dropped
 │   │   ├── collisions
 │   │   └── multicast
 │   │
 │   ├── queues/                  ◄─── TX/RX queue configuration
 │   │   ├── tx-0/
 │   │   │   ├── tx_timeout
 │   │   │   ├── tx_packets .... per-queue counter
 │   │   │   ├── tx_bytes ...... per-queue counter
 │   │   │   └── xps_cpus ..... CPU affinity bitmap
 │   │   ├── tx-1/
 │   │   │   └── ...
 │   │   ├── rx-0/
 │   │   │   ├── rps_cpus ...... RPS CPU bitmap
 │   │   │   └── rps_flow_cnt
 │   │   └── rx-1/
 │   │       └── ...
 │   │
 │   └── device -> ../../../0000:01:00.0   ◄─── PCI bus path
 │
 └── vnet0/                       ◄─── Virtual device (this module)
     ├── type ............. 1
     ├── address .......... XX:XX:XX:XX:XX:XX (random)
     ├── mtu .............. 1500
     └── operstate ........ down (initially)
```

## 3. /proc/net/dev Format (One Line per Interface)

```
 Inter-|   Receive                                                |  Transmit
  face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed
   eth0: 123456   1000    0    0    0     0          0         0   654321    800    0    0    0     0       0          0
     lo:   5000     50    0    0    0     0          0         0     5000     50    0    0    0     0       0          0
  vnet0:      0      0    0    0    0     0          0         0        0      0    0    0    0     0       0          0
        ├──────────────────────────────────────┤├─────────────────────────────────────────────┤
                    RX counters                               TX counters
```

## 4. Ethtool IOCTL Interface

```
  Userspace                              Kernel
  ─────────                              ──────

  ┌────────────┐                   ┌───────────────────┐
  │  ethtool   │                   │  net/ethtool/      │
  │  command   │                   │  ioctl.c           │
  └─────┬──────┘                   └─────┬─────────────┘
        │                                │
        │  ioctl(fd, SIOCETHTOOL, &ifr)  │
        ├───────────────────────────────►│
        │                                │
        │                                ▼
        │                   ┌────────────────────────┐
        │                   │ ethtool_ops dispatch   │
        │                   │                        │
        │                   │ ETHTOOL_GDRVINFO       │
        │                   │  → .get_drvinfo()      │
        │                   │                        │
        │                   │ ETHTOOL_GLINK          │
        │                   │  → .get_link()         │
        │                   │                        │
        │                   │ ETHTOOL_GSTATS         │
        │                   │  → .get_ethtool_stats()│
        │                   │                        │
        │                   │ ETHTOOL_GRINGPARAM     │
        │                   │  → .get_ringparam()    │
        │                   └────────────────────────┘
```

## 5. Network Interface State Machine

```
     ┌─────────┐    register_netdev()    ┌────────────┐
     │  ALLOC  ├────────────────────────►│ REGISTERED │
     └─────────┘                         └──────┬─────┘
                                                │
                              ip link set up     │
                              (.ndo_open)        │
                                                ▼
                                         ┌────────────┐
                            ┌───────────►│     UP     │
                            │            └──────┬─────┘
                            │                   │
                ip link set up          ip link set down
                (.ndo_open)             (.ndo_stop)
                            │                   │
                            │                   ▼
                         ┌──┴─────────┐  ┌────────────┐
                         │ REGISTERED │◄─┤    DOWN    │
                         └──────┬─────┘  └────────────┘
                                │
                  unregister_netdev()
                                │
                                ▼
                         ┌────────────┐
                         │   FREED    │
                         └────────────┘
```

## 6. struct net_device — Key Fields Map

```
  struct net_device (allocated by alloc_etherdev)
  ┌───────────────────────────────────────────────────┐
  │                                                   │
  │  name[IFNAMSIZ] ........... "vnet0"               │
  │  ifindex .................. 3  (auto-assigned)     │
  │  dev_addr[ETH_ALEN] ...... MAC address (6 bytes)  │
  │                                                   │
  │  ┌─── Limits ──────────────────────────────┐      │
  │  │  mtu .................. 1500             │      │
  │  │  min_mtu .............. 68               │      │
  │  │  max_mtu .............. 65535            │      │
  │  └─────────────────────────────────────────┘      │
  │                                                   │
  │  ┌─── Flags ───────────────────────────────┐      │
  │  │  flags ................ IFF_NOARP        │      │
  │  │  features ............. NETIF_F_HW_CSUM  │      │
  │  └─────────────────────────────────────────┘      │
  │                                                   │
  │  ┌─── Operations ─────────────────────────┐       │
  │  │  netdev_ops ──►  .ndo_open             │       │
  │  │                  .ndo_stop              │       │
  │  │                  .ndo_start_xmit        │       │
  │  │                  .ndo_get_stats         │       │
  │  │                                        │       │
  │  │  ethtool_ops ──► .get_drvinfo          │       │
  │  │                  .get_link              │       │
  │  │                  .get_ringparam         │       │
  │  └────────────────────────────────────────┘       │
  │                                                   │
  │  ┌─── Private Data (netdev_priv) ─────────┐       │
  │  │  Follows immediately after net_device   │       │
  │  │  in the same allocation                 │       │
  │  │                                         │       │
  │  │  struct vnet_priv {                     │       │
  │  │      struct net_device *ndev;           │       │
  │  │      struct net_device_stats stats;     │       │
  │  │  };                                     │       │
  │  └─────────────────────────────────────────┘       │
  └───────────────────────────────────────────────────┘
```

## 7. XPS/RPS CPU Affinity (Multi-Queue NICs)

```
  CPU 0    CPU 1    CPU 2    CPU 3
   │        │        │        │
   ▼        ▼        ▼        ▼
  ┌────┐  ┌────┐  ┌────┐  ┌────┐    XPS (Transmit Packet Steering)
  │TX-0│  │TX-1│  │TX-2│  │TX-3│    xps_cpus: per-queue CPU bitmap
  └────┘  └────┘  └────┘  └────┘    Steers TX to CPU-local queue
                                     Reduces cache bouncing

  ┌────┐  ┌────┐  ┌────┐  ┌────┐    RPS (Receive Packet Steering)
  │RX-0│  │RX-1│  │RX-2│  │RX-3│    rps_cpus: per-queue CPU bitmap
  └────┘  └────┘  └────┘  └────┘    Software RSS for single-queue NICs
   │        │        │        │
   ▼        ▼        ▼        ▼
  CPU 0    CPU 1    CPU 2    CPU 3
```
