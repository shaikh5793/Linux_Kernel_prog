<!-- SPDX-License-Identifier: GPL-2.0 -->
<!-- Copyright (c) 2024 TECH VEDA -->
<!-- Author: Raghu Bharadwaj -->

# VNET Virtual Ethernet Controller - Part 3 Datasheet
## Net Device Ops & Register Interface

---

## 1. Overview

Part 3 focuses on the `net_device_ops` callback table and the register
interface needed to implement each operation. This datasheet covers only the
registers used by the ops -- CTRL, STATUS, MAC address, and MAX_FRAME.

Interrupt registers (INT_MASK, INT_STATUS) and DMA ring registers are
documented in their respective datasheets.

---

## 2. CTRL Register (Offset 0x000, R/W)

The control register enables the controller and its TX/RX data paths.

```
 31                               8  7   6   5   4   3  2  1   0
┌──────────────────────────────────┬───┬───┬───┬───┬──┬──┬───┬───┐
│          Reserved (0)            │TX │RX │LB │RST│  │  │RNG│EN │
│                                  │EN │EN │   │   │  │  │EN │   │
└──────────────────────────────────┴───┴───┴───┴───┴──┴──┴───┴───┘
```

| Bit | Name | Description |
|-----|------|-------------|
| 0 | ENABLE | Global controller enable. Must be set for any operation. |
| 1 | RING_ENABLE | Enable descriptor ring DMA (Part 4+). |
| 4 | RESET | Software reset. Self-clears when complete. |
| 5 | LOOPBACK | Enable loopback mode (TX data loops to RX). |
| 6 | RX_ENABLE | Enable the receive data path. |
| 7 | TX_ENABLE | Enable the transmit data path. |

### 2.1 Usage in ndo_open

```c
/* Enable controller with TX and RX paths */
iowrite32(VNET_CTRL_ENABLE | VNET_CTRL_TX_ENABLE |
          VNET_CTRL_RX_ENABLE, priv->regs + VNET_CTRL);
```

### 2.2 Usage in ndo_stop

```c
/* Disable entire controller */
iowrite32(0, priv->regs + VNET_CTRL);
```

### 2.3 Usage in ndo_set_rx_mode

The CTRL register is read-modify-written to preserve enable bits while
adjusting filter settings (in a real controller, additional filter bits
would live here or in a separate filter register).

---

## 3. STATUS Register (Offset 0x004, Read-Only)

```
 31                               8   7     6     5    4         0
┌──────────────────────────────────┬─────┬─────┬─────┬───────────┐
│          Reserved (0)            │TX   │RX   │LINK │ Reserved  │
│                                  │ACT  │ACT  │UP   │           │
└──────────────────────────────────┴─────┴─────┴─────┴───────────┘
```

| Bit | Name | Description |
|-----|------|-------------|
| 5 | LINK_UP | 1 = link is up, 0 = link is down |
| 6 | RX_ACTIVE | 1 = RX path is active |
| 7 | TX_ACTIVE | 1 = TX path is active |

### 3.1 Usage in ndo_open

```c
/* Check link status after enabling hardware */
if (ioread32(priv->regs + VNET_STATUS) & VNET_STATUS_LINK_UP)
    netif_carrier_on(ndev);
else
    netif_carrier_off(ndev);
```

---

## 4. MAC Address Registers

A 6-byte MAC address is split across two 32-bit registers.

### 4.1 MAC_ADDR_LOW (Offset 0x010, R/W)

```
 31       24  23       16  15        8  7         0
┌───────────┬───────────┬───────────┬───────────┐
│  Byte 3   │  Byte 2   │  Byte 1   │  Byte 0   │
│  addr[3]  │  addr[2]  │  addr[1]  │  addr[0]  │
└───────────┴───────────┴───────────┴───────────┘
```

### 4.2 MAC_ADDR_HIGH (Offset 0x014, R/W)

```
 31                    16  15        8  7         0
┌────────────────────────┬───────────┬───────────┐
│     Reserved (0)       │  Byte 5   │  Byte 4   │
│                        │  addr[5]  │  addr[4]  │
└────────────────────────┴───────────┴───────────┘
```

### 4.3 Writing the MAC Address (ndo_set_mac_address)

