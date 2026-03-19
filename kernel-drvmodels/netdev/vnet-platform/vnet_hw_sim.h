/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 *
 * VNET Virtual Hardware Simulator
 * ================================
 *
 * This file IS the hardware. It implements everything described in
 * VNET-Controller-Datasheet.md -- the register file, TX completion
 * engine, interrupt controller, and DMA backing.
 *
 * In a real NIC, these functions would be implemented in silicon on the
 * PCIe card. Here, they run as kernel code inside the driver module.
 * The driver code doesn't know the difference -- it calls hw->ops->xxx()
 * either way.
 *
 * HOW THIS FILE IS USED:
 *
 *   Each driver part (2-6) does:  #include "../vnet-platform/vnet_hw_sim.h"
 *
 *   This compiles the simulator directly into each driver's .ko module.
 *   The driver calls vnet_hw_alloc() to create a virtual NIC, then
 *   accesses it through the hw->ops function pointer table -- identical
 *   to how a real driver would access real hardware.
 *
 * WHAT REAL HARDWARE WOULD DO DIFFERENTLY:
 *
 *   - regs[] array        → MMIO-mapped silicon registers on a PCIe BAR
 *   - delayed_work        → DMA engine with hardware timer
 *   - sw_isr callback     → MSI/MSI-X interrupt from PCIe
 *   - platform_device     → PCI device from the bus
 *
 * Students: This file is the "black box". Read it with the datasheet
 * open alongside.
 */

#ifndef VNET_HW_SIM_H
#define VNET_HW_SIM_H

#define VNET_HW_SIM_INCLUDED
#include "vnet_hw_interface.h"
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/dma-mapping.h>

/* ===================================================================
 * SECTION 1: Hardware Internal State
 * ===================================================================
 *
 * This structure represents the internal state of the NIC silicon.
 * None of this is visible to the driver -- the driver only sees
 * struct vnet_hw (defined in vnet_hw_interface.h).
 *
 * Real hardware equivalent:
 *   - regs[]       = flip-flops and latches in the register file
 *   - sim_work     = the TX DMA engine's state machine
 *   - pdev_sim     = the physical PCIe device on the bus
 *   - running      = the controller's enable bit in silicon
 *   - tx_sim_tail  = the TX DMA engine's internal read pointer
 */
struct vnet_hw_sim {
	/*
	 * Virtual register file -- 256 x 32-bit registers.
	 * Indexed as regs[offset / 4].
	 *
	 * In real hardware, these are flip-flops on the die, accessed
	 * by the driver via ioread32()/iowrite32() on a PCIe BAR.
	 * Here, they're a plain C array.
	 *
	 * See Datasheet Section 2 for the complete register map.
	 */
	u32 regs[256];

	/* Hardware statistics counters (backing for regs at 0x200+) */
	u64 stats[32];

	/*
	 * Back-pointer to the public hw struct. The driver holds a
	 * pointer to struct vnet_hw; the sim needs to reach back to
	 * it for sw_isr delivery and tx_ring_va access.
	 */
	struct vnet_hw *hw_back;

	/*
	 * TX completion engine.
	 *
	 * In real hardware, this is a DMA state machine that runs
	 * continuously, reading descriptors from host memory, pushing
	 * packet data out the wire, and writing back completion status.
	 *
	 * Here, we use a delayed_work that fires ~100us after the
	 * driver writes TX_RING_HEAD. The delay simulates the time
	 * real hardware would spend on DMA and transmission.
	 */
	struct delayed_work sim_work;

	/*
	 * Platform device providing struct device for DMA.
	 *
	 * Every DMA API call (dma_alloc_coherent, dma_map_single, etc.)
	 * requires a struct device*. Real drivers use &pdev->dev from
	 * their PCI device. Since we have no PCI device, we create a
	 * platform device to fill this role.
	 *
	 * The driver accesses it as hw->dev, never knowing it's not
	 * a real PCI device.
	 */
	struct platform_device *pdev_sim;

	/* Controller running state (set by start/stop ops) */
	bool running;

	/*
	 * TX DMA engine's internal read pointer.
	 *
	 * The driver advances ring->head when it submits descriptors.
	 * The hardware advances tx_sim_tail as it "transmits" them.
	 * When tx_sim_tail catches up to head, all packets are sent.
	 *
	 * This is NOT the same as the driver's ring->tail -- that
	 * tracks which descriptors the driver has reclaimed after
	 * completion. This tracks which descriptors the hardware
	 * has processed.
	 */
	u32 tx_sim_tail;
};

