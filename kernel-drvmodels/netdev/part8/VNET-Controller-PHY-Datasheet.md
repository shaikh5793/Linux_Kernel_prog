<!-- SPDX-License-Identifier: GPL-2.0 -->
<!-- Copyright (c) 2024 TECH VEDA -->
<!-- Author: Raghu Bharadwaj -->

# VNET Virtual Ethernet Controller - PHY Datasheet
## Part 8: PHY & MDIO Bus

---

## 1. Overview

Every Ethernet NIC consists of two layers: the **MAC** (Media Access Control)
and the **PHY** (Physical Layer Transceiver). The MAC handles framing,
buffering, DMA, and interrupts. The PHY handles the electrical signaling on
the wire -- encoding/decoding bits, autonegotiation, and link detection.

The MAC communicates with the PHY over the **MDIO bus** (Management Data
Input/Output), a two-wire serial interface defined by IEEE 802.3 Clause 22.
Through MDIO, the MAC can read and write the PHY's standard MII registers
to configure speed, duplex, autonegotiation, and query link status.

In Parts 2-6, the VNET driver polled `VNET_STATUS & LINK_UP` directly to
determine link state. Part 8 replaces this with a proper PHY device connected
over an MDIO bus. The Linux PHY library (phylib) manages the PHY state
machine, polls link status, runs autonegotiation, and notifies the driver
through an `adjust_link` callback.

---

## 2. System Architecture

How the MAC, PHY, and medium connect in a real NIC (and in our virtual one):

```
  +-------------------+          MDIO Bus           +------------------+
  |                   |    (2-wire: MDC + MDIO)     |                  |
  |   MAC Controller  |<=========================>|   PHY Transceiver |
  |   (VNET NIC)      |                            |   (Virtual PHY)  |
  |                   |      Data Path (MII/       |                  |
  |   +-----------+   |      GMII/RGMII)           |   +-----------+  |      +---------+
  |   | Register  |   |<-------------------------->|   | PHY Regs  |  |<---->| Copper  |
  |   | File      |   |                            |   | (MII std) |  |      | Medium  |
  |   | (BAR0)    |   |                            |   | Regs 0-31 |  |      | (wire)  |
  |   +-----------+   |                            |   +-----------+  |      +---------+
  |                   |                            |                  |
  +-------------------+                            +------------------+

  Driver accesses MAC             Driver accesses PHY
  registers via:                  registers via:
    ioread32(priv->regs + off)      mdiobus->read(bus, phy_addr, reg)
    iowrite32(val, priv->regs)      mdiobus->write(bus, phy_addr, reg, val)

  In our virtual hardware:
    MAC regs = BAR0 MMIO array (same as Parts 2-6)
    PHY regs = accessed through MDIO control registers at BAR0 + 0x300
```

---

## 3. MDIO Bus Architecture

The MDIO bus is a shared bus that can address up to 32 PHY devices (addresses
0-31). Each PHY has 32 registers (Clause 22) or an extended register space
(Clause 45). The VNET controller uses Clause 22.

```
  MDIO Bus Topology
  =================

  +-------------+
  |   MAC       |
  | (bus master)|---+--- MDC (clock)
  |             |   +--- MDIO (data, bidirectional)
  +-------------+
        |
        |  MDIO Bus (shared, up to 32 devices)
        |
   +----+--------+--------+--------+--- - - -
   |             |        |        |
  +-----+   +-----+  +-----+  +-----+
  | PHY |   | PHY |  | PHY |  | PHY |
  | @0  |   | @1  |  | @2  |  | @31 |
  +-----+   +-----+  +-----+  +-----+

  VNET uses PHY address 0 (one PHY on the bus).


  Clause 22 MDIO Frame Format (32-bit serial frame on the wire)
  ==============================================================

  +------+------+------+---------+---------+----+----------------+
  | PRE  | ST   | OP   | PHYADDR | REGADDR | TA | DATA           |
  | 32x1 | 01   | R/W  | 5 bits  | 5 bits  | Z0 | 16 bits        |
  +------+------+------+---------+---------+----+----------------+

  PRE:     32 preamble bits (all 1s)
  ST:      Start of frame (01)
  OP:      Operation -- 10 = read, 01 = write
  PHYADDR: PHY address on the bus (0-31)
  REGADDR: Register within the PHY (0-31)
  TA:      Turnaround (2 bits)
  DATA:    16-bit read data or write data

  The MAC generates this frame by writing to the MDIO control registers.
  The driver does NOT construct frames manually -- the hardware (or our
  simulation) handles the bus protocol. The driver writes:
      MDIO_CTRL = {start=1, read/write, phy_addr, reg_addr}
      MDIO_DATA = write_data   (for writes)
  Then polls MDIO_STATUS for completion and reads MDIO_DATA (for reads).
```

