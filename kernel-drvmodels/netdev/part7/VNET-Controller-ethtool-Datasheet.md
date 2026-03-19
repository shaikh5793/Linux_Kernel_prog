<!-- SPDX-License-Identifier: GPL-2.0 -->
<!-- Copyright (c) 2024 TECH VEDA -->
<!-- Author: Raghu Bharadwaj -->

# VNET Virtual Ethernet Controller - ethtool Datasheet
## Part 7: ethtool Integration

---

## 1. Overview

ethtool is the standard Linux command-line tool for querying and configuring
network interface settings. It communicates with drivers through the kernel's
`ethtool_ops` callback structure. Each callback corresponds to one or more
ethtool command-line flags.

Part 7 adds `ethtool_ops` to the VNET driver with 9 callbacks:

| # | Callback               | Purpose                          |
|---|------------------------|----------------------------------|
| 1 | get_drvinfo            | Driver name, version, bus info   |
| 2 | get_link               | Link up/down detection           |
| 3 | get_link_ksettings     | Report speed, duplex, autoneg    |
| 4 | set_link_ksettings     | Validate requested link settings |
| 5 | get_strings            | Names for custom statistics      |
| 6 | get_sset_count         | Number of custom statistics      |
| 7 | get_ethtool_stats      | Custom statistics values         |
| 8 | get_regs_len           | Size of register dump buffer     |
| 9 | get_regs               | Raw register dump                |

Driver identity:
- DRV_NAME: `vnet_ethtool`
- DRV_VERSION: `6.0.0`
- Registered via: `ndev->ethtool_ops = &vnet_ethtool_ops;`

---

## 2. ethtool Architecture

How an ethtool command reaches the driver and returns data to userspace:

```
  USERSPACE                        KERNEL                           HARDWARE
  =========                        ======                           ========

  +-----------+    ioctl /     +----------------+                +------------+
  | ethtool   | -- netlink --> | ethtool_ops    |                | VNET       |
  | -i vnet0  |               | dispatch layer |                | Registers  |
  +-----------+               +-------+--------+                +------+-----+
       ^                              |                                |
       |                              v                                |
       |                     +--------+---------+    ioread32()        |
       |                     | vnet_get_drvinfo | <--------------------+
       |                     | vnet_get_link    |    iowrite32()       |
       |                     | vnet_get_regs    | ------------------->-+
       |                     | ... (9 callbacks)|                      |
       |                     +--------+---------+                      |
       |                              |                                |
       +---- result returned ---------+                                |
              to userspace                                             |

  Registration in probe():
      ndev->ethtool_ops = &vnet_ethtool_ops;

  Callback structure:
      static const struct ethtool_ops vnet_ethtool_ops = {
          .get_drvinfo         = vnet_get_drvinfo,
          .get_link            = vnet_get_link,
          .get_link_ksettings  = vnet_get_link_ksettings,
          .set_link_ksettings  = vnet_set_link_ksettings,
          .get_strings         = vnet_get_strings,
          .get_sset_count      = vnet_get_sset_count,
          .get_ethtool_stats   = vnet_get_ethtool_stats,
          .get_regs_len        = vnet_get_regs_len,
          .get_regs            = vnet_get_regs,
      };
```

---

## 3. Statistics Architecture

The driver exposes 10 custom statistics. Eight come from `priv->stats`
(the `net_device_stats` structure updated in the data path). Two are
extended counters stored directly in `struct vnet_priv`.

