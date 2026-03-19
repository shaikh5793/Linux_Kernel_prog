<!-- SPDX-License-Identifier: GPL-2.0 -->
<!-- Copyright (c) 2024 TECH VEDA -->
<!-- Author: Raghu Bharadwaj -->

# VNET Virtual Ethernet Controller -- Basic Datasheet
## Part 2: PCI Driver Skeleton

---

## 1. Overview

The VNET Virtual Ethernet Controller is a PCI network device used throughout
this driver curriculum. This datasheet covers **only** the hardware features
exercised by the Part 2 skeleton driver:

- PCI enumeration (probe / remove)
- BAR0 MMIO register access via `ioread32` / `iowrite32`
- Basic enable / disable through the CTRL register
- Link-status detection through the STATUS register
- MAC address stored in registers (read at probe, not yet used for RX)
- A stub transmit path (packet is accepted and immediately dropped)

Features such as descriptor rings, DMA, interrupts, and checksum offload
are **not** used in Part 2 and are documented in later datasheets.

### 1.1 PCI Identity

| Field             | Value    | Notes                          |
|-------------------|----------|--------------------------------|
| Vendor ID (VID)   | `0x1234` | Used in `pci_device_id` table  |
| Device ID (DID)   | `0x5678` | Used in `pci_device_id` table  |
| Class Code        | `0x0200` | Ethernet controller            |
| BAR0 Size         | 4 KB     | MMIO register space            |

---

## 2. PCI Configuration Space

The first 64 bytes of PCI configuration space are shown below.
The driver never reads these directly -- the PCI core parses them --
but understanding the layout helps when debugging with `lspci`.

```
PCI Configuration Space Header (Type 0)
Offset
(hex)    31            16 15             0
        +----------------+----------------+
  00    |  Device ID     |  Vendor ID     |     0x5678 | 0x1234
        |   (0x5678)     |   (0x1234)     |
        +----------------+----------------+
  04    |    Status      |   Command      |     PCI core sets bus-master, MMIO
        +----------------+----------------+
  08    |         Class Code / Rev        |     Class 0x020000 (Ethernet)
        +----------------+----------------+
  0C    |  BIST | Hdr | Lat | CacheLn    |     Header type 0
        +----------------+----------------+
  10    |          BAR0 (MMIO)            |  <-- 4 KB register window
        +---------------------------------+
  14    |          BAR1 (unused)          |
        +---------------------------------+
   ...           (BARs 2-5, etc.)
        +---------------------------------+
  3C    |  Max_Lat | Min_Gnt | Pin | Line |     IRQ routing
        +---------------------------------+
```

The driver matches on VID/DID via the `pci_device_id` table.  The PCI
subsystem maps BAR0 before calling `probe`.

---

## 3. Controller Block Diagram

```
                    PCI Bus
                      |
        ==============|===============
        |     PCI Interface           |
        |   (config space, BAR0 decode)|
        ===============|===============
                       |
            +----------+----------+
            |    Register File    |
            |   (BAR0, 4 KB)     |
            |                     |
            |  CTRL      0x000   |
            |  STATUS    0x004   |
            |  INT_MASK  0x008   |  <-- not used in Part 2
            |  INT_STATUS 0x00C  |  <-- not used in Part 2
            |  MAC_LOW   0x010   |
            |  MAC_HIGH  0x014   |
            |  MAX_FRAME 0x018   |
            +-----+---------+----+
                  |         |
           +------+--+ +---+------+
           | Control  | | Status   |
           | Logic    | | Logic    |
           |          | |          |
           | ENABLE --+--> LINK_UP |
           | RESET    | | RX_ACT   |
           |          | | TX_ACT   |
           +----------+ +----------+
                  |         |
            ======|=========|======
            |    Data Path         |  <-- idle in Part 2
            |  (TX/RX engines,    |      (stub xmit drops
            |   DMA, rings)       |       every packet)
            =======================
```

In Part 2 the data path is never activated.  The stub `ndo_start_xmit`
frees every SKB and increments `tx_dropped`.

---

## 4. Register Map (Part 2 Subset)

All registers are 32-bit, little-endian, accessed through BAR0 MMIO.

| Offset | Name           | Access | Reset Value  | Description                         |
|--------|----------------|--------|--------------|-------------------------------------|
| 0x000  | CTRL           | R/W    | 0x0000_0000  | Controller control                  |
| 0x004  | STATUS         | R      | 0x0000_0000  | Controller status                   |
| 0x008  | INT_MASK       | R/W    | 0x0000_0000  | Interrupt mask (Part 4+)           |
| 0x00C  | INT_STATUS     | R/W1C  | 0x0000_0000  | Interrupt status (Part 4+)         |
| 0x010  | MAC_ADDR_LOW   | R/W    | 0x0000_0000  | MAC address bytes [3:0]             |
| 0x014  | MAC_ADDR_HIGH  | R/W    | 0x0000_0000  | MAC address bytes [5:4] (bits 15:0) |
| 0x018  | MAX_FRAME_REG  | R/W    | 0x0000_05EE  | Maximum frame size (1518)           |