/* Forward declarations */
static struct vnet_hw_ops vnet_sim_ops;
static void vnet_sim_fire_interrupt(struct vnet_hw_sim *sim, u32 bits);
static void vnet_sim_tx_work(struct work_struct *work);

/* ===================================================================
 * SECTION 2: Hardware Lifecycle
 * ===================================================================
 *
 * These functions correspond to powering on the NIC, initializing it,
 * and removing it from the system. In a real PCI driver, these would
 * be triggered by the PCI subsystem during probe/remove.
 */

/*
 * vnet_hw_alloc -- "Plug the NIC into the PCIe slot"
 *
 * Creates the virtual hardware: allocates the register file, creates
 * the DMA-capable device, and wires up the ops table.
 *
 * Real hardware equivalent: the NIC physically exists on the PCIe bus.
 * The kernel discovers it during PCI enumeration and calls the driver's
 * probe() function.
 */
static struct vnet_hw *vnet_hw_alloc(struct pci_dev *pdev)
{
	struct vnet_hw *hw;
	struct vnet_hw_sim *sim;
	int err;

	hw = kzalloc(sizeof(*hw), GFP_KERNEL);
	if (!hw)
		return NULL;

	sim = kzalloc(sizeof(*sim), GFP_KERNEL);
	if (!sim) {
		kfree(hw);
		return NULL;
	}

	/*
	 * Create a platform device to back DMA operations.
	 *
	 * Real NIC: the PCI subsystem already created a struct device
	 * for us during enumeration. We'd use &pdev->dev.
	 *
	 * Simulated: no PCI device exists, so we create a platform
	 * device. The kernel's DMA subsystem treats it the same way.
	 */
	sim->pdev_sim = platform_device_alloc("vnet_sim", PLATFORM_DEVID_AUTO);
	if (!sim->pdev_sim) {
		kfree(sim);
		kfree(hw);
		return NULL;
	}

	err = platform_device_add(sim->pdev_sim);
	if (err) {
		platform_device_put(sim->pdev_sim);
		kfree(sim);
		kfree(hw);
		return NULL;
	}

	/*
	 * Set 32-bit DMA mask.
	 *
	 * This tells the kernel what physical address range our "hardware"
	 * can DMA to/from. Real NICs might support 64-bit DMA; we keep
	 * it simple with 32-bit.
	 */
	err = dma_set_mask_and_coherent(&sim->pdev_sim->dev, DMA_BIT_MASK(32));
	if (err)
		pr_warn("vnet_sim: failed to set DMA mask (%d)\n", err);

	/* Wire up the public hw struct that drivers see */
	hw->pdev = pdev;
	hw->ops = &vnet_sim_ops;       /* Function pointer table */
	hw->priv = sim;                 /* Opaque pointer to our internals */
	hw->dev = &sim->pdev_sim->dev;  /* DMA device for driver use */

	sim->hw_back = hw;

	/*
	 * Initialize the TX completion engine.
	 *
	 * INIT_DELAYED_WORK sets up a workqueue item that can be
	 * scheduled with a delay. We use this to simulate the time
	 * hardware spends processing TX descriptors.
	 */
	INIT_DELAYED_WORK(&sim->sim_work, vnet_sim_tx_work);

	return hw;
}

/*
 * vnet_hw_free -- "Unplug the NIC"
 *
 * Tears down all hardware resources. After this, the hw pointer is
 * invalid -- just like removing a PCIe card.
 */
static void vnet_hw_free(struct vnet_hw *hw)
{
	struct vnet_hw_sim *sim;

	if (!hw)
		return;

	sim = hw->priv;
	if (sim) {
		/* Stop any pending TX completion work */
		cancel_delayed_work_sync(&sim->sim_work);

		/* Destroy the platform device (our DMA backing) */
		if (sim->pdev_sim)
			platform_device_unregister(sim->pdev_sim);
		kfree(sim);
	}
	kfree(hw);
}

/*
 * vnet_hw_init -- "Power-on reset"
 *
 * Sets all registers to their power-on defaults.
 *
 * See Datasheet Section 2: all registers reset to 0x00000000 except:
 *   - STATUS (0x004): LINK_UP | TX_ACTIVE | RX_ACTIVE
 *     (link is always up in simulation)
 *   - MAX_FRAME_REG (0x018): 1518 (standard Ethernet MTU + header)
 */