```
  DATA PATH                          ETHTOOL STATS FLOW
  =========                          ==================

  vnet_xmit()                        ethtool -S vnet0
    priv->stats.tx_packets++              |
    priv->stats.tx_bytes += len           v
    priv->stats.tx_dropped++         vnet_get_sset_count()
    priv->tx_ring_full_count++         returns 10 (VNET_STAT_COUNT)
                                          |
  vnet_napi_poll()                        v
    priv->stats.rx_packets++         vnet_get_strings()
    priv->stats.rx_bytes += len        copies vnet_stat_strings[]
    priv->stats.rx_errors++               |
    priv->stats.rx_dropped++              v
    priv->napi_polls++               vnet_get_ethtool_stats()
                                       fills data[] array:
  vnet_interrupt()
    priv->stats.tx_errors++          +-------+-------------------+-----------------------+
    priv->stats.rx_errors++          | Index | String Name       | Source                |
                                     +-------+-------------------+-----------------------+
                                     |   0   | tx_packets        | priv->stats.tx_packets|
                                     |   1   | tx_bytes          | priv->stats.tx_bytes  |
                                     |   2   | tx_errors         | priv->stats.tx_errors |
                                     |   3   | tx_dropped        | priv->stats.tx_dropped|
                                     |   4   | rx_packets        | priv->stats.rx_packets|
                                     |   5   | rx_bytes          | priv->stats.rx_bytes  |
                                     |   6   | rx_errors         | priv->stats.rx_errors |
                                     |   7   | rx_dropped        | priv->stats.rx_dropped|
                                     |   8   | napi_polls        | priv->napi_polls      |
                                     |   9   | tx_ring_full      | priv->tx_ring_full_cnt|
                                     +-------+-------------------+-----------------------+

  String table (compiled into the driver):
      static const char vnet_stat_strings[][ETH_GSTRING_LEN] = {
          "tx_packets", "tx_bytes",   "tx_errors",  "tx_dropped",
          "rx_packets", "rx_bytes",   "rx_errors",  "rx_dropped",
          "napi_polls", "tx_ring_full",
      };
```

---

## 4. Register Dump Architecture

`get_regs_len` returns `64 * sizeof(u32)` = 256 bytes.
`get_regs` reads 64 consecutive 32-bit registers starting at offset 0x00
using `ioread32(priv->regs + i * 4)`.

```
  Register Space (BAR0 MMIO)                 Register Dump Buffer
  ==========================                 ====================

  Offset   Register                          data[]   Value
  ------   --------                          ------   -----
  0x000    CTRL         <-- ioread32() -->   [0]      ctrl bits
  0x004    STATUS       <-- ioread32() -->   [1]      status bits
  0x008    INT_STATUS   <-- ioread32() -->   [2]      int status
  0x00C    INT_MASK     <-- ioread32() -->   [3]      int mask
  0x010    MAC_ADDR_LOW <-- ioread32() -->   [4]      mac low
  0x014    MAC_ADDR_HIGH<-- ioread32() -->   [5]      mac high
    ...       ...                              ...      ...
  0x0FC    (offset 63)  <-- ioread32() -->   [63]     reg 63

  Total: 64 registers x 4 bytes = 256 bytes

  Loop in vnet_get_regs():
      regs->version = 1;
      for (i = 0; i < 64; i++)
          reg_data[i] = ioread32(priv->regs + i * 4);

  This covers the core control/status registers and TX/RX ring
  configuration registers. Statistics registers at 0x200+ are
  outside this 256-byte window.
```

---

## 5. Link Settings

`get_link_ksettings` reports fixed capabilities based on the hardware
design. `set_link_ksettings` only accepts 1000/Full (rejects everything
else with -EINVAL).

```
  VNET_STATUS register (offset 0x004)
  +----+----+----+----+----+----+----+----+
  | 31 | .. |  1 |  0 |              Bits |
  +----+----+----+----+----+----+----+----+
                  |  ^
                  |  |
                  |  VNET_STATUS_LINK_UP (bit 0)
                  |
                  v
        +---------+-----------+
        | LINK_UP |  !LINK_UP |
        +---------+-----------+
        | speed:  | speed:    |
        |  1000   |  UNKNOWN  |
        | duplex: | duplex:   |
        |  FULL   |  UNKNOWN  |
        +---------+-----------+

  Reported link modes (always):
      supported:    1000baseT_Full, Autoneg
      advertising:  1000baseT_Full, Autoneg
      autoneg:      AUTONEG_ENABLE
      port:         PORT_TP

  get_link():
      return !!(ioread32(priv->regs + VNET_STATUS) & VNET_STATUS_LINK_UP);
      Returns 1 (link up) or 0 (link down).
```

