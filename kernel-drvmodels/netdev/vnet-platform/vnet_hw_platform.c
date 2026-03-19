// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 *
 * VNET Virtual PCI Hardware Platform
 *
 * This module IS the hardware. It creates a virtual PCI device on a
 * virtual PCI bus, with an emulated register file, TX completion engine,
 * and software interrupt. Driver modules (Parts 2-11) bind to this device
 * using standard PCI driver APIs (module_pci_driver, probe/remove).
 *
 * LOAD ORDER: Load this module first, then any Part 2-11 driver.
 *   sudo insmod vnet_hw_platform.ko
 *   sudo insmod ../part2/vnet_skeleton.ko
 *
 * Components:
 *   - Virtual PCI host bridge with config space emulation
 *   - Register file (256 x 32-bit) for ioread32/iowrite32 access
 *   - Software IRQs allocated via irq_alloc_descs()
 *   - Per-queue TX completion engines (poll descriptors, fire interrupts)
 *   - Virtual PHY with MDIO bus emulation
 *   - MSI-X vector support (multiple IRQ lines)
 *
 * What real hardware does differently:
 *   - regs[] array       -> MMIO-mapped silicon registers on a PCIe BAR
 *   - delayed_work poll   -> DMA engine with hardware timer
 *   - generic_handle_irq  -> MSI/MSI-X from PCIe bus
 *   - pci_alloc_host_bridge -> physical PCIe root complex
 *
 * Students: This is the "black box". Read it with the datasheet open
 * alongside.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/irq.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/phy.h>

#ifdef CONFIG_X86
#include <asm/pci.h>
#endif

#include "vnet_hw_interface.h"

#define DRV_NAME "vnet_hw_platform"

/* ================================================================
 * Section 1: Hardware Internal State
 * ================================================================
 */

/* Per-queue TX completion engine state */
struct vnet_txq_state {
	struct vnet_hw_desc *ring_va;	/* kernel VA of descriptor ring */
	u32 ring_count;			/* number of descriptors */
	u32 sim_tail;			/* hardware's internal read pointer */
	struct delayed_work poll_work;	/* periodic TX poll */
	int queue_idx;			/* queue index (for register offset) */
};

struct vnet_hw_device {
	/*
	 * PCI config space -- 256 bytes, Type 0 header.
	 * The PCI core reads/writes this during device enumeration
	 * and when drivers call pci_enable_device(), pci_set_master(), etc.
	 */
	u8 config[256];

	/*
	 * Register file -- 256 x 32-bit registers.
	 * Drivers access these via ioread32/iowrite32 on the pointer
	 * returned by vnet_hw_map_bar0().
	 */
	u32 regs[256];

	/*
	 * Per-queue TX completion engines.
	 * Queue 0 is the default. Queues 1-3 are for multi-queue.
	 */
	struct vnet_txq_state txq[VNET_NUM_QUEUES];

	/* Legacy single-queue pointers (for backward compat exports) */
	struct vnet_hw_desc *tx_ring_va;
	u32 tx_ring_count;
	u32 tx_sim_tail;
	struct delayed_work tx_poll_work;
	bool simple_tx_mode;		/* true = register-based TX (Part 4) */

	/*
	 * RX simulation engine.
	 * Generates synthetic packets into the driver's RX ring buffers
	 * and fires VNET_INT_RX_PACKET interrupts. This simulates packets
	 * arriving from the network.
	 */
	struct vnet_hw_desc *rx_ring_va;
	u32 rx_ring_count;
	void **rx_bufs_va;		/* kernel VAs of RX buffers */
	u32 rx_buf_size;
	u32 rx_sim_head;		/* next RX slot to fill */
	struct delayed_work rx_poll_work;
	u32 rx_seq;			/* packet sequence number */

	/*
	 * Virtual PHY registers.
	 * Standard MII registers 0-31, 16 bits each.
	 */
	u16 phy_regs[32];

	/*
	 * Software IRQs for interrupt delivery.
	 * irqs[0]: legacy IRQ (pdev->irq)
	 * irqs[1-4]: per-queue MSI-X vectors
	 */
	int irqs[VNET_MAX_MSIX_VECTORS];
	int num_irqs;

	/* PCI infrastructure */
	struct pci_host_bridge *bridge;
	struct pci_dev *pdev;

#ifdef CONFIG_X86
	struct pci_sysdata sysdata;
#endif
};

/* Single global device instance (one virtual NIC) */
static struct vnet_hw_device *vnet_hw_dev;

/* ================================================================
 * Section 2: PCI Config Space Emulation
 * ================================================================
 */