static int vnet_hw_init(struct vnet_hw *hw)
{
	struct vnet_hw_sim *sim = hw->priv;

	memset(sim->regs, 0, sizeof(sim->regs));

	/* STATUS register: link up, both engines ready */
	sim->regs[VNET_STATUS / 4] = VNET_STATUS_LINK_UP |
				      VNET_STATUS_TX_ACTIVE |
				      VNET_STATUS_RX_ACTIVE;

	/* MAX_FRAME_REG: standard Ethernet frame size */
	sim->regs[VNET_MAX_FRAME_REG / 4] = VNET_MAX_FRAME_SIZE_DEFAULT;

	/* Reset the TX DMA engine's internal pointer */
	sim->tx_sim_tail = 0;
	sim->running = false;

	return 0;
}

/*
 * vnet_hw_cleanup -- "Shut down before removal"
 *
 * Stops the controller and cancels any pending work. The driver calls
 * this before vnet_hw_free(). In real hardware, this would quiesce the
 * DMA engines to prevent stale writes to freed memory.
 */
static void vnet_hw_cleanup(struct vnet_hw *hw)
{
	struct vnet_hw_sim *sim = hw->priv;

	sim->running = false;
	cancel_delayed_work_sync(&sim->sim_work);
}

/* ===================================================================
 * SECTION 3: Interrupt Controller
 * ===================================================================
 *
 * See Datasheet Section 7 -- Interrupt Model.
 *
 * The interrupt controller has two registers:
 *   INT_MASK   (0x008): 1 = disabled, 0 = enabled
 *   INT_STATUS (0x00C): 1 = pending, write-1-to-clear
 *
 * An interrupt fires when: INT_STATUS & ~INT_MASK != 0
 *
 * Real hardware: sets INT_STATUS bits, then asserts the MSI/MSI-X
 * interrupt line on the PCIe bus. The CPU's interrupt controller
 * routes it to the driver's IRQ handler.
 *
 * Simulation: sets INT_STATUS bits, then directly calls the sw_isr
 * callback that the driver registered in hw->sw_isr. Same handler
 * signature (irqreturn_t), same logic inside -- the driver can't
 * tell the difference.
 */
static void vnet_sim_fire_interrupt(struct vnet_hw_sim *sim, u32 bits)
{
	struct vnet_hw *hw = sim->hw_back;
	u32 mask = sim->regs[VNET_INT_MASK / 4];

	/* Set the pending bits in INT_STATUS */
	sim->regs[VNET_INT_STATUS / 4] |= bits;

	/*
	 * Deliver the interrupt only if:
	 *   1. At least one of the new bits is unmasked (enabled)
	 *   2. The driver has registered a handler
	 *
	 * This matches real hardware behavior: masked interrupts still
	 * set status bits, but don't assert the interrupt line.
	 */
	if ((bits & ~mask) && hw->sw_isr && hw->sw_isr_data)
		hw->sw_isr(0, hw->sw_isr_data);
}

/* ===================================================================
 * SECTION 4: TX Completion Engine
 * ===================================================================
 *
 * See Datasheet Section 5 -- TX Operation.
 *
 * This is the heart of the simulation. When the driver submits TX
 * descriptors and writes TX_RING_HEAD, this work handler runs after
 * a short delay to simulate hardware processing:
 *
 *   Driver (Part 4+)                  Simulator (this code)
 *   ────────────────                  ─────────────────────
 *   1. Map skb for DMA
 *   2. Fill descriptor:
 *      addr, len, flags=OWN|SOP|EOP
 *   3. Advance head
 *   4. Write TX_RING_HEAD register
 *      ──── write_reg triggers ────→  5. Schedule delayed_work
 *                                        (~100us delay)
 *                                     6. Walk ring: sim_tail → head
 *                                     7. For each OWN descriptor:
 *                                        - Clear OWN flag
 *                                        - Set STATUS_OK | length
 *                                        - Increment stats
 *                                     8. Update TX_RING_TAIL register
 *                                     9. Fire TX_COMPLETE interrupt
 *      ←── sw_isr called ──────────
 *   10. vnet_tx_complete():
 *       - Walk ring from tail
 *       - Unmap DMA, free skb
 *       - Wake queue if was stopped
 *
 * In real hardware, steps 5-9 happen in the NIC's DMA engine silicon,
 * with the packet actually going out on the wire between steps 6 and 7.
 */