---

## 6. Driver Info Flow

`get_drvinfo` fills three fields using string copy and `pci_name()`:

```
  vnet_get_drvinfo(ndev, info)
  +------------------------------------------+
  |                                          |
  |  info->driver   <-- "vnet_ethtool"       |
  |                     (DRV_NAME)           |
  |                                          |
  |  info->version  <-- "6.0.0"             |
  |                     (DRV_VERSION)        |
  |                                          |
  |  info->bus_info <-- pci_name(priv->pdev) |
  |                     e.g. "0000:00:04.0"  |
  |                                          |
  +------------------------------------------+

  Uses strscpy() for safe bounded string copy:
      strscpy(info->driver,   DRV_NAME,            sizeof(info->driver));
      strscpy(info->version,  DRV_VERSION,          sizeof(info->version));
      strscpy(info->bus_info, pci_name(priv->pdev), sizeof(info->bus_info));
```

---

## 7. ethtool Command Reference

| Command           | Flag | Callback(s)                                      | Output                              |
|-------------------|------|--------------------------------------------------|-------------------------------------|
| `ethtool -i vnet0`| -i   | get_drvinfo                                      | driver, version, bus_info           |
| `ethtool vnet0`   | none | get_link_ksettings, get_link                      | speed, duplex, autoneg, link status |
| `ethtool -S vnet0`| -S   | get_sset_count, get_strings, get_ethtool_stats   | 10 custom statistics with names     |
| `ethtool -d vnet0`| -d   | get_regs_len, get_regs                           | 64 x 32-bit register dump           |
| `ethtool -s vnet0 speed 1000 duplex full` | -s | set_link_ksettings          | Validates and accepts (or -EINVAL)  |

Example output for `ethtool -i vnet0`:

```
  driver: vnet_ethtool
  version: 6.0.0
  bus-info: 0000:00:04.0
```

Example output for `ethtool -S vnet0`:

```
  NIC statistics:
       tx_packets: 1024
       tx_bytes: 131072
       tx_errors: 0
       tx_dropped: 0
       rx_packets: 512
       rx_bytes: 65536
       rx_errors: 0
       rx_dropped: 0
       napi_polls: 48
       tx_ring_full: 2
```

---

## 8. Hardware Statistics Registers

The hardware platform maintains four statistics counters in MMIO register
space. These are updated by the hardware simulation on each successful
TX/RX operation. The driver can read them via `ioread32()`.

```
  Register Map (Statistics Block at 0x200)
  =========================================

  Offset   Name               Type  Description
  ------   ----               ----  -----------
  0x200    STATS_TX_PACKETS   R     Total packets transmitted by hardware
  0x204    STATS_TX_BYTES     R     Total bytes transmitted by hardware
  0x240    STATS_RX_PACKETS   R     Total packets received by hardware
  0x244    STATS_RX_BYTES     R     Total bytes received by hardware

  Reading:
      u32 hw_tx_pkts = ioread32(priv->regs + 0x200);
      u32 hw_tx_bytes = ioread32(priv->regs + 0x204);
      u32 hw_rx_pkts = ioread32(priv->regs + 0x240);
      u32 hw_rx_bytes = ioread32(priv->regs + 0x244);
```

Note: The Part 7 ethtool callbacks currently report statistics from the
driver's in-memory `priv->stats` counters (updated in the TX/RX data
path), not from these hardware registers. The hardware registers provide
an independent count that can be used for cross-validation.

---

*Document Number: VNET-DS-ETHTOOL-006*
*Part 7 of the PCI Network Driver Curriculum*