static void vnet_init_config_space(struct vnet_hw_device *dev)
{
	memset(dev->config, 0, sizeof(dev->config));

	/* Vendor ID / Device ID */
	*(u16 *)&dev->config[PCI_VENDOR_ID] = cpu_to_le16(VNET_VENDOR_ID);
	*(u16 *)&dev->config[PCI_DEVICE_ID] = cpu_to_le16(VNET_DEVICE_ID);

	/* Command: start with memory space and bus master disabled */
	*(u16 *)&dev->config[PCI_COMMAND] = 0;

	/* Status: fast back-to-back capable */
	*(u16 *)&dev->config[PCI_STATUS] = cpu_to_le16(PCI_STATUS_FAST_BACK);

	/* Revision 0x01, Class: Ethernet controller (0x020000) */
	dev->config[PCI_REVISION_ID] = 0x01;
	dev->config[PCI_CLASS_PROG] = 0x00;
	dev->config[PCI_CLASS_DEVICE] = 0x00;
	dev->config[PCI_CLASS_DEVICE + 1] = 0x02;

	/* Header type 0 (standard endpoint), single-function */
	dev->config[PCI_HEADER_TYPE] = PCI_HEADER_TYPE_NORMAL;

	/* Subsystem IDs */
	*(u16 *)&dev->config[PCI_SUBSYSTEM_VENDOR_ID] =
		cpu_to_le16(VNET_VENDOR_ID);
	*(u16 *)&dev->config[PCI_SUBSYSTEM_ID] =
		cpu_to_le16(VNET_DEVICE_ID);

	/* Interrupt pin: INTA# */
	dev->config[PCI_INTERRUPT_PIN] = 1;
}

static int vnet_pci_read(struct pci_bus *bus, unsigned int devfn,
			 int where, int size, u32 *val)
{
	struct vnet_hw_device *dev = vnet_hw_dev;

	if (PCI_SLOT(devfn) != 0 || PCI_FUNC(devfn) != 0) {
		*val = ~0;
		return PCIBIOS_DEVICE_NOT_FOUND;
	}

	if (where + size > 256) {
		*val = ~0;
		return PCIBIOS_BAD_REGISTER_NUMBER;
	}

	*val = 0;
	memcpy(val, &dev->config[where], size);
	return PCIBIOS_SUCCESSFUL;
}

static int vnet_pci_write(struct pci_bus *bus, unsigned int devfn,
			  int where, int size, u32 val)
{
	struct vnet_hw_device *dev = vnet_hw_dev;

	if (PCI_SLOT(devfn) != 0 || PCI_FUNC(devfn) != 0)
		return PCIBIOS_DEVICE_NOT_FOUND;

	if (where + size > 256)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	switch (where) {
	case PCI_COMMAND:
	case PCI_COMMAND + 1:
	case PCI_STATUS:
	case PCI_STATUS + 1:
	case PCI_CACHE_LINE_SIZE:
	case PCI_LATENCY_TIMER:
	case PCI_INTERRUPT_LINE:
		memcpy(&dev->config[where], &val, size);
		break;
	}

	return PCIBIOS_SUCCESSFUL;
}

static struct pci_ops vnet_pci_ops = {
	.read  = vnet_pci_read,
	.write = vnet_pci_write,
};

/* ================================================================
 * Section 3: Interrupt Controller
 * ================================================================
 *
 * INT_MASK register: bit = 1 means DISABLED (masked).
 * INT_STATUS register: bit = 1 means PENDING.
 * An interrupt fires when: INT_STATUS & ~INT_MASK != 0
 */
static int vnet_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	return vnet_hw_dev->irqs[0];
}

/*
 * Fire interrupt on the legacy IRQ line (used by Parts 2-9).
 */
static void vnet_fire_interrupt(struct vnet_hw_device *dev, u32 bits)
{
	u32 mask = dev->regs[VNET_INT_MASK / 4];
	unsigned long flags;

	dev->regs[VNET_INT_STATUS / 4] |= bits;

	if (!(bits & ~mask))
		return;

	local_irq_save(flags);
	generic_handle_irq(dev->irqs[0]);
	local_irq_restore(flags);
}

/*
 * Fire interrupt on a specific MSI-X vector.
 * Vector 0-3: per-queue, Vector 4: misc (link/error).
 */
static void vnet_fire_msix_vector(struct vnet_hw_device *dev, int vector,
				  u32 status_bits)
{
	unsigned long flags;

	dev->regs[VNET_INT_STATUS / 4] |= status_bits;

	if (vector < 0 || vector >= dev->num_irqs)
		return;

	local_irq_save(flags);
	generic_handle_irq(dev->irqs[vector]);
	local_irq_restore(flags);
}