INT_MASK and INT_STATUS are listed for completeness but are not touched
until Part 3 (interrupt handling).

---

## 5. CTRL Register (0x000) Bit Fields

```
 31                              16 15           8 7 6 5 4 3 2 1 0
+----------------------------------+---------------+-+-+-+-+-+-+-+-+
|            Reserved (0)          |  Reserved (0) |T|R|L|R| Rsvd|R|E|
|                                  |               |X|X|B|S|     |I|N|
|                                  |               | | | |T|     |N|A|
|                                  |               |E|E|K| |     |G|B|
|                                  |               |N|N| | |     | |L|
+----------------------------------+---------------+-+-+-+-+---+-+-+-+
                                                    | | | |       |  \__ Bit 0: ENABLE
                                                    | | | |       \____ Bit 1: RING_ENABLE
                                                    | | | \____________ Bit 4: RESET
                                                    | | \______________ Bit 5: LOOPBACK
                                                    | \________________ Bit 6: RX_ENABLE
                                                    \__________________ Bit 7: TX_ENABLE
```

### Part 2 Usage

| Bit | Name        | Part 2 role                                         |
|-----|-------------|-----------------------------------------------------|
|  0  | ENABLE      | Set to 1 in `ndo_open`, cleared to 0 in `ndo_stop`  |
|  1  | RING_ENABLE | Not used in this part                                |
|  4  | RESET       | Not used in this part                                |
|  5  | LOOPBACK    | Not used in this part                                |
|  6  | RX_ENABLE   | Not used in this part                                |
|  7  | TX_ENABLE   | Not used in this part                                |

In Part 2, `ndo_open` writes **only** `VNET_CTRL_ENABLE` (0x01) and
`ndo_stop` writes **0x00**.

---

## 6. STATUS Register (0x004) Bit Fields

```
 31                              16 15           8 7 6 5 4       0
+----------------------------------+---------------+-+-+-+---------+
|            Reserved (0)          |  Reserved (0) |T|R|L| Rsvd    |
|                                  |               |X|X|I|         |
|                                  |               | | |N|         |
|                                  |               |A|A|K|         |
|                                  |               |C|C| |         |
|                                  |               |T|T|U|         |
|                                  |               | | |P|         |
+----------------------------------+---------------+-+-+-+---------+
                                                    | | |
                                                    | | \__________ Bit 5: LINK_UP
                                                    | \____________ Bit 6: RX_ACTIVE
                                                    \______________ Bit 7: TX_ACTIVE
```

### Part 2 Usage

| Bit | Name      | Part 2 role                                              |
|-----|-----------|----------------------------------------------------------|
|  5  | LINK_UP   | Read in `ndo_open` to call `netif_carrier_on/off`        |
|  6  | RX_ACTIVE | Not checked in Part 2                                    |
|  7  | TX_ACTIVE | Not checked in Part 2                                    |

---

## 7. PCI Probe / Remove Lifecycle

```
                  insmod vnet.ko
                       |
                       v
              pci_register_driver()
                       |
                       v
            +---------------------+
            | PCI core scans bus, |
            | matches VID/DID     |
            +---------------------+
                       |
                       v
              vnet_probe(pdev, id)
                       |
          +------------+-------------+
          |            |             |
          v            v             v
   pci_enable_device  alloc_netdev  vnet_hw_map_bar0
   (enable I/O &      (allocates    (pci_iomap:
    bus-master)        net_device    maps BAR0 to
                       with          kernel virtual
                       NET_NAME_USER)address)
          |            |             |
          +-----+------+------+------+
                |             |
                v             v
        SET_NETDEV_DEV    pci_set_drvdata
        (tie netdev to    (store priv ptr)
         PCI parent)
                |
                v
         register_netdev()
         (creates ethN,
          device is live)
                |
                v
         dev_info: "probed"
                |
                v
        [device is registered
         but DOWN -- no traffic]


              rmmod vnet.ko
                   |
                   v
            vnet_remove(pdev)
                   |
          +--------+---------+
          |        |         |
          v        v         v
   unregister_   vnet_hw_   pci_disable_
   netdev()      unmap_bar0  device()
                 (pci_iounmap)
```

---

## 8. Open / Stop State Machine

