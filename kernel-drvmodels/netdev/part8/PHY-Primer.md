<!-- SPDX-License-Identifier: GPL-2.0 -->
<!-- Copyright (c) 2024 TECH VEDA -->
<!-- Author: Raghu Bharadwaj -->

# PHY Primer -- What Every Driver Developer Needs to Know

Read this before the PHY datasheet. It covers what a PHY is, why it exists,
and the terminology you will encounter when working with phylib in the kernel.

---

## 1. The Two Halves of Every Ethernet Port

An Ethernet NIC is not one chip -- it is two cooperating blocks:

```
         Inside the NIC
  ┌──────────────┐    ┌──────────────┐
  │     MAC      │    │     PHY      │
  │              │    │              │       ┌──────────┐
  │  Framing     │◄──►│  Electrical  │◄─────►│  Cable   │
  │  Buffering   │data│  Signaling   │analog │  (wire)  │
  │  DMA         │path│  Encoding    │signal └──────────┘
  │  Interrupts  │    │  Link Detect │
  │              │    │  Autoneg     │
  └──────┬───────┘    └──────┬───────┘
         │   MDIO bus        │
         │  (management)     │
         └───────────────────┘
```

| Block | What it does | Analogy |
|-------|-------------|---------|
| **MAC** | Assembles/disassembles Ethernet frames, manages DMA rings, raises interrupts, enforces frame sizes | The post office -- sorts and routes mail |
| **PHY** | Converts digital bits to/from electrical signals on the wire, detects link, negotiates speed | The telephone modem -- handles the physical line |

**Why two?** Because the electrical interface to the wire changes (copper,
fiber, different speeds) but the MAC logic stays the same. Splitting them
lets you swap PHY chips without redesigning the MAC.

---

## 2. Glossary of PHY Terms

These terms appear frequently in datasheets, kernel code, and `ethtool` output.

### Physical Layer

| Term | Meaning |
|------|---------|
| **PHY** | Physical layer transceiver. The chip that transmits and receives electrical signals on the Ethernet cable. |
| **MAC** | Media Access Controller. The digital side -- frames, DMA, interrupts. Your driver programs the MAC. |
| **MII** | Media Independent Interface. The standardized data bus between MAC and PHY (IEEE 802.3). 4-bit data path at 25 MHz = 100 Mbps. |
| **GMII** | Gigabit MII. 8-bit data path at 125 MHz = 1000 Mbps. |
| **RGMII** | Reduced GMII. Same speed as GMII but uses fewer pins (4 instead of 8) by clocking on both rising and falling edges. Common on embedded boards. |
| **SGMII** | Serial GMII. Serialized version over a single differential pair. Used when pin count is critical. |

### Management Bus

| Term | Meaning |
|------|---------|
| **MDIO** | Management Data Input/Output. A slow 2-wire serial bus (clock + data) the MAC uses to read/write PHY registers. Think of it as I2C for Ethernet PHYs. |
| **MDC** | MDIO Clock. Driven by the MAC. Typically 2.5 MHz. |
| **Clause 22** | The original MDIO standard. Addresses up to 32 PHYs, each with 32 registers of 16 bits. Used by most simple PHYs. |
| **Clause 45** | Extended MDIO. Adds a "device" layer above register addressing, giving access to thousands of registers. Used by 10G PHYs and complex devices. |

### Link Negotiation

| Term | Meaning |
|------|---------|
| **Autonegotiation (AN)** | Two PHYs exchange capabilities (speed, duplex, pause) via special electrical pulses and agree on the best common mode. Happens every time a cable is plugged in. |
| **Link Partner** | The PHY at the other end of the cable. |
| **Speed** | 10, 100, 1000, 2500, 10000 Mbps etc. Both ends must agree. |
| **Duplex** | Full duplex = simultaneous TX and RX. Half duplex = one direction at a time (obsolete for Gigabit+). |
| **Pause / Flow Control** | PHY can advertise ability to send/receive 802.3x PAUSE frames to throttle the link partner when RX buffers fill up. |

### PHY Registers (Standard MII set)

| Register | Name | One-Line Purpose |
|----------|------|-----------------|
| 0 | **BMCR** | Basic Mode Control -- reset, enable AN, force speed/duplex |
| 1 | **BMSR** | Basic Mode Status -- link up/down, AN complete, capabilities |
| 2-3 | **PHYID1/2** | PHY chip identification (manufacturer OUI + model + revision) |
| 4 | **ANAR** | Our advertisement -- what speeds/duplex we support |
| 5 | **ANLPAR** | Link partner's advertisement -- what they support |

After autonegotiation, the highest speed and best duplex that both ANAR
and ANLPAR agree on is selected automatically.

### PHY States

| State | What the PHY is doing |
|-------|----------------------|
| **Down** | Powered off or held in reset |
| **Ready** | Attached to a driver, waiting for `phy_start()` |
| **AN** | Running autonegotiation -- exchanging capability pulses with the link partner |
| **Running** | Link is up, data is flowing |
| **No Link** | Was running, but link partner disconnected -- will retry AN when cable is restored |
| **Halted** | Driver called `phy_stop()`, state machine frozen |

---

## 3. How Autonegotiation Actually Works

This is the single most important PHY concept. Here is what happens when
you plug in a cable:

```
  Your PHY                                    Remote PHY
  ════════                                    ══════════

  1. Sends Fast Link Pulses (FLPs)  ──────►
     encoding: "I can do 1000/Full,          ◄────── Remote sends its FLPs
     100/Full, 100/Half, 10/Full, 10/Half"           "I can do 100/Full, 10/Full"

  2. Both sides decode each other's FLPs

  3. Highest common capability wins:
     Your PHY: 1000F, 100F, 100H, 10F, 10H
     Remote:          100F,       10F
     ─────────────────────────────────────
     Winner:          100F ◄── 100 Mbps Full Duplex

  4. Both PHYs configure themselves to 100/Full

  5. Link training completes → BMSR.LINK_STATUS = 1

  6. PHY fires adjust_link → driver calls netif_carrier_on()

  Total time: ~2-4 seconds (dominated by link training)
```

