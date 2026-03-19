/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 *
 * VNET Virtual Hardware Interface
 *
 * Register map, descriptor format, and constants for the VNET virtual
 * network controller. This is the "datasheet in code" -- drivers include
 * this header and program the hardware using these definitions.
 *
 * See VNET-Controller-Datasheet.md for the full hardware specification.
 */

#ifndef VNET_HW_INTERFACE_H
#define VNET_HW_INTERFACE_H

#include <linux/types.h>
#include <linux/pci.h>

/* ---- Register Offsets (Datasheet Section 2) ---- */

#define VNET_CTRL                0x000
#define VNET_STATUS              0x004
#define VNET_INT_MASK            0x008
#define VNET_INT_STATUS          0x00C
#define VNET_MAC_ADDR_LOW        0x010
#define VNET_MAC_ADDR_HIGH       0x014
#define VNET_MAX_FRAME_REG       0x018

/* Wake-on-LAN / Power Management */
#define VNET_WOL_CTRL            0x040
#define VNET_PM_CTRL             0x048

/* Queue control */
#define VNET_QUEUE_CTRL          0x044
#define VNET_MSIX_CTRL           0x04C

/* ---- TX Ring Registers ---- */
/* Queue 0 */
#define VNET_TX_RING_ADDR        0x100
#define VNET_TX_RING_SIZE        0x104
#define VNET_TX_RING_HEAD        0x108
#define VNET_TX_RING_TAIL        0x10C

/* Per-queue TX ring access macros */
#define VNET_NUM_QUEUES          4
#define VNET_TXQ_ADDR(n)         (0x100 + (n) * 0x10)
#define VNET_TXQ_SIZE(n)         (0x104 + (n) * 0x10)
#define VNET_TXQ_HEAD(n)         (0x108 + (n) * 0x10)
#define VNET_TXQ_TAIL(n)         (0x10C + (n) * 0x10)

/* ---- RX Ring Registers ---- */
/* Queue 0 */
#define VNET_RX_RING_ADDR        0x140
#define VNET_RX_RING_SIZE        0x144
#define VNET_RX_RING_HEAD        0x148
#define VNET_RX_RING_TAIL        0x14C

/* Per-queue RX ring access macros */
#define VNET_RXQ_ADDR(n)         (0x140 + (n) * 0x10)
#define VNET_RXQ_SIZE(n)         (0x144 + (n) * 0x10)
#define VNET_RXQ_HEAD(n)         (0x148 + (n) * 0x10)
#define VNET_RXQ_TAIL(n)         (0x14C + (n) * 0x10)

/* ---- Statistics Registers ---- */
#define VNET_STATS_TX_PACKETS    0x200
#define VNET_STATS_TX_BYTES      0x204
#define VNET_STATS_RX_PACKETS    0x240
#define VNET_STATS_RX_BYTES      0x244

/* ---- MDIO Registers ---- */
#define VNET_MDIO_CTRL           0x300
#define VNET_MDIO_DATA           0x304
#define VNET_MDIO_STATUS         0x308

/* ---- Control Register Bits (Datasheet Section 3.1) ---- */

#define VNET_CTRL_ENABLE         BIT(0)
#define VNET_CTRL_RING_ENABLE    BIT(1)
#define VNET_CTRL_RESET          BIT(4)
#define VNET_CTRL_LOOPBACK       BIT(5)
#define VNET_CTRL_RX_ENABLE      BIT(6)
#define VNET_CTRL_TX_ENABLE      BIT(7)

/* ---- Status Register Bits (Datasheet Section 3.2) ---- */

#define VNET_STATUS_LINK_UP      BIT(5)
#define VNET_STATUS_RX_ACTIVE    BIT(6)
#define VNET_STATUS_TX_ACTIVE    BIT(7)

/* ---- Interrupt Bits (Datasheet Section 3.3, 3.4) ---- */

/* Queue 0 */
#define VNET_INT_ERROR           BIT(0)
#define VNET_INT_LINK_CHANGE     BIT(1)
#define VNET_INT_RX_PACKET       BIT(2)
#define VNET_INT_TX_COMPLETE     BIT(3)