static void vnet_sim_tx_work(struct work_struct *work)
{
	struct vnet_hw_sim *sim = container_of(work, struct vnet_hw_sim,
					       sim_work.work);
	struct vnet_hw *hw = sim->hw_back;
	u32 head;
	bool completed = false;

	/* Don't process if controller is stopped or ring not set up */
	if (!sim->running || !hw->tx_ring_va)
		return;

	/* Read where the driver says the newest descriptor is */
	head = sim->regs[VNET_TX_RING_HEAD / 4];

	/*
	 * Walk from the hardware's internal tail to the driver's head,
	 * "transmitting" each descriptor along the way.
	 */
	while (sim->tx_sim_tail != head) {
		struct vnet_hw_desc *desc = &hw->tx_ring_va[sim->tx_sim_tail];

		/*
		 * Only process descriptors that the driver has submitted.
		 * The OWN flag means "hardware, this is yours to process."
		 *
		 * See Datasheet Section 4.1: OWN = bit 31 of flags field.
		 */
		if (!(desc->flags & VNET_DESC_OWN))
			break;

		/*
		 * "Transmit" the packet:
		 *
		 * In real hardware, the DMA engine would read packet data
		 * from the address in desc->addr, push it through the MAC,
		 * out the PHY, and onto the wire.
		 *
		 * We simulate this by simply clearing OWN (giving the
		 * descriptor back to the driver) and setting STATUS_OK.
		 * The driver's TX completion handler sees OWN=0 and
		 * knows the packet was sent.
		 */
		desc->flags &= ~VNET_DESC_OWN;
		desc->status = VNET_DESC_STATUS_OK |
			       (desc->len & VNET_DESC_STATUS_LEN_MASK);

		/*
		 * Update hardware statistics counters.
		 * See Datasheet Section 2.4 -- Statistics registers.
		 * These are what ethtool -S reads.
		 */
		sim->regs[VNET_STATS_TX_PACKETS / 4]++;
		sim->regs[VNET_STATS_TX_BYTES / 4] +=
			desc->len & VNET_DESC_STATUS_LEN_MASK;

		/* Advance the hardware's internal pointer (wraps around) */
		sim->tx_sim_tail =
			(sim->tx_sim_tail + 1) % hw->tx_ring_count;
		completed = true;
	}

	/* Update the TX_RING_TAIL register so the driver can read it */
	sim->regs[VNET_TX_RING_TAIL / 4] = sim->tx_sim_tail;

	/*
	 * Fire a TX_COMPLETE interrupt to tell the driver that
	 * descriptors are ready to be reclaimed.
	 *
	 * See Datasheet Section 7.1: TX_COMPLETE = bit 3 of INT_STATUS.
	 *
	 * This triggers the driver's interrupt handler, which calls
	 * vnet_tx_complete() to walk the ring and free transmitted skbs.
	 */
	if (completed)
		vnet_sim_fire_interrupt(sim, VNET_INT_TX_COMPLETE);
}

/* ===================================================================
 * SECTION 5: Register Access
 * ===================================================================
 *
 * See Datasheet Section 2 -- Register Map.
 *
 * In real hardware, read_reg/write_reg would be ioread32()/iowrite32()
 * accessing MMIO-mapped registers on a PCIe BAR. Here, they're array
 * reads and writes.
 *
 * The key insight: write_reg has SIDE EFFECTS. Writing certain offsets
 * triggers hardware behavior, just like real silicon. For example,
 * writing TX_RING_HEAD tells the DMA engine "new work is ready."
 */

/*
 * Reset the controller -- see Datasheet Section 3.1, CTRL.RESET.
 * Restores all registers to power-on defaults.
 */
static int vnet_sim_reset(struct vnet_hw *hw) { return vnet_hw_init(hw); }

/*
 * Start the controller.
 *
 * After this, the TX completion engine will process descriptors when
 * the driver writes TX_RING_HEAD. Before this, writes to TX_RING_HEAD
 * are ignored (sim->running is false).
 */
static int vnet_sim_start(struct vnet_hw *hw)
{
	struct vnet_hw_sim *sim = hw->priv;

	sim->running = true;
	return 0;
}