---

## 4. MDIO Register Space

The VNET controller exposes three MDIO control registers in the MAC's BAR0
MMIO space. The driver uses these to perform MDIO bus transactions.

```
  MDIO Control Registers (BAR0 offsets)
  ======================================

  Offset   Name            Access   Description
  ------   ----            ------   -----------
  0x300    VNET_MDIO_CTRL  R/W      MDIO transaction control
  0x304    VNET_MDIO_DATA  R/W      16-bit PHY register data
  0x308    VNET_MDIO_STATUS R       Transaction status


  VNET_MDIO_CTRL (0x300) -- Bit Field Layout
  ============================================

  31      30       29:25         24:20         19:16      15:0
  +-------+--------+-------------+-------------+----------+----------+
  | START | R/!W   | PHY_ADDR    | REG_ADDR    | Reserved | Reserved |
  | [31]  | [30]   | [29:25]     | [24:20]     | [19:16]  | [15:0]   |
  +-------+--------+-------------+-------------+----------+----------+

  START (bit 31):     Write 1 to initiate an MDIO transaction.
                      Self-clearing: returns to 0 when transaction completes.

  R/!W (bit 30):      1 = Read from PHY register
                      0 = Write to PHY register

  PHY_ADDR (bits 29:25): 5-bit PHY address on the MDIO bus (0-31).
                          VNET uses address 0.

  REG_ADDR (bits 24:20): 5-bit register address within the PHY (0-31).
                          Maps to standard MII registers (see Section 5).


  VNET_MDIO_DATA (0x304)
  =======================

  31:16      15:0
  +----------+----------+
  | Reserved | DATA     |
  |          | [15:0]   |
  +----------+----------+

  For writes: load DATA before setting START in MDIO_CTRL.
  For reads:  read DATA after MDIO_STATUS shows ready.


  VNET_MDIO_STATUS (0x308)
  =========================

  31:1       0
  +----------+------+
  | Reserved | BUSY |
  |          | [0]  |
  +----------+------+

  BUSY (bit 0):  1 = Transaction in progress
                 0 = Ready (transaction complete or idle)


  MDIO Read Sequence                 MDIO Write Sequence
  ===================                ====================

  1. Poll STATUS until !BUSY         1. Poll STATUS until !BUSY
  2. Write CTRL:                      2. Write DATA[15:0] with value
     START=1, R/W=1 (read),          3. Write CTRL:
     PHY_ADDR, REG_ADDR                 START=1, R/W=0 (write),
  3. Poll STATUS until !BUSY            PHY_ADDR, REG_ADDR
  4. Read DATA[15:0]                  4. Poll STATUS until !BUSY
```

---

## 5. Virtual PHY Registers (Standard MII)

The virtual PHY implements the standard IEEE 802.3 MII register set. These
registers are accessed through the MDIO bus (Section 4), not through direct
BAR0 `ioread32`.