/* Per-queue interrupt bits */
#define VNET_INT_TXQ_COMPLETE(n) BIT(3 + (n))   /* bits 3,4,5,6 */
#define VNET_INT_RXQ_PACKET(n)   BIT(2 + (n) * 0) /* queue 0 only via legacy */
/* For multi-queue: use per-queue bits 8-15 */
#define VNET_INT_TXQ1_COMPLETE   BIT(4)
#define VNET_INT_TXQ2_COMPLETE   BIT(5)
#define VNET_INT_TXQ3_COMPLETE   BIT(6)
#define VNET_INT_MDIO_COMPLETE   BIT(7)

/* All TX complete bits for multi-queue ISR */
#define VNET_INT_TX_ALL          (BIT(3) | BIT(4) | BIT(5) | BIT(6))

/* ---- MDIO Control Register Bits ---- */
#define VNET_MDIO_CTRL_START     BIT(31)
#define VNET_MDIO_CTRL_WRITE     BIT(30)
#define VNET_MDIO_CTRL_READ      0       /* bit 30 clear = read */
#define VNET_MDIO_CTRL_PHY_SHIFT 21
#define VNET_MDIO_CTRL_PHY_MASK  (0x1F << 21)
#define VNET_MDIO_CTRL_REG_SHIFT 16
#define VNET_MDIO_CTRL_REG_MASK  (0x1F << 16)

/* MDIO status */
#define VNET_MDIO_STATUS_BUSY    BIT(0)
#define VNET_MDIO_STATUS_DONE    BIT(1)

/* Virtual PHY address on MDIO bus */
#define VNET_PHY_ADDR            1

/* Virtual PHY ID (OUI-like) */
#define VNET_PHY_ID1             0x1234
#define VNET_PHY_ID2             0x5678

/* ---- Wake-on-LAN Register Bits (offset 0x040) ---- */

#define VNET_WOL_MAGIC           BIT(0)   /* Enable wake on magic packet */
#define VNET_WOL_PATTERN         BIT(1)   /* Enable wake on pattern match */
#define VNET_WOL_LINK            BIT(2)   /* Enable wake on link change */
#define VNET_WOL_PME_EN          BIT(3)   /* Enable PME# assertion */
#define VNET_WOL_MAGIC_ST        BIT(8)   /* Magic packet detected (W1C) */
#define VNET_WOL_PATT_ST         BIT(9)   /* Pattern match detected (W1C) */
#define VNET_WOL_LINK_ST         BIT(10)  /* Link change detected (W1C) */

/* ---- PM Control Register Bits (offset 0x048) ---- */

#define VNET_PM_CTRL_D0          0x00     /* Full power */
#define VNET_PM_CTRL_D3HOT       0x03     /* Minimal power */

/* ---- Queue Control Register Bits ---- */
#define VNET_QCTRL_TXQ_ENABLE(n) BIT(n)       /* bits 0-3: TX queue enable */
#define VNET_QCTRL_RXQ_ENABLE(n) BIT(8 + (n)) /* bits 8-11: RX queue enable */
#define VNET_QCTRL_NUM_QUEUES_SHIFT 16
#define VNET_QCTRL_NUM_QUEUES_MASK  (0xF << 16)

/* ---- Descriptor Flags (Datasheet Section 4.1) ---- */

#define VNET_DESC_TSO            BIT(27)
#define VNET_DESC_CSUM           BIT(28)
#define VNET_DESC_EOP            BIT(29)
#define VNET_DESC_SOP            BIT(30)
#define VNET_DESC_OWN            BIT(31)

/* ---- Descriptor Status (Datasheet Section 4.2) ---- */

#define VNET_DESC_STATUS_OK      BIT(0)
#define VNET_DESC_STATUS_CSUM_OK BIT(1)
#define VNET_DESC_STATUS_CSUM_ERR BIT(2)
#define VNET_DESC_STATUS_LEN_MASK 0xFFFF

/* ---- Hardware Capabilities (Datasheet Section 11) ---- */

#define VNET_FEAT_CSUM_TX        BIT(0)
#define VNET_FEAT_CSUM_RX        BIT(1)
#define VNET_FEAT_NAPI           BIT(2)

/* ---- Hardware Descriptor (DMA descriptor format) ---- */

struct vnet_hw_desc {
	u32 addr;
	u32 len;
	u32 flags;
	u32 status;
};

/* ---- Constants ---- */

#define VNET_MAX_FRAME_SIZE_DEFAULT  1518
#define VNET_RING_SIZE_DEFAULT       256
#define VNET_VENDOR_ID               0x1234
#define VNET_DEVICE_ID               0x5678