```c
static void vnet_write_mac_to_hw(void __iomem *regs, const u8 *addr)
{
    iowrite32(addr[0] | (addr[1] << 8) |
              (addr[2] << 16) | (addr[3] << 24),
              regs + VNET_MAC_ADDR_LOW);
    iowrite32(addr[4] | (addr[5] << 8),
              regs + VNET_MAC_ADDR_HIGH);
}
```

### 4.4 MAC Address Validation (ndo_validate_addr)

Before `ndo_open`, the kernel calls `ndo_validate_addr` to ensure the
MAC address is valid. The driver checks `is_valid_ether_addr(dev->dev_addr)`
which rejects:
- All-zeros addresses
- Multicast bit set (bit 0 of byte 0)
- Broadcast address (FF:FF:FF:FF:FF:FF)

---

## 5. MAX_FRAME Register (Offset 0x018, R/W)

Controls the maximum Ethernet frame size the hardware will accept or transmit.

```
 31                    16  15                     0
┌────────────────────────┬────────────────────────┐
│     Reserved (0)       │   Max Frame Size       │
│                        │   (bytes, 14-bit)      │
└────────────────────────┴────────────────────────┘
```

Default value: 1518 (standard Ethernet: 1500 MTU + 14 header + 4 FCS)

### 5.1 Usage in ndo_change_mtu

```c
u32 max_frame = new_mtu + ETH_HLEN + ETH_FCS_LEN;
iowrite32(max_frame, priv->regs + VNET_MAX_FRAME_REG);
```

### 5.2 MTU Limits

The driver sets `ndev->min_mtu` and `ndev->max_mtu` in probe. The stack
enforces these limits before calling `ndo_change_mtu`, so the driver does
not need to re-validate the range:

| Limit | Value | Frame Size |
|-------|-------|------------|
| Minimum MTU | 68 | 86 bytes |
| Default MTU | 1500 | 1518 bytes |
| Maximum MTU | 9000 | 9018 bytes (jumbo) |

---

## 6. Register Quick Reference (Part 3 Scope)

| Offset | Name | Type | Reset | Part 3 Usage |
|--------|------|------|-------|--------------|
| 0x000 | CTRL | R/W | 0x00000000 | ndo_open: enable TX+RX; ndo_stop: disable; ndo_set_rx_mode: filter |
| 0x004 | STATUS | R | 0x00000000 | ndo_open: check link state (bit 5) |
| 0x010 | MAC_ADDR_LOW | R/W | 0x00000000 | ndo_set_mac_address: write bytes 0-3; probe: initial MAC |
| 0x014 | MAC_ADDR_HIGH | R/W | 0x00000000 | ndo_set_mac_address: write bytes 4-5; probe: initial MAC |
| 0x018 | MAX_FRAME_REG | R/W | 0x000005EE | ndo_change_mtu: set max frame size; probe: initial frame size |

### Registers NOT Used in Part 3

| Offset | Name | Purpose |
|--------|------|---------|
| 0x008 | INT_MASK | Interrupt mask |
| 0x00C | INT_STATUS | Interrupt status |
| 0x100-0x10C | TX Ring | DMA TX registers |
| 0x140-0x14C | RX Ring | DMA RX ring registers |

---

## 7. Net Device Ops and Register Access Map

This table maps each ndo callback to the registers it touches:

| ndo Callback | Registers Accessed | Access Pattern |
|--------------|-------------------|----------------|
| `ndo_open` | CTRL (W), STATUS (R) | Write enable bits, read link |
| `ndo_stop` | CTRL (W) | Write 0 to disable |
| `ndo_start_xmit` | None | Stats only (no HW TX in Part 3) |
| `ndo_get_stats64` | None | Software counters only |
| `ndo_set_mac_address` | MAC_ADDR_LOW (W), MAC_ADDR_HIGH (W) | Write 6-byte MAC |
| `ndo_change_mtu` | MAX_FRAME_REG (W) | Write new frame size |
| `ndo_validate_addr` | None | Software validation only |
| `ndo_tx_timeout` | None | Log and wake queue |
| `ndo_set_rx_mode` | CTRL (R/W) | Read-modify-write filter bits |

---

*Document: VNET-DS-PART3-001 | Net Device Ops & Register Interface*