/* ================================================================
 * Section 4: TX Completion Engines
 * ================================================================
 *
 * Per-queue TX polling. Each queue has its own delayed_work that
 * checks for new descriptors every ~100us.
 */

/*
 * Legacy single-queue TX poll.
 *
 * Supports two modes:
 *
 * Ring mode (Part 4+): walks descriptor ring via tx_ring_va, processes
 * OWN'd descriptors, fires TX_COMPLETE when done.
 *
 * Simple mode (Part 4): no descriptor ring. The driver writes a DMA
 * address and length directly to TX registers and rings the doorbell
 * (TX_RING_HEAD). The platform detects the doorbell, increments stats,
 * resets head, and fires TX_COMPLETE. The driver's ISR then unmaps the
 * streaming DMA mapping and frees the skb.
 */
static void vnet_tx_poll(struct work_struct *work)
{
	struct vnet_hw_device *dev = container_of(work, struct vnet_hw_device,
						  tx_poll_work.work);
	u32 head;
	bool completed = false;

	if (!(dev->regs[VNET_CTRL / 4] & VNET_CTRL_ENABLE))
		goto resched;

	if (dev->tx_ring_va) {
		/* Ring-based TX completion (Part 4+) */
		head = dev->regs[VNET_TX_RING_HEAD / 4];

		while (dev->tx_sim_tail != head) {
			struct vnet_hw_desc *desc =
				&dev->tx_ring_va[dev->tx_sim_tail];

			if (!(desc->flags & VNET_DESC_OWN))
				break;

			desc->flags &= ~VNET_DESC_OWN;
			desc->status = VNET_DESC_STATUS_OK |
				(desc->len & VNET_DESC_STATUS_LEN_MASK);
			if (desc->flags & VNET_DESC_CSUM)
				desc->status |= VNET_DESC_STATUS_CSUM_OK;

			dev->regs[VNET_STATS_TX_PACKETS / 4]++;
			dev->regs[VNET_STATS_TX_BYTES / 4] +=
				desc->len & VNET_DESC_STATUS_LEN_MASK;

			dev->tx_sim_tail =
				(dev->tx_sim_tail + 1) % dev->tx_ring_count;
			completed = true;
		}

		dev->regs[VNET_TX_RING_TAIL / 4] = dev->tx_sim_tail;
	} else if (dev->simple_tx_mode) {
		/*
		 * Simple register-based TX completion (Part 4).
		 *
		 * The driver wrote a DMA address to TX_RING_ADDR and
		 * length to TX_RING_SIZE, then set TX_RING_HEAD = 1
		 * as a doorbell. We "transmit" by updating stats,
		 * resetting head to 0, and firing TX_COMPLETE.
		 *
		 * In real hardware without ring support, the NIC would
		 * read the address/length from registers, DMA the packet
		 * out the wire, then signal completion.
		 */
		head = dev->regs[VNET_TX_RING_HEAD / 4];
		if (head != 0) {
			u32 len = dev->regs[VNET_TX_RING_SIZE / 4];

			dev->regs[VNET_STATS_TX_PACKETS / 4]++;
			dev->regs[VNET_STATS_TX_BYTES / 4] += len;
			dev->regs[VNET_TX_RING_HEAD / 4] = 0;
			completed = true;
		}
	}

	if (completed)
		vnet_fire_interrupt(dev, VNET_INT_TX_COMPLETE);

resched:
	if (dev->tx_ring_va || dev->simple_tx_mode)
		schedule_delayed_work(&dev->tx_poll_work,
				      usecs_to_jiffies(100));
}

/* ================================================================
 * Section 4b: RX Simulation Engine
 * ================================================================
 *
 * Generates synthetic Ethernet frames into the driver's RX ring.
 * Each frame is a minimal valid Ethernet frame with incrementing
 * sequence numbers. The driver processes these exactly as it would
 * process real packets from the wire.
 *
 * In real hardware, the NIC's RX DMA engine would:
 *   1. Receive a frame from the PHY
 *   2. Read the next RX descriptor to find a buffer address
 *   3. DMA the frame data into the buffer
 *   4. Write back descriptor status (OK, length, checksum)
 *   5. Clear the OWN flag (give descriptor back to driver)
 *   6. Fire RX_PACKET interrupt
 *
 * We simulate steps 2-6 using a delayed_work.
 */