/*
 * Stop the controller and drain pending work.
 *
 * cancel_delayed_work_sync() ensures the TX completion handler has
 * finished before we return. This prevents use-after-free if the
 * driver is about to free the ring buffers.
 */
static void vnet_sim_stop(struct vnet_hw *hw)
{
	struct vnet_hw_sim *sim = hw->priv;

	sim->running = false;
	cancel_delayed_work_sync(&sim->sim_work);
}

/*
 * Read a 32-bit register.
 *
 * Real hardware: ioread32(hw->regs + offset)
 * Simulation:    sim->regs[offset / 4]
 *
 * The driver uses this to check STATUS.LINK_UP, read INT_STATUS,
 * poll statistics counters, etc.
 */
static u32 vnet_sim_read_reg(struct vnet_hw *hw, u32 off)
{
	struct vnet_hw_sim *sim = hw->priv;

	return (off < sizeof(sim->regs)) ? sim->regs[off / 4] : 0;
}

/*
 * Write a 32-bit register.
 *
 * Real hardware: iowrite32(val, hw->regs + offset)
 * Simulation:    sim->regs[offset / 4] = val, plus side effects
 *
 * IMPORTANT: This is where the simulation "comes alive." Writing to
 * TX_RING_HEAD triggers the TX completion engine, just like writing
 * to a real NIC's doorbell register kicks its DMA engine.
 *
 * See Datasheet Section 5.2, step 4: "Write head to TX_RING_HEAD"
 */
static void vnet_sim_write_reg(struct vnet_hw *hw, u32 off, u32 val)
{
	struct vnet_hw_sim *sim = hw->priv;

	if (off >= sizeof(sim->regs))
		return;

	sim->regs[off / 4] = val;

	/*
	 * Side effect: writing TX_RING_HEAD is the "doorbell."
	 *
	 * The driver writes this after filling new TX descriptors.
	 * In real hardware, this would wake up the DMA engine.
	 * Here, we schedule delayed work to simulate processing time.
	 *
	 * The 100us delay is arbitrary but gives the driver time to
	 * return from xmit before the completion interrupt fires,
	 * matching real hardware timing.
	 */
	if (off == VNET_TX_RING_HEAD && sim->running)
		mod_delayed_work(system_wq, &sim->sim_work,
				 usecs_to_jiffies(100));
}

/* ===================================================================
 * SECTION 6: Interrupt Mask Control
 * ===================================================================
 *
 * See Datasheet Section 3.3 -- INT_MASK register.
 *
 * Convention: mask bit = 1 means DISABLED, 0 means ENABLED.
 * This matches real hardware (e.g., Intel e1000's IMC/IMS registers).
 *
 * The NAPI pattern uses this heavily:
 *   1. RX interrupt fires
 *   2. ISR calls disable_interrupts(VNET_INT_RX_PACKET)
 *   3. ISR schedules NAPI poll
 *   4. NAPI poll processes packets
 *   5. When done, NAPI calls enable_interrupts(VNET_INT_RX_PACKET)
 */

static int vnet_sim_enable_interrupts(struct vnet_hw *hw, u32 mask)
{
	struct vnet_hw_sim *sim = hw->priv;

	/* Clear mask bits = enable those interrupts */
	sim->regs[VNET_INT_MASK / 4] &= ~mask;
	return 0;
}

static void vnet_sim_disable_interrupts(struct vnet_hw *hw, u32 mask)
{
	struct vnet_hw_sim *sim = hw->priv;

	/* Set mask bits = disable those interrupts */
	sim->regs[VNET_INT_MASK / 4] |= mask;
}

/*
 * Read pending interrupt status.
 *
 * The driver's interrupt handler reads this first to determine what
 * happened. Multiple bits can be set simultaneously.
 *
 * See Datasheet Section 3.4 -- INT_STATUS register.
 */
static u32 vnet_sim_get_int_status(struct vnet_hw *hw)
{
	struct vnet_hw_sim *sim = hw->priv;

	return sim->regs[VNET_INT_STATUS / 4];
}

/*
 * Clear interrupt status bits (write-1-to-clear).
 *
 * The driver writes 1 to each bit it has handled. This acknowledges
 * the interrupt and prevents it from firing again for the same event.
 *
 * See Datasheet Section 3.4: "Write-1-to-clear semantics."
 */