/* Maximum MSI-X vectors (per-queue + misc) */
#define VNET_MAX_MSIX_VECTORS    (VNET_NUM_QUEUES + 1)

/* ---- Platform Module Interface ---- */

/*
 * Register mapping functions exported by vnet_hw_platform.ko.
 *
 * In real hardware, the driver would call pci_iomap() to map a BAR.
 * Our virtual device's registers live in kernel RAM, which pci_iomap
 * cannot ioremap. These functions provide the same result -- a pointer
 * suitable for ioread32()/iowrite32().
 *
 * Usage in driver probe():
 *   priv->regs = vnet_hw_map_bar0(pdev);
 *   // equivalent to: priv->regs = pci_iomap(pdev, 0, 0);
 *
 * Usage in driver remove():
 *   vnet_hw_unmap_bar0(pdev);
 *   // equivalent to: pci_iounmap(pdev, priv->regs);
 */
extern void __iomem *vnet_hw_map_bar0(struct pci_dev *pdev);
extern void vnet_hw_unmap_bar0(struct pci_dev *pdev);

/*
 * TX ring registration -- tells the hardware where to find descriptors.
 *
 * In real hardware, the NIC reads descriptors via DMA from the address
 * written to the TX_RING_ADDR register. Our simulator needs the kernel
 * virtual address to walk descriptors directly.
 *
 * Call this in open() after allocating the TX ring and writing the DMA
 * address to the TX_RING_ADDR register.
 */
extern void vnet_hw_set_tx_ring(struct pci_dev *pdev,
				struct vnet_hw_desc *ring_va, u32 count);
extern void vnet_hw_clear_tx_ring(struct pci_dev *pdev);

/*
 * Simple register-based TX mode (Part 4).
 *
 * For drivers without TX descriptor rings. The driver:
 *   1. Writes DMA address to VNET_TX_RING_ADDR register
 *   2. Writes packet length to VNET_TX_RING_SIZE register
 *   3. Writes 1 to VNET_TX_RING_HEAD as doorbell
 * The platform fires TX_COMPLETE when the "transmission" is done.
 *
 * Call start in open(), stop in stop().
 */
extern void vnet_hw_start_simple_tx(struct pci_dev *pdev);
extern void vnet_hw_stop_simple_tx(struct pci_dev *pdev);

/*
 * RX ring registration -- tells the hardware where to deliver packets.
 *
 * In real hardware, the NIC reads RX buffer addresses from descriptors
 * and DMA-writes received packet data into those buffers. Our simulator
 * needs the kernel virtual addresses of both the descriptor ring and
 * the RX buffers to write directly.
 *
 * Parameters:
 *   ring_va  - kernel VA of the RX descriptor ring
 *   count    - number of descriptors in the ring
 *   bufs_va  - array of kernel VAs, one per RX buffer slot
 *   buf_size - size of each RX buffer in bytes
 *
 * Call this in open() after allocating the RX ring and buffers.
 * The platform will generate synthetic packets into these buffers
 * and fire VNET_INT_RX_PACKET interrupts.
 */
extern void vnet_hw_set_rx_ring(struct pci_dev *pdev,
				struct vnet_hw_desc *ring_va, u32 count,
				void **bufs_va, u32 buf_size);
extern void vnet_hw_clear_rx_ring(struct pci_dev *pdev);

/*
 * Per-queue TX ring registration.
 * Same as above but for a specific queue index (0-3).
 */
extern void vnet_hw_set_tx_ring_queue(struct pci_dev *pdev, int queue,
				      struct vnet_hw_desc *ring_va, u32 count);
extern void vnet_hw_clear_tx_ring_queue(struct pci_dev *pdev, int queue);

/*
 * MSI-X vector access.
 * Returns the IRQ number for a given vector index.
 * Vector 0-3: per-queue, Vector 4: misc (link/error).
 */
extern int vnet_hw_get_msix_vector(struct pci_dev *pdev, int vector);
extern int vnet_hw_get_num_msix_vectors(struct pci_dev *pdev);

/*
 * MDIO bus access.
 * Read/write PHY registers via MDIO bus emulation.
 * These are called by the driver's mdio_read/mdio_write callbacks.
 */
extern int vnet_hw_mdio_read(struct pci_dev *pdev, int phy_addr, int reg);
extern int vnet_hw_mdio_write(struct pci_dev *pdev, int phy_addr,
			      int reg, u16 val);

#endif /* VNET_HW_INTERFACE_H */