```
  PHY Register Map (Clause 22, standard MII registers)
  =====================================================

  Reg   Name    Access   Description
  ---   ----    ------   -----------
   0    BMCR    R/W      Basic Mode Control Register
   1    BMSR    R        Basic Mode Status Register
   2    PHYID1  R        PHY Identifier 1 (upper OUI)
   3    PHYID2  R        PHY Identifier 2 (lower OUI + model + rev)
   4    ANAR    R/W      Auto-Negotiation Advertisement Register
   5    ANLPAR  R        Auto-Negotiation Link Partner Ability Register
  6-31  --      --       Reserved / vendor-specific


  Register 0: BMCR -- Basic Mode Control Register
  =================================================

  15     14        13       12      11       10:7     6        5:0
  +------+---------+--------+-------+--------+--------+-------+--------+
  |RESET |LOOPBACK |SPEED_  |AN_    |PWR_    |Reserved|SPEED_ |Reserved|
  |      |         |SEL[1]  |ENABLE |DOWN    |        |SEL[0] |        |
  +------+---------+--------+-------+--------+--------+-------+--------+

  Bit 15 - RESET:       1 = PHY software reset (self-clearing)
  Bit 14 - LOOPBACK:    1 = Enable loopback mode
  Bit 13 - SPEED_SEL1:  Speed selection (MSB). Combined with bit 6:
                         {13,6} = 00: 10 Mbps
                         {13,6} = 01: 100 Mbps
                         {13,6} = 10: 1000 Mbps
  Bit 12 - AN_ENABLE:   1 = Enable auto-negotiation
  Bit 11 - PWR_DOWN:    1 = Power down the PHY
  Bit  6 - SPEED_SEL0:  Speed selection (LSB). See bit 13.

  VNET PHY defaults: RESET=0, AN_ENABLE=1, SPEED=1000 Mbps, DUPLEX=Full


  Register 1: BMSR -- Basic Mode Status Register
  ================================================

  15:7       6         5        4         3         2        1:0
  +----------+---------+--------+---------+---------+--------+---------+
  |Capability|MF_PRE   |AN_     |REMOTE   |AN_      |LINK    |Extended |
  |bits      |SUPPRESS |COMPLETE|FAULT    |ABILITY  |STATUS  |Capable  |
  +----------+---------+--------+---------+---------+--------+---------+

  Bit 5 - AN_COMPLETE:  1 = Auto-negotiation process completed
  Bit 4 - REMOTE_FAULT: 1 = Remote fault condition detected
  Bit 3 - AN_ABILITY:   1 = PHY is able to perform auto-negotiation
  Bit 2 - LINK_STATUS:  1 = Link is up
                         0 = Link is down
                         NOTE: This bit is latching-low. Once it reads 0,
                         it stays 0 until read again after link is restored.
  Bit 0 - EXT_CAPABLE:  1 = Extended register set (regs 16+) available

  Capability bits (15:7):
    Bit 15: 100BASE-T4 capable
    Bit 14: 100BASE-TX Full Duplex capable
    Bit 13: 100BASE-TX Half Duplex capable
    Bit 12: 10BASE-T Full Duplex capable
    Bit 11: 10BASE-T Half Duplex capable

  VNET PHY BMSR: LINK_STATUS=1, AN_COMPLETE=1, AN_ABILITY=1,
                 100FD=1, 100HD=1, 10FD=1, 10HD=1


  Register 2: PHYID1 -- PHY Identifier 1
  ========================================

  15:0
  +------------------+
  | OUI bits [17:2]  |
  +------------------+

  Upper 16 bits of the Organizationally Unique Identifier.
  VNET PHY: 0x0022 (example OUI)


  Register 3: PHYID2 -- PHY Identifier 2
  ========================================

  15:10       9:4         3:0
  +-----------+-----------+----------+
  | OUI[1:0]  | Model     | Revision |
  | (6 bits)  | (6 bits)  | (4 bits) |
  +-----------+-----------+----------+

  VNET PHY: OUI low = 0x15, Model = 0x61, Revision = 0x1
  Combined PHYID: 0x00221561


  Register 4: ANAR -- Auto-Negotiation Advertisement Register
  =============================================================

  15:11     10        9        8        7        6        5        4:0
  +---------+---------+--------+--------+--------+--------+--------+-------+
  |Reserved |PAUSE_   |PAUSE   |100BASE |100BASE |10BASE  |10BASE  |Selector|
  |         |ASYM     |        |TX_FD   |TX_HD   |T_FD    |T_HD    |Field  |
  +---------+---------+--------+--------+--------+--------+--------+-------+

  The driver (via phylib) writes this register to advertise what speeds
  and duplex modes the MAC supports. The PHY includes these capabilities
  in its autonegotiation advertisement to the link partner.

  VNET advertises: 10HD, 10FD, 100HD, 100FD (and 1000 via extended regs)


  Register 5: ANLPAR -- Auto-Negotiation Link Partner Ability Register
  =====================================================================

  Same bit layout as ANAR (Register 4). Contains the capabilities
  advertised by the link partner during autonegotiation. Read-only.

  After autoneg completes, phylib compares ANAR and ANLPAR to determine
  the highest common speed/duplex and configures the PHY accordingly.
```