static void vnet_rx_poll(struct work_struct *work)
{
	struct vnet_hw_device *dev = container_of(work, struct vnet_hw_device,
						  rx_poll_work.work);
	struct vnet_hw_desc *desc;
	void *buf;
	u8 *pkt;
	u32 pkt_len;
	u32 mac_lo, mac_hi;

	if (!(dev->regs[VNET_CTRL / 4] & VNET_CTRL_ENABLE))
		goto resched;
	if (!(dev->regs[VNET_CTRL / 4] & VNET_CTRL_RX_ENABLE))
		goto resched;
	if (!dev->rx_ring_va || !dev->rx_bufs_va)
		goto resched;

	desc = &dev->rx_ring_va[dev->rx_sim_head];

	/* Only fill descriptors the driver has posted (OWN = driver gave to HW) */
	if (!(desc->flags & VNET_DESC_OWN))
		goto resched;

	buf = dev->rx_bufs_va[dev->rx_sim_head];
	if (!buf)
		goto resched;

	/*
	 * Build a minimal Ethernet frame (46 bytes minimum payload).
	 * Dest MAC = driver's programmed MAC address
	 * Src MAC  = 02:00:00:00:00:01 (virtual peer)
	 * EtherType = 0x0800 (IPv4, though payload is synthetic)
	 */
	pkt = (u8 *)buf;
	pkt_len = 60; /* minimum Ethernet frame */

	memset(pkt, 0, pkt_len);

	/* Destination MAC: read from driver's programmed MAC registers */
	mac_lo = dev->regs[VNET_MAC_ADDR_LOW / 4];
	mac_hi = dev->regs[VNET_MAC_ADDR_HIGH / 4];
	pkt[0] = mac_lo & 0xFF;
	pkt[1] = (mac_lo >> 8) & 0xFF;
	pkt[2] = (mac_lo >> 16) & 0xFF;
	pkt[3] = (mac_lo >> 24) & 0xFF;
	pkt[4] = mac_hi & 0xFF;
	pkt[5] = (mac_hi >> 8) & 0xFF;

	/* Source MAC: virtual peer 02:00:00:00:00:01 */
	pkt[6]  = 0x02;
	pkt[7]  = 0x00;
	pkt[8]  = 0x00;
	pkt[9]  = 0x00;
	pkt[10] = 0x00;
	pkt[11] = 0x01;

	/* EtherType: 0x9000 (Ethernet loopback/test) */
	pkt[12] = 0x90;
	pkt[13] = 0x00;

	/* Payload: sequence number for visibility */
	pkt[14] = (dev->rx_seq >> 24) & 0xFF;
	pkt[15] = (dev->rx_seq >> 16) & 0xFF;
	pkt[16] = (dev->rx_seq >> 8) & 0xFF;
	pkt[17] = dev->rx_seq & 0xFF;
	dev->rx_seq++;

	/* Complete the descriptor: clear OWN, set status */
	desc->flags &= ~VNET_DESC_OWN;
	desc->status = VNET_DESC_STATUS_OK |
		       (pkt_len & VNET_DESC_STATUS_LEN_MASK);
	desc->len = pkt_len;

	/* Update RX stats */
	dev->regs[VNET_STATS_RX_PACKETS / 4]++;
	dev->regs[VNET_STATS_RX_BYTES / 4] += pkt_len;

	/* Advance head (hardware's write pointer) */
	dev->rx_sim_head = (dev->rx_sim_head + 1) % dev->rx_ring_count;
	dev->regs[VNET_RX_RING_HEAD / 4] = dev->rx_sim_head;

	/* Fire RX interrupt */
	vnet_fire_interrupt(dev, VNET_INT_RX_PACKET);

resched:
	if (dev->rx_ring_va)
		schedule_delayed_work(&dev->rx_poll_work,
				      msecs_to_jiffies(200));
}

/* Per-queue TX poll (used via vnet_hw_set_tx_ring_queue) */
static void vnet_txq_poll(struct work_struct *work)
{
	struct vnet_txq_state *txq = container_of(work,
						  struct vnet_txq_state,
						  poll_work.work);
	struct vnet_hw_device *dev = vnet_hw_dev;
	int q = txq->queue_idx;
	u32 head_reg = VNET_TXQ_HEAD(q) / 4;
	u32 tail_reg = VNET_TXQ_TAIL(q) / 4;
	u32 head;
	bool completed = false;
	u32 int_bit;
	bool use_msix;

	if (!(dev->regs[VNET_CTRL / 4] & VNET_CTRL_ENABLE))
		goto resched;
	if (!txq->ring_va)
		goto resched;

	head = dev->regs[head_reg];

	while (txq->sim_tail != head) {
		struct vnet_hw_desc *desc = &txq->ring_va[txq->sim_tail];

		if (!(desc->flags & VNET_DESC_OWN))
			break;

		desc->flags &= ~VNET_DESC_OWN;
		desc->status = VNET_DESC_STATUS_OK |
			       (desc->len & VNET_DESC_STATUS_LEN_MASK);
		/* Simulate checksum offload: set CSUM_OK if requested */
		if (desc->flags & VNET_DESC_CSUM)
			desc->status |= VNET_DESC_STATUS_CSUM_OK;

		dev->regs[VNET_STATS_TX_PACKETS / 4]++;
		dev->regs[VNET_STATS_TX_BYTES / 4] +=
			desc->len & VNET_DESC_STATUS_LEN_MASK;

		txq->sim_tail = (txq->sim_tail + 1) % txq->ring_count;
		completed = true;
	}

	dev->regs[tail_reg] = txq->sim_tail;

	if (completed) {
		/* Determine per-queue interrupt bit */
		int_bit = VNET_INT_TXQ_COMPLETE(q);

		/*
		 * Check if MSI-X mode is enabled. If so, fire per-queue
		 * vector. Otherwise fire legacy IRQ with per-queue status.
		 */
		use_msix = !!(dev->regs[VNET_MSIX_CTRL / 4] & BIT(0));
		if (use_msix && q < dev->num_irqs)
			vnet_fire_msix_vector(dev, q, int_bit);
		else
			vnet_fire_interrupt(dev, int_bit);
	}

resched:
	if (txq->ring_va)
		schedule_delayed_work(&txq->poll_work,
				      usecs_to_jiffies(100));
}