static void vnet_sim_clear_int_status(struct vnet_hw *hw, u32 mask)
{
	struct vnet_hw_sim *sim = hw->priv;

	sim->regs[VNET_INT_STATUS / 4] &= ~mask;
}

/* ===================================================================
 * SECTION 7: MAC Address
 * ===================================================================
 *
 * See Datasheet Section 8 -- MAC Address.
 *
 * The 48-bit MAC is split across two 32-bit registers:
 *   MAC_ADDR_LOW  (0x010): bytes [3:0], little-endian
 *   MAC_ADDR_HIGH (0x014): bytes [5:4] in low 16 bits
 *
 * Example: MAC AA:BB:CC:DD:EE:FF
 *   MAC_ADDR_LOW  = 0xDDCCBBAA
 *   MAC_ADDR_HIGH = 0x0000FFEE
 *
 * In real hardware, the MAC address is often burned into EEPROM and
 * loaded into these registers at power-on. Here, the driver sets it
 * with eth_hw_addr_random() and writes it down.
 */
static int vnet_sim_set_mac(struct vnet_hw *hw, const u8 *addr)
{
	struct vnet_hw_sim *sim = hw->priv;

	sim->regs[VNET_MAC_ADDR_LOW / 4] =
		addr[0] | (addr[1] << 8) | (addr[2] << 16) | (addr[3] << 24);
	sim->regs[VNET_MAC_ADDR_HIGH / 4] = addr[4] | (addr[5] << 8);
	return 0;
}

static int vnet_sim_get_mac(struct vnet_hw *hw, u8 *addr)
{
	struct vnet_hw_sim *sim = hw->priv;
	u32 lo = sim->regs[VNET_MAC_ADDR_LOW / 4];
	u32 hi = sim->regs[VNET_MAC_ADDR_HIGH / 4];

	addr[0] = lo & 0xFF;
	addr[1] = (lo >> 8) & 0xFF;
	addr[2] = (lo >> 16) & 0xFF;
	addr[3] = (lo >> 24) & 0xFF;
	addr[4] = hi & 0xFF;
	addr[5] = (hi >> 8) & 0xFF;
	return 0;
}

/* ===================================================================
 * SECTION 8: Link Status
 * ===================================================================
 *
 * See Datasheet Section 3.2 -- STATUS register, LINK_UP bit.
 *
 * In real hardware, link detection involves reading PHY registers
 * over MDIO, checking auto-negotiation results, and monitoring the
 * link-detect pin. The LINK_CHANGE interrupt fires on transitions.
 *
 * In simulation, link is always up (set during vnet_hw_init).
 * The driver still checks it in open() and handles LINK_CHANGE
 * interrupts -- the code is correct for real hardware.
 */
static int vnet_sim_get_link(struct vnet_hw *hw)
{
	struct vnet_hw_sim *sim = hw->priv;

	return !!(sim->regs[VNET_STATUS / 4] & VNET_STATUS_LINK_UP);
}

/* Speed is fixed at 1 Gbps in simulation */
static void vnet_sim_set_speed(struct vnet_hw *hw, u32 s) { }

/* ===================================================================
 * SECTION 9: Hardware Statistics
 * ===================================================================
 *
 * See Datasheet Section 2.4 -- Statistics registers.
 *
 * The TX completion engine (Section 4) increments these counters as
 * it processes descriptors. The driver reads them via get_stats(),
 * and ethtool -S displays them.
 *
 * In real hardware, these are typically 64-bit counters maintained
 * by the NIC's packet processing pipeline. They count every packet
 * that passes through the wire, independent of what the driver sees.
 */
static void vnet_sim_get_stats(struct vnet_hw *hw, u64 *d)
{
	struct vnet_hw_sim *sim = hw->priv;

	d[0] = sim->regs[VNET_STATS_TX_PACKETS / 4];
	d[1] = sim->regs[VNET_STATS_TX_BYTES / 4];
	d[2] = sim->regs[VNET_STATS_RX_PACKETS / 4];
	d[3] = sim->regs[VNET_STATS_RX_BYTES / 4];
	d[4] = 0; d[5] = 0; d[6] = 0; d[7] = 0;
}