---

## 6. PHY Link State Machine

The Linux PHY library manages a state machine for each attached PHY. The
driver does not poll for link status -- phylib does it automatically and
calls the driver's `adjust_link` callback on transitions.

```
  PHY State Machine (simplified view of phylib states)
  =====================================================

               phy_connect()
                    |
                    v
              +----------+
              | PHY_READY|  PHY attached, not started
              +-----+----+
                    |
               phy_start()
                    |
                    v
              +-----+----+
         +--->| PHY_AN   |  Running auto-negotiation
         |    +-----+----+
         |          |
         |     AN completes,
         |     link partner found
         |          |
         |          v
         |    +-----+------+
         |    | PHY_RUNNING|  Link is UP, normal operation
         |    +-----+------+       |
         |          |              |
         |     link lost       adjust_link() fires:
         |          |            phydev->link = 1
         |          v            phydev->speed = 1000
         |    +-----+------+    phydev->duplex = DUPLEX_FULL
         +----|  PHY_NOLINK |    -> netif_carrier_on(ndev)
              +-----+------+
                    |
               link detected
                    |          adjust_link() fires:
                    v            phydev->link = 0
              restart AN         -> netif_carrier_off(ndev)
              (back to PHY_AN)


  adjust_link callback (fires on ANY link state change):
  ======================================================

      static void vnet_adjust_link(struct net_device *ndev)
      {
          struct phy_device *phydev = ndev->phydev;

          if (phydev->link) {
              /* PHY says link is up */
              netif_carrier_on(ndev);
              /* Optionally log speed/duplex change */
          } else {
              /* PHY says link is down */
              netif_carrier_off(ndev);
          }
      }

  phylib calls adjust_link from its polling workqueue. The driver never
  needs to read BMSR or check link status directly.
```

---

## 7. Driver Integration Flow

How the MDIO bus and PHY integrate into the driver lifecycle:

```
  PROBE (module load, PCI match)
  ===============================

  vnet_probe(pdev)
    |
    +-- [Parts 2-6 setup: PCI enable, BAR map, alloc_etherdev, etc.]
    |
    +-- bus = mdiobus_alloc()
    |     |
    |     +-- bus->name = "vnet-mdio"
    |     +-- bus->read = vnet_mdio_read     // callback: read PHY reg
    |     +-- bus->write = vnet_mdio_write   // callback: write PHY reg
    |     +-- bus->parent = &pdev->dev
    |     +-- snprintf(bus->id, "vnet-mdio-%s", pci_name(pdev))
    |
    +-- mdiobus_register(bus)
    |     |
    |     +-- For each address 0-31:
    |           try to read PHYID1/PHYID2
    |           if valid OUI found -> create phy_device
    |           VNET PHY found at address 0 (OUI 0x00221561)
    |
    +-- phydev = phy_connect(ndev, phy_addr,
    |                        vnet_adjust_link,
    |                        PHY_INTERFACE_MODE_GMII)
    |     |
    |     +-- Binds PHY to net_device
    |     +-- Registers adjust_link callback
    |     +-- Sets interface mode (GMII for 1000 Mbps)
    |
    +-- register_netdev(ndev)


  OPEN (ip link set up)
  ======================

  vnet_open(ndev)
    |
    +-- [allocate rings, enable hardware, request_irq, etc.]
    |
    +-- phy_start(ndev->phydev)
          |
          +-- Starts phylib state machine
          +-- Triggers auto-negotiation
          +-- Begins periodic link polling
          +-- adjust_link fires when link comes up


  ADJUST_LINK (called by phylib on link change)
  ===============================================

  vnet_adjust_link(ndev)
    |
    +-- if (phydev->link)
    |     +-- netif_carrier_on(ndev)      // tell network stack: link up
    |     +-- log: "Link up: %d/%s",
    |              phydev->speed,
    |              phydev->duplex == DUPLEX_FULL ? "Full" : "Half"
    |
    +-- else
          +-- netif_carrier_off(ndev)     // tell network stack: link down
          +-- log: "Link down"


  STOP (ip link set down)
  ========================

  vnet_stop(ndev)
    |
    +-- phy_stop(ndev->phydev)
    |     |
    |     +-- Stops phylib state machine
    |     +-- No more adjust_link callbacks
    |
    +-- [free rings, free_irq, disable hardware, etc.]


  REMOVE (module unload)
  =======================

  vnet_remove(pdev)
    |
    +-- unregister_netdev(ndev)
    |
    +-- phy_disconnect(ndev->phydev)
    |     |
    |     +-- Detaches PHY from net_device
    |
    +-- mdiobus_unregister(priv->mii_bus)
    |     |
    |     +-- Removes all PHY devices discovered during register
    |
    +-- mdiobus_free(priv->mii_bus)
    |     |
    |     +-- Frees the mdiobus structure
    |
    +-- [Parts 2-6 teardown: unmap BAR, disable PCI, free_netdev]
```