/* ================================================================
 * Section 5: Virtual PHY / MDIO Emulation
 * ================================================================
 *
 * Emulates a Clause 22 MDIO bus with a single virtual PHY.
 * The PHY reports 1 Gbps full-duplex, link always up.
 *
 * Standard MII registers:
 *   Reg 0: BMCR  (Basic Mode Control)
 *   Reg 1: BMSR  (Basic Mode Status)
 *   Reg 2: PHYID1 (PHY Identifier 1)
 *   Reg 3: PHYID2 (PHY Identifier 2)
 *   Reg 4: ANAR  (Autoneg Advertisement)
 *   Reg 5: ANLPAR (Autoneg Link Partner Ability)
 */
static void vnet_init_phy(struct vnet_hw_device *dev)
{
	memset(dev->phy_regs, 0, sizeof(dev->phy_regs));

	/* BMCR: autoneg enabled, 1000 Mbps */
	dev->phy_regs[MII_BMCR] = BMCR_ANENABLE | BMCR_SPEED1000 |
				   BMCR_FULLDPLX;

	/* BMSR: link up, autoneg complete, capabilities */
	dev->phy_regs[MII_BMSR] = BMSR_LSTATUS | BMSR_ANEGCOMPLETE |
				   BMSR_ANEGCAPABLE |
				   BMSR_100FULL | BMSR_100HALF |
				   BMSR_10FULL | BMSR_10HALF;

	/* PHY ID registers */
	dev->phy_regs[MII_PHYSID1] = VNET_PHY_ID1;
	dev->phy_regs[MII_PHYSID2] = VNET_PHY_ID2;

	/* Autoneg advertisement: 10/100/1000 full/half */
	dev->phy_regs[MII_ADVERTISE] = ADVERTISE_ALL | ADVERTISE_CSMA;

	/* Link partner ability: mirrors advertisement (back-to-back) */
	dev->phy_regs[MII_LPA] = LPA_100FULL | LPA_100HALF |
				  LPA_10FULL | LPA_10HALF | LPA_LPACK;

	/* Extended status: 1000baseT capable */
	dev->phy_regs[MII_STAT1000] = 0;
	dev->phy_regs[MII_CTRL1000] = ADVERTISE_1000FULL;

	/* MDIO status: ready */
	dev->regs[VNET_MDIO_STATUS / 4] = VNET_MDIO_STATUS_DONE;
}

/* ================================================================
 * Section 6: Register File Initialization
 * ================================================================
 */
static void vnet_init_regs(struct vnet_hw_device *dev)
{
	memset(dev->regs, 0, sizeof(dev->regs));

	dev->regs[VNET_STATUS / 4] = VNET_STATUS_LINK_UP |
				      VNET_STATUS_TX_ACTIVE |
				      VNET_STATUS_RX_ACTIVE;

	dev->regs[VNET_MAX_FRAME_REG / 4] = VNET_MAX_FRAME_SIZE_DEFAULT;

	/* All interrupts masked (disabled) until driver enables them */
	dev->regs[VNET_INT_MASK / 4] = ~0U;

	/* Queue control: report 4 queues available */
	dev->regs[VNET_QUEUE_CTRL / 4] =
		(VNET_NUM_QUEUES << VNET_QCTRL_NUM_QUEUES_SHIFT);

	dev->tx_sim_tail = 0;
}

/* ================================================================
 * Section 7: Exported Platform Functions
 * ================================================================
 */