**Key point:** The driver never does this manually. `phy_start()` triggers
the whole sequence, and phylib calls `adjust_link()` when it completes.

---

## 4. Why MDIO Exists (and Why It Is Slow)

The data path between MAC and PHY runs at wire speed (125 MHz for Gigabit).
But to *configure* the PHY (set speed, read link status, reset), the MAC
needs a separate management channel. That's MDIO.

```
  Data path:    MAC ◄══════════════════════════════► PHY
                     8-bit parallel @ 125 MHz (GMII)
                     = 1 Gbps of packet data

  Mgmt path:    MAC ◄──────────────────────────────► PHY
                     2-wire serial @ 2.5 MHz (MDIO)
                     = one 16-bit register read per ~1 µs
```

MDIO is intentionally slow -- it only carries configuration, not data.
Reading one PHY register takes about 1 microsecond. This is why phylib
polls link status on a timer (typically 1 Hz) rather than per-packet.

---

## 5. Where the MAC Ends and the PHY Begins

A common source of confusion is which side owns what:

```
  ┌─ MAC Responsibility ───────────────────────────────────────────┐
  │                                                                │
  │  • Frame assembly: preamble + header + payload + FCS           │
  │  • TX/RX queuing and DMA ring management                      │
  │  • Interrupt generation                                        │
  │  • Address filtering (unicast, multicast, promiscuous)         │
  │  • VLAN tag insertion/stripping                                │
  │  • Checksum offload computation (if supported)                 │
  │  • Pause frame generation (802.3x flow control)                │
  │                                                                │
  └────────────────────────────────────────────────────────────────┘

  ┌─ PHY Responsibility ───────────────────────────────────────────┐
  │                                                                │
  │  • Encoding: convert bits to electrical levels (MLT-3, PAM-5)  │
  │  • Clock recovery from incoming signal                         │
  │  • Autonegotiation: exchange capabilities, agree on speed      │
  │  • Link detection: sense signal presence on the wire           │
  │  • Signal quality monitoring (SNR, cable diagnostics)          │
  │  • Power management of the analog front-end                    │
  │  • Electrical isolation (transformer / magnetics)              │
  │                                                                │
  └────────────────────────────────────────────────────────────────┘
```

**Rule of thumb:** if it touches the wire electrically, it is the PHY.
If it touches a DMA buffer or an skb, it is the MAC.

---

## 6. Linux phylib -- The Kernel's PHY Framework

The kernel's PHY library (`drivers/net/phy/`) manages all PHY complexity
so that NIC drivers don't have to. Here is the contract:

### What the Driver Does

| Step | API Call | When |
|------|----------|------|
| Allocate MDIO bus | `mdiobus_alloc()` | probe |
| Provide read/write callbacks | `bus->read = ...` | probe |
| Register bus (scans for PHYs) | `mdiobus_register(bus)` | probe |
| Attach PHY to netdev | `phy_connect(ndev, addr, adjust_link, mode)` | probe |
| Start PHY state machine | `phy_start(phydev)` | open |
| Handle link changes | `adjust_link()` callback | automatic |
| Stop PHY state machine | `phy_stop(phydev)` | stop |
| Disconnect PHY | `phy_disconnect(phydev)` | remove |
| Unregister and free bus | `mdiobus_unregister()` + `mdiobus_free()` | remove |

### What phylib Does For You

- Scans the MDIO bus for PHYs by reading PHYID1/PHYID2
- Matches PHYs to PHY drivers (generic or vendor-specific)
- Runs the autonegotiation state machine
- Polls link status at 1 Hz via a delayed workqueue
- Calls your `adjust_link()` when link state changes
- Provides ethtool helpers (`phy_ethtool_get_link_ksettings`, etc.)
- Handles PHY suspend/resume for power management

### What the Driver Stops Doing

Once phylib is integrated, the driver **no longer**:
- Reads the MAC STATUS register for link state
- Hardcodes speed/duplex in ethtool callbacks
- Manages autonegotiation manually
- Polls for link up/down

---

## 7. Common Interface Modes

When calling `phy_connect()`, you specify a `phy_interface_t` that tells
phylib how the MAC and PHY are wired:

| Mode | Pins | Max Speed | Typical Use |
|------|------|-----------|-------------|
| `PHY_INTERFACE_MODE_MII` | 18 | 100 Mbps | Legacy 10/100 NICs |
| `PHY_INTERFACE_MODE_GMII` | 24 | 1 Gbps | Discrete MAC+PHY boards |
| `PHY_INTERFACE_MODE_RGMII` | 12 | 1 Gbps | Embedded SoCs (fewer pins) |
| `PHY_INTERFACE_MODE_SGMII` | 4 | 1 Gbps | Backplane / SFP modules |

The VNET virtual hardware uses `PHY_INTERFACE_MODE_GMII`.

---

## 8. Reading List

After this primer, proceed to:

1. **`VNET-Controller-PHY-Datasheet.md`** -- our virtual hardware's MDIO
   registers, PHY register map, and driver integration flow.
2. **`vnet_phy.c`** -- the driver source. Search for `mdiobus_alloc`,
   `phy_connect`, and `adjust_link` to see phylib in action.
3. `ethtool eth0` / `ethtool -S eth0` -- run after loading the module to
   see PHY-reported link settings and statistics.

---

*Part 8 of the PCI Network Driver Curriculum*