---

## 8. phylib vs. Direct Register Access

Part 7 managed link state by directly reading the MAC's STATUS register.
Part 8 delegates all link management to phylib and the PHY device.

```
  BEFORE (Part 7): Direct register polling
  ==========================================

  Interrupt handler or ethtool:
      u32 status = ioread32(priv->regs + VNET_STATUS);
      if (status & VNET_STATUS_LINK_UP)
          netif_carrier_on(ndev);
      else
          netif_carrier_off(ndev);

  ethtool get_link:
      return !!(ioread32(priv->regs + VNET_STATUS) & VNET_STATUS_LINK_UP);

  ethtool get_link_ksettings:
      /* Hardcoded: 1000/Full, no real PHY */
      cmd->base.speed = SPEED_1000;
      cmd->base.duplex = DUPLEX_FULL;


  AFTER (Part 8): PHY-driven via phylib
  =======================================

  adjust_link callback (called automatically by phylib):
      if (phydev->link)
          netif_carrier_on(ndev);
      else
          netif_carrier_off(ndev);

  ethtool get_link:
      /* phylib provides this automatically via phy_ethtool_get_link() */

  ethtool get_link_ksettings:
      /* Delegates to phylib: */
      return phy_ethtool_get_link_ksettings(ndev, cmd);
      /* phylib reads speed/duplex from actual PHY autoneg result */


  Side-by-Side Comparison
  ========================

  +-------------------------+----------------------------------+----------------------------------+
  | Aspect                  | Part 7 (direct)                  | Part 8 (phylib)                  |
  +-------------------------+----------------------------------+----------------------------------+
  | Link detection          | ioread32(STATUS) & LINK_UP       | PHY polls BMSR, calls            |
  |                         | in ISR or ethtool callback       | adjust_link automatically        |
  +-------------------------+----------------------------------+----------------------------------+
  | Speed/duplex reporting  | Hardcoded 1000/Full              | Read from phydev->speed,         |
  |                         |                                  | phydev->duplex (autoneg result)  |
  +-------------------------+----------------------------------+----------------------------------+
  | Autonegotiation         | Faked (always reports enabled)   | Real: phylib runs AN state       |
  |                         |                                  | machine, reads ANAR/ANLPAR       |
  +-------------------------+----------------------------------+----------------------------------+
  | ethtool link settings   | Custom get/set callbacks         | phy_ethtool_get/set_link_        |
  |                         | with manual field filling        | ksettings (one-liner delegates)   |
  +-------------------------+----------------------------------+----------------------------------+
  | Link state management   | Driver responsibility            | phylib responsibility            |
  |                         | (easy to get wrong)              | (battle-tested kernel code)      |
  +-------------------------+----------------------------------+----------------------------------+
  | MDIO bus                | Not present                      | mdiobus with read/write cbs      |
  +-------------------------+----------------------------------+----------------------------------+
  | PHY identification      | Not present                      | PHYID OUI discovered on bus      |
  +-------------------------+----------------------------------+----------------------------------+

  Key insight: With phylib, the driver's link management code shrinks to a
  single adjust_link callback. All the complexity of polling, autonegotiation,
  and speed/duplex resolution moves into well-tested kernel infrastructure.
```

---

*Document Number: VNET-DS-PHY-007*
*Part 8 of the PCI Network Driver Curriculum*