```
     register_netdev()
           |
           v
     +-----------+        ndo_open()        +-----------+
     |   DOWN    | -----------------------> |    UP     |
     |           |                          |           |
     | carrier:  |    1. iowrite32(         | carrier:  |
     | off       |       ENABLE, CTRL)      | on / off  |
     |           |    2. read STATUS         | (from     |
     |           |    3. if LINK_UP:         |  LINK_UP) |
     |           |       netif_carrier_on   |           |
     |           |       else               |           |
     |           |       netif_carrier_off  | xmit:     |
     |           |    4. netif_start_queue   | stub-drop |
     +-----------+                          +-----------+
           ^                                      |
           |          ndo_stop()                   |
           +--------------------------------------+
                  1. netif_stop_queue
                  2. netif_carrier_off
                  3. iowrite32(0, CTRL)
```

All packets submitted to `ndo_start_xmit` while UP are freed immediately
with `dev_kfree_skb_any` and counted as `tx_dropped`.

---

## 9. Programming Model

Part 2 uses **direct MMIO** via `ioread32` / `iowrite32` on the BAR0
mapping.  There is no `hw_ops` indirection at this stage.

### 9.1 Reading a Register

```c
u32 status;

status = ioread32(priv->regs + VNET_STATUS);
if (status & VNET_STATUS_LINK_UP)
        netif_carrier_on(netdev);
else
        netif_carrier_off(netdev);
```

### 9.2 Writing a Register

```c
/* Enable the controller (ndo_open) */
iowrite32(VNET_CTRL_ENABLE, priv->regs + VNET_CTRL);

/* Disable the controller (ndo_stop) */
iowrite32(0, priv->regs + VNET_CTRL);
```

### 9.3 Reading the MAC Address

```c
u32 lo, hi;

lo = ioread32(priv->regs + VNET_MAC_ADDR_LOW);
hi = ioread32(priv->regs + VNET_MAC_ADDR_HIGH);

netdev->dev_addr[0] = (lo >>  0) & 0xFF;
netdev->dev_addr[1] = (lo >>  8) & 0xFF;
netdev->dev_addr[2] = (lo >> 16) & 0xFF;
netdev->dev_addr[3] = (lo >> 24) & 0xFF;
netdev->dev_addr[4] = (hi >>  0) & 0xFF;
netdev->dev_addr[5] = (hi >>  8) & 0xFF;
```

All register offsets are defined in `vnet_hw_interface.h`:

```c
#define VNET_CTRL           0x000
#define VNET_STATUS         0x004
#define VNET_INT_MASK       0x008
#define VNET_INT_STATUS     0x00C
#define VNET_MAC_ADDR_LOW   0x010
#define VNET_MAC_ADDR_HIGH  0x014
#define VNET_MAX_FRAME_REG  0x018
```

---

## 10. Platform Interface (vnet_hw)

The Part 2 driver does not call `pci_iomap` / `pci_iounmap` directly.
Instead it uses two thin wrappers from the platform layer:

### 10.1 vnet_hw_map_bar0

```
vnet_hw_map_bar0(struct pci_dev *pdev)
  Returns: void __iomem *   (kernel virtual address of BAR0)
           NULL on failure

  Internally calls pci_iomap(pdev, 0, 0) to map the entire BAR0
  region into kernel address space.
```

### 10.2 vnet_hw_unmap_bar0

```
vnet_hw_unmap_bar0(struct pci_dev *pdev, void __iomem *regs)
  Returns: void

  Internally calls pci_iounmap(pdev, regs).
```

These wrappers exist so that later parts can swap in a different BAR
mapping strategy (e.g., `devm_` managed) without changing driver code.

---

## Appendix A: Quick Reference Card

```
VID/DID:    0x1234 / 0x5678
BAR0:       4 KB MMIO register space

Key Registers for Part 2:
  CTRL   (0x000, R/W)  -- bit 0 = ENABLE
  STATUS (0x004, R)     -- bit 5 = LINK_UP

Probe sequence:
  pci_enable_device -> alloc_netdev -> vnet_hw_map_bar0
  -> SET_NETDEV_DEV -> register_netdev

Open:
  iowrite32(VNET_CTRL_ENABLE, regs + VNET_CTRL)
  check STATUS.LINK_UP -> carrier on/off
  netif_start_queue

Stop:
  netif_stop_queue
  netif_carrier_off
  iowrite32(0, regs + VNET_CTRL)

Remove:
  unregister_netdev -> vnet_hw_unmap_bar0 -> pci_disable_device

Xmit (stub):
  dev_kfree_skb_any(skb)
  stats->tx_dropped++
  return NETDEV_TX_OK
```

---

*VNET Controller Basic Datasheet -- Part 2: PCI Driver Skeleton*
*For use with the PCI Network Driver Curriculum*