static void vnet_sim_reset_stats(struct vnet_hw *hw)
{
	struct vnet_hw_sim *sim = hw->priv;

	sim->regs[VNET_STATS_TX_PACKETS / 4] = 0;
	sim->regs[VNET_STATS_TX_BYTES / 4] = 0;
	sim->regs[VNET_STATS_RX_PACKETS / 4] = 0;
	sim->regs[VNET_STATS_RX_BYTES / 4] = 0;
}

/* ===================================================================
 * SECTION 10: Capabilities and Features
 * ===================================================================
 *
 * See Datasheet Section 11 -- Capabilities.
 *
 * Reports what the hardware supports. The driver reads this during
 * probe to know which features to enable. Real NICs use PCI config
 * space or dedicated capability registers for this.
 */
static u32 vnet_sim_get_caps(struct vnet_hw *hw)
{
	return VNET_FEAT_CSUM_TX | VNET_FEAT_CSUM_RX | VNET_FEAT_NAPI;
}

/* Feature enablement is accepted but has no effect in simulation */
static int vnet_sim_set_features(struct vnet_hw *hw, u32 f) { return 0; }

/* ===================================================================
 * SECTION 11: Ring Buffer Setup
 * ===================================================================
 *
 * See Datasheet Sections 2.2, 2.3 -- TX/RX Ring registers.
 * See Datasheet Section 5.1 -- Ring Setup.
 *
 * The driver calls these during open() to tell the hardware where the
 * descriptor rings live in DMA memory and how large they are.
 *
 * In real hardware, these values are written to BAR registers that
 * the DMA engine reads to know where to fetch/store descriptors.
 * Here, they populate the register array.
 *
 * The TX ring also resets the sim's internal tail pointer, so the
 * completion engine starts from the beginning of the ring.
 */
static int vnet_sim_setup_tx(struct vnet_hw *hw, dma_addr_t a, u32 s)
{
	struct vnet_hw_sim *sim = hw->priv;

	sim->regs[VNET_TX_RING_ADDR / 4] = (u32)a;
	sim->regs[VNET_TX_RING_SIZE / 4] = s;
	sim->regs[VNET_TX_RING_HEAD / 4] = 0;
	sim->regs[VNET_TX_RING_TAIL / 4] = 0;
	sim->tx_sim_tail = 0;
	return 0;
}

static int vnet_sim_setup_rx(struct vnet_hw *hw, dma_addr_t a, u32 s)
{
	struct vnet_hw_sim *sim = hw->priv;

	sim->regs[VNET_RX_RING_ADDR / 4] = (u32)a;
	sim->regs[VNET_RX_RING_SIZE / 4] = s;
	sim->regs[VNET_RX_RING_HEAD / 4] = 0;
	sim->regs[VNET_RX_RING_TAIL / 4] = 0;
	return 0;
}

/* ===================================================================
 * SECTION 12: Operations Table
 * ===================================================================
 *
 * This is the function pointer table that drivers access as hw->ops.
 * It's the hardware abstraction layer -- the single point where
 * "call a C function" translates to "poke the hardware."
 *
 * In a real driver with a real NIC, these would be thin wrappers
 * around ioread32()/iowrite32() and perhaps some register sequences.
 * The driver code calling hw->ops->xxx() is identical either way.
 *
 * See vnet_hw_interface.h for the struct vnet_hw_ops definition.
 */
static struct vnet_hw_ops vnet_sim_ops = {
	.reset               = vnet_sim_reset,
	.start               = vnet_sim_start,
	.stop                = vnet_sim_stop,
	.read_reg            = vnet_sim_read_reg,
	.write_reg           = vnet_sim_write_reg,
	.enable_interrupts   = vnet_sim_enable_interrupts,
	.disable_interrupts  = vnet_sim_disable_interrupts,
	.get_interrupt_status = vnet_sim_get_int_status,
	.clear_interrupt_status = vnet_sim_clear_int_status,
	.set_mac_address     = vnet_sim_set_mac,
	.get_mac_address     = vnet_sim_get_mac,
	.get_link_status     = vnet_sim_get_link,
	.set_link_speed      = vnet_sim_set_speed,
	.get_stats           = vnet_sim_get_stats,
	.reset_stats         = vnet_sim_reset_stats,
	.get_capabilities    = vnet_sim_get_caps,
	.set_features        = vnet_sim_set_features,
	.setup_tx_ring       = vnet_sim_setup_tx,
	.setup_rx_ring       = vnet_sim_setup_rx,
};

#endif /* VNET_HW_SIM_H */