void __iomem *vnet_hw_map_bar0(struct pci_dev *pdev)
{
	if (!vnet_hw_dev)
		return NULL;
	return (void __iomem *)vnet_hw_dev->regs;
}
EXPORT_SYMBOL_GPL(vnet_hw_map_bar0);

void vnet_hw_unmap_bar0(struct pci_dev *pdev)
{
}
EXPORT_SYMBOL_GPL(vnet_hw_unmap_bar0);

/*
 * Legacy single-queue TX ring setup (Parts 2-8).
 * Sets up queue 0 using the legacy delayed_work.
 */
void vnet_hw_set_tx_ring(struct pci_dev *pdev,
			 struct vnet_hw_desc *ring_va, u32 count)
{
	if (!vnet_hw_dev)
		return;

	vnet_hw_dev->tx_ring_va = ring_va;
	vnet_hw_dev->tx_ring_count = count;
	vnet_hw_dev->tx_sim_tail = 0;

	schedule_delayed_work(&vnet_hw_dev->tx_poll_work,
			      usecs_to_jiffies(100));
}
EXPORT_SYMBOL_GPL(vnet_hw_set_tx_ring);

void vnet_hw_clear_tx_ring(struct pci_dev *pdev)
{
	if (!vnet_hw_dev)
		return;

	cancel_delayed_work_sync(&vnet_hw_dev->tx_poll_work);
	vnet_hw_dev->tx_ring_va = NULL;
	vnet_hw_dev->tx_ring_count = 0;
}
EXPORT_SYMBOL_GPL(vnet_hw_clear_tx_ring);

/*
 * Simple register-based TX mode (Part 4).
 *
 * Starts the TX poll engine without a descriptor ring. The driver
 * writes a DMA address and length to TX registers, then rings the
 * doorbell (TX_RING_HEAD = 1). The platform detects this, updates
 * stats, resets head, and fires TX_COMPLETE.
 *
 * This simulates a simple NIC that processes one TX at a time from
 * register values, without descriptor ring infrastructure.
 */
void vnet_hw_start_simple_tx(struct pci_dev *pdev)
{
	if (!vnet_hw_dev)
		return;

	vnet_hw_dev->simple_tx_mode = true;
	vnet_hw_dev->tx_sim_tail = 0;

	schedule_delayed_work(&vnet_hw_dev->tx_poll_work,
			      usecs_to_jiffies(100));
}
EXPORT_SYMBOL_GPL(vnet_hw_start_simple_tx);

void vnet_hw_stop_simple_tx(struct pci_dev *pdev)
{
	if (!vnet_hw_dev)
		return;

	cancel_delayed_work_sync(&vnet_hw_dev->tx_poll_work);
	vnet_hw_dev->simple_tx_mode = false;
}
EXPORT_SYMBOL_GPL(vnet_hw_stop_simple_tx);

/*
 * Legacy RX ring setup (Parts 3b-8).
 * Registers the RX descriptor ring and buffer VAs so the platform's
 * RX simulation engine can write synthetic packets into them.
 */
void vnet_hw_set_rx_ring(struct pci_dev *pdev,
			 struct vnet_hw_desc *ring_va, u32 count,
			 void **bufs_va, u32 buf_size)
{
	if (!vnet_hw_dev)
		return;

	vnet_hw_dev->rx_ring_va = ring_va;
	vnet_hw_dev->rx_ring_count = count;
	vnet_hw_dev->rx_bufs_va = bufs_va;
	vnet_hw_dev->rx_buf_size = buf_size;
	vnet_hw_dev->rx_sim_head = 0;
	vnet_hw_dev->rx_seq = 0;

	schedule_delayed_work(&vnet_hw_dev->rx_poll_work,
			      msecs_to_jiffies(200));
}
EXPORT_SYMBOL_GPL(vnet_hw_set_rx_ring);

void vnet_hw_clear_rx_ring(struct pci_dev *pdev)
{
	if (!vnet_hw_dev)
		return;

	cancel_delayed_work_sync(&vnet_hw_dev->rx_poll_work);
	vnet_hw_dev->rx_ring_va = NULL;
	vnet_hw_dev->rx_ring_count = 0;
	vnet_hw_dev->rx_bufs_va = NULL;
	vnet_hw_dev->rx_buf_size = 0;
}
EXPORT_SYMBOL_GPL(vnet_hw_clear_rx_ring);

/*
 * Per-queue TX ring setup.
 */
void vnet_hw_set_tx_ring_queue(struct pci_dev *pdev, int queue,
			       struct vnet_hw_desc *ring_va, u32 count)
{
	struct vnet_txq_state *txq;

	if (!vnet_hw_dev || queue < 0 || queue >= VNET_NUM_QUEUES)
		return;

	txq = &vnet_hw_dev->txq[queue];
	txq->ring_va = ring_va;
	txq->ring_count = count;
	txq->sim_tail = 0;

	schedule_delayed_work(&txq->poll_work, usecs_to_jiffies(100));
}
EXPORT_SYMBOL_GPL(vnet_hw_set_tx_ring_queue);

void vnet_hw_clear_tx_ring_queue(struct pci_dev *pdev, int queue)
{
	struct vnet_txq_state *txq;

	if (!vnet_hw_dev || queue < 0 || queue >= VNET_NUM_QUEUES)
		return;

	txq = &vnet_hw_dev->txq[queue];
	cancel_delayed_work_sync(&txq->poll_work);
	txq->ring_va = NULL;
	txq->ring_count = 0;
}
EXPORT_SYMBOL_GPL(vnet_hw_clear_tx_ring_queue);

/*
 * MSI-X vector access.
 */
int vnet_hw_get_msix_vector(struct pci_dev *pdev, int vector)
{
	if (!vnet_hw_dev || vector < 0 || vector >= vnet_hw_dev->num_irqs)
		return -EINVAL;
	return vnet_hw_dev->irqs[vector];
}
EXPORT_SYMBOL_GPL(vnet_hw_get_msix_vector);

int vnet_hw_get_num_msix_vectors(struct pci_dev *pdev)
{
	if (!vnet_hw_dev)
		return 0;
	return vnet_hw_dev->num_irqs;
}
EXPORT_SYMBOL_GPL(vnet_hw_get_num_msix_vectors);

/*
 * MDIO bus access.
 */
int vnet_hw_mdio_read(struct pci_dev *pdev, int phy_addr, int reg)
{
	if (!vnet_hw_dev)
		return -ENODEV;

	/* Only one PHY at address VNET_PHY_ADDR */
	if (phy_addr != VNET_PHY_ADDR)
		return 0xFFFF;

	if (reg < 0 || reg >= 32)
		return 0xFFFF;

	/*
	 * Special handling for BMSR: always report link up
	 * (re-read latching behavior emulation)
	 */
	if (reg == MII_BMSR) {
		if (vnet_hw_dev->regs[VNET_STATUS / 4] & VNET_STATUS_LINK_UP)
			vnet_hw_dev->phy_regs[MII_BMSR] |= BMSR_LSTATUS;
		else
			vnet_hw_dev->phy_regs[MII_BMSR] &= ~BMSR_LSTATUS;
	}

	return vnet_hw_dev->phy_regs[reg];
}
EXPORT_SYMBOL_GPL(vnet_hw_mdio_read);

int vnet_hw_mdio_write(struct pci_dev *pdev, int phy_addr, int reg, u16 val)
{
	if (!vnet_hw_dev)
		return -ENODEV;

	if (phy_addr != VNET_PHY_ADDR)
		return -ENXIO;

	if (reg < 0 || reg >= 32)
		return -EINVAL;

	/* BMCR reset: re-initialize PHY */
	if (reg == MII_BMCR && (val & BMCR_RESET)) {
		vnet_init_phy(vnet_hw_dev);
		return 0;
	}

	vnet_hw_dev->phy_regs[reg] = val;
	return 0;
}
EXPORT_SYMBOL_GPL(vnet_hw_mdio_write);

/* ================================================================
 * Section 8: PCI Bus Discovery
 * ================================================================
 */
static int vnet_find_free_bus_nr(void)
{
	int nr;

	for (nr = 255; nr > 0; nr--) {
		if (!pci_find_bus(0, nr))
			return nr;
	}

	return -ENOSPC;
}

/* ================================================================
 * Section 9: Module Init / Exit
 * ================================================================
 */
static int __init vnet_platform_init(void)
{
	struct vnet_hw_device *dev;
	struct pci_host_bridge *bridge;
	int bus_nr;
	int irq;
	int err;
	int i;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	/* Initialize PCI config space, register file, and PHY */
	vnet_init_config_space(dev);
	vnet_init_regs(dev);
	vnet_init_phy(dev);

	/* Legacy TX poll work (backward compat) */
	INIT_DELAYED_WORK(&dev->tx_poll_work, vnet_tx_poll);

	/* RX simulation work */
	INIT_DELAYED_WORK(&dev->rx_poll_work, vnet_rx_poll);

	/* Per-queue TX poll work items */
	for (i = 0; i < VNET_NUM_QUEUES; i++) {
		dev->txq[i].queue_idx = i;
		INIT_DELAYED_WORK(&dev->txq[i].poll_work, vnet_txq_poll);
	}

	/*
	 * Allocate software IRQs:
	 *   irqs[0]: legacy (pdev->irq, shared by all events)
	 *   irqs[1-4]: per-queue MSI-X vectors
	 */
	dev->num_irqs = VNET_MAX_MSIX_VECTORS;
	for (i = 0; i < dev->num_irqs; i++) {
		irq = irq_alloc_descs(-1, 0, 1, numa_node_id());
		if (irq < 0) {
			err = irq;
			pr_err(DRV_NAME ": failed to allocate IRQ %d (%d)\n",
			       i, err);
			goto err_free_irqs;
		}
		irq_set_chip_and_handler(irq, &dummy_irq_chip,
					 handle_simple_irq);
		dev->irqs[i] = irq;
	}

	/* Write legacy IRQ into config space */
	dev->config[PCI_INTERRUPT_LINE] = dev->irqs[0] & 0xFF;

	/* Find an unused PCI bus number */
	bus_nr = vnet_find_free_bus_nr();
	if (bus_nr < 0) {
		err = bus_nr;
		pr_err(DRV_NAME ": no free PCI bus number\n");
		goto err_free_irqs;
	}

	/* Create virtual PCI host bridge */
	bridge = pci_alloc_host_bridge(0);
	if (!bridge) {
		err = -ENOMEM;
		goto err_free_irqs;
	}

	bridge->ops = &vnet_pci_ops;
	bridge->map_irq = vnet_map_irq;
	bridge->busnr = bus_nr;

#ifdef CONFIG_X86
	memset(&dev->sysdata, 0, sizeof(dev->sysdata));
	dev->sysdata.node = NUMA_NO_NODE;
	bridge->sysdata = &dev->sysdata;
#endif

	dev->bridge = bridge;

	/* Must set global before scanning (config ops need it) */
	vnet_hw_dev = dev;

	/* Scan the virtual bus */
	err = pci_scan_root_bus_bridge(bridge);
	if (err) {
		pr_err(DRV_NAME ": failed to scan PCI bus (%d)\n", err);
		goto err_free_bridge;
	}

	/* Find the pci_dev that was just created */
	dev->pdev = pci_get_domain_bus_and_slot(
		pci_domain_nr(bridge->bus), bus_nr, PCI_DEVFN(0, 0));
	if (!dev->pdev) {
		pr_err(DRV_NAME ": virtual PCI device not found after scan\n");
		err = -ENODEV;
		goto err_remove_bus;
	}

	/* Make device available to PCI drivers */
	pci_bus_add_devices(bridge->bus);

	pr_info(DRV_NAME ": virtual PCI NIC on bus %d "
		"(VID=%04x DID=%04x IRQ=%d MSI-X vectors=%d)\n",
		bus_nr, VNET_VENDOR_ID, VNET_DEVICE_ID,
		dev->irqs[0], dev->num_irqs);

	return 0;

err_remove_bus:
	pci_stop_root_bus(bridge->bus);
	pci_remove_root_bus(bridge->bus);
	vnet_hw_dev = NULL;
	goto err_free_irqs;

err_free_bridge:
	vnet_hw_dev = NULL;
	pci_free_host_bridge(bridge);

err_free_irqs:
	for (i = 0; i < dev->num_irqs; i++) {
		if (dev->irqs[i]) {
			irq_set_chip_and_handler(dev->irqs[i], NULL, NULL);
			irq_free_descs(dev->irqs[i], 1);
		}
	}

	kfree(dev);
	return err;
}

static void __exit vnet_platform_exit(void)
{
	struct vnet_hw_device *dev = vnet_hw_dev;
	int i;

	if (!dev)
		return;

	/* Stop all TX and RX polling */
	cancel_delayed_work_sync(&dev->tx_poll_work);
	cancel_delayed_work_sync(&dev->rx_poll_work);
	for (i = 0; i < VNET_NUM_QUEUES; i++)
		cancel_delayed_work_sync(&dev->txq[i].poll_work);

	/* Remove PCI bus and device */
	if (dev->pdev)
		pci_dev_put(dev->pdev);
	if (dev->bridge) {
		pci_stop_root_bus(dev->bridge->bus);
		pci_remove_root_bus(dev->bridge->bus);
	}

	/* Free all IRQ descriptors */
	for (i = 0; i < dev->num_irqs; i++) {
		if (dev->irqs[i]) {
			irq_set_chip_and_handler(dev->irqs[i], NULL, NULL);
			irq_free_descs(dev->irqs[i], 1);
		}
	}

	vnet_hw_dev = NULL;
	kfree(dev);

	pr_info(DRV_NAME ": virtual PCI NIC removed\n");
}

module_init(vnet_platform_init);
module_exit(vnet_platform_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Network Driver Curriculum");
MODULE_DESCRIPTION("Virtual PCI hardware platform for VNET driver series");
