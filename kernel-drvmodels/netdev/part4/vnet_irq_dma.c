// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 *
 * Part 4: Interrupts, RX Ring & Simple TX DMA
 *
 * Concepts introduced (over Part 3):
 *   - request_irq / free_irq with pdev->irq
 *   - Hardware interrupt handler (ISR)
 *   - pci_set_master for bus mastering (DMA)
 *   - dma_set_mask_and_coherent for DMA addressing
 *   - dma_alloc_coherent for RX descriptor ring and RX buffers
 *   - dma_map_single / dma_unmap_single for TX packet data
 *   - RX path: hardware fills buffers, driver processes in ISR, netif_rx()
 *   - Simple TX: map skb, write DMA addr+len to registers, doorbell
 *   - TX completion: unmap DMA, free skb in ISR
 *   - Interrupt enable/disable via INT_MASK register
 *   - netif_stop_queue / netif_wake_queue for single-shot TX
 *
 * TX has NO descriptor ring -- just register writes + doorbell.
 *
 * DMA pattern:
 *   RX uses dma_alloc_coherent (long-lived buffers, HW writes anytime)
 *   TX uses dma_map_single (transient, driver-initiated, unmap after done)
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/io.h>

#include "../vnet-platform/vnet_hw_interface.h"

#define DRV_NAME	"vnet_irq_dma"
#define VNET_RING_SIZE	64
#define VNET_MAX_PKT_LEN 1518

/* Hardware MTU limits */
#define VNET_MIN_MTU	68
#define VNET_MAX_MTU	9000

struct vnet_priv {
	struct net_device *ndev;
	struct pci_dev *pdev;
	void __iomem *regs;
	struct net_device_stats stats;

	/* RX ring (coherent DMA) */
	struct vnet_hw_desc *rx_descs;		/* descriptor ring VA */
	dma_addr_t rx_descs_dma;		/* descriptor ring DMA addr */
	void **rx_bufs_va;			/* per-slot buffer VAs */
	dma_addr_t *rx_bufs_dma;		/* per-slot buffer DMA addrs */
	u32 rx_count;
	u32 rx_tail;				/* next descriptor to check */

	/*
	 * Simple TX -- no descriptor ring, no coherent TX allocation.
	 * Just dma_map_single the skb, write addr+len to HW registers,
	 * and ring the doorbell. One packet at a time.
	 */
	struct sk_buff *tx_skb;			/* in-flight skb */
	dma_addr_t tx_dma_addr;			/* mapped skb DMA addr */
	u32 tx_len;				/* length for unmap */
	bool tx_busy;
};

/* ---- Helpers ---- */

static void vnet_write_mac_to_hw(void __iomem *regs, const u8 *addr)
{
	iowrite32(addr[0] | (addr[1] << 8) |
		  (addr[2] << 16) | (addr[3] << 24),
		  regs + VNET_MAC_ADDR_LOW);
	iowrite32(addr[4] | (addr[5] << 8),
		  regs + VNET_MAC_ADDR_HIGH);
}

/*
 * vnet_irq_enable -- Unmask all interrupt sources
 *
 * INT_MASK register: bit = 1 means DISABLED (masked).
 * Writing 0 enables all four interrupt sources.
 */
static void vnet_irq_enable(struct vnet_priv *priv)
{
	iowrite32(0, priv->regs + VNET_INT_MASK);
}

/*
 * vnet_irq_disable -- Mask all interrupt sources
 *
 * Writing all-ones disables every interrupt.
 */
static void vnet_irq_disable(struct vnet_priv *priv)
{
	iowrite32(~0U, priv->regs + VNET_INT_MASK);
}

/* ---- RX Path ---- */

/*
 * vnet_rx_alloc_ring -- Allocate RX descriptor ring and packet buffers
 *
 * Uses dma_alloc_coherent for both the descriptor ring and every RX
 * buffer.  Coherent DMA is the right choice here because the hardware
 * (platform simulator) writes into these buffers at unpredictable
 * times -- there is no explicit sync point the driver can use.
 */
static int vnet_rx_alloc_ring(struct vnet_priv *priv)
{
	struct device *dev = &priv->pdev->dev;
	int i;

	priv->rx_count = VNET_RING_SIZE;
	priv->rx_tail = 0;

	/* Allocate descriptor ring (coherent -- HW reads/writes descriptors) */
	priv->rx_descs = dma_alloc_coherent(dev,
			priv->rx_count * sizeof(struct vnet_hw_desc),
			&priv->rx_descs_dma, GFP_KERNEL);
	if (!priv->rx_descs)
		return -ENOMEM;

	/* Allocate tracking arrays for per-slot buffer VAs and DMA addrs */
	priv->rx_bufs_va = kcalloc(priv->rx_count, sizeof(void *), GFP_KERNEL);
	if (!priv->rx_bufs_va)
		goto err_free_descs;

	priv->rx_bufs_dma = kcalloc(priv->rx_count, sizeof(dma_addr_t),
				    GFP_KERNEL);
	if (!priv->rx_bufs_dma)
		goto err_free_bufs_va;

	/* Allocate each RX buffer with coherent DMA */
	for (i = 0; i < priv->rx_count; i++) {
		priv->rx_bufs_va[i] = dma_alloc_coherent(dev,
				VNET_MAX_PKT_LEN,
				&priv->rx_bufs_dma[i], GFP_KERNEL);
		if (!priv->rx_bufs_va[i])
			goto err_free_buffers;

		/* Initialize descriptor: give ownership to hardware */
		priv->rx_descs[i].addr = priv->rx_bufs_dma[i];
		priv->rx_descs[i].len = VNET_MAX_PKT_LEN;
		priv->rx_descs[i].flags = VNET_DESC_OWN;
		priv->rx_descs[i].status = 0;
	}

	return 0;

err_free_buffers:
	while (--i >= 0) {
		dma_free_coherent(dev, VNET_MAX_PKT_LEN,
				  priv->rx_bufs_va[i],
				  priv->rx_bufs_dma[i]);
	}
	kfree(priv->rx_bufs_dma);
	priv->rx_bufs_dma = NULL;
err_free_bufs_va:
	kfree(priv->rx_bufs_va);
	priv->rx_bufs_va = NULL;
err_free_descs:
	dma_free_coherent(dev,
			  priv->rx_count * sizeof(struct vnet_hw_desc),
			  priv->rx_descs, priv->rx_descs_dma);
	priv->rx_descs = NULL;
	return -ENOMEM;
}

/*
 * vnet_rx_free_ring -- Free all RX coherent DMA allocations
 */
static void vnet_rx_free_ring(struct vnet_priv *priv)
{
	struct device *dev = &priv->pdev->dev;
	int i;

	if (!priv->rx_descs)
		return;

	for (i = 0; i < priv->rx_count; i++) {
		if (priv->rx_bufs_va && priv->rx_bufs_va[i])
			dma_free_coherent(dev, VNET_MAX_PKT_LEN,
					  priv->rx_bufs_va[i],
					  priv->rx_bufs_dma[i]);
	}

	kfree(priv->rx_bufs_dma);
	priv->rx_bufs_dma = NULL;

	kfree(priv->rx_bufs_va);
	priv->rx_bufs_va = NULL;

	dma_free_coherent(dev,
			  priv->rx_count * sizeof(struct vnet_hw_desc),
			  priv->rx_descs, priv->rx_descs_dma);
	priv->rx_descs = NULL;
}

/*
 * vnet_rx_process -- Walk the RX ring and deliver packets to the stack
 *
 * Called from the ISR when VNET_INT_RX_PACKET fires.
 * Walks from rx_tail forward, looking for descriptors the hardware has
 * completed (OWN bit cleared).  For each:
 *   1. Allocate an skb
 *   2. Copy the packet data from the coherent DMA buffer
 *   3. Set up skb metadata (protocol, dev)
 *   4. Hand to stack via netif_rx()
 *   5. Re-post the descriptor (set OWN) so hardware can reuse the slot
 */
static void vnet_rx_process(struct vnet_priv *priv)
{
	struct net_device *ndev = priv->ndev;
	u32 tail = priv->rx_tail;
	int budget = priv->rx_count;  /* process at most one full ring */

	while (budget-- > 0) {
		struct vnet_hw_desc *desc = &priv->rx_descs[tail];
		struct sk_buff *skb;
		u32 pkt_len;

		/* Check if hardware still owns this descriptor */
		if (desc->flags & VNET_DESC_OWN)
			break;

		/* Check for successful reception */
		if (!(desc->status & VNET_DESC_STATUS_OK)) {
			priv->stats.rx_errors++;
			goto repost;
		}

		pkt_len = desc->status & VNET_DESC_STATUS_LEN_MASK;
		if (pkt_len == 0 || pkt_len > VNET_MAX_PKT_LEN) {
			priv->stats.rx_errors++;
			goto repost;
		}

		/* Allocate an skb and copy the data from the coherent buffer */
		skb = netdev_alloc_skb_ip_align(ndev, pkt_len);
		if (!skb) {
			priv->stats.rx_dropped++;
			goto repost;
		}

		memcpy(skb_put(skb, pkt_len),
		       priv->rx_bufs_va[tail], pkt_len);

		skb->protocol = eth_type_trans(skb, ndev);
		skb->dev = ndev;

		/* Hand the packet to the network stack */
		netif_rx(skb);

		priv->stats.rx_packets++;
		priv->stats.rx_bytes += pkt_len;

repost:
		/* Give the descriptor back to hardware */
		desc->len = VNET_MAX_PKT_LEN;
		desc->status = 0;
		desc->flags = VNET_DESC_OWN;

		tail = (tail + 1) % priv->rx_count;
	}

	priv->rx_tail = tail;
}

/* ---- TX Path (Simple Register-Based, No Ring) ---- */

/*
 * vnet_tx_complete -- Handle TX completion interrupt
 *
 * Called from the ISR when VNET_INT_TX_COMPLETE fires.
 * The platform has "transmitted" the packet (detected our doorbell
 * write, updated stats, fired this interrupt).
 *
 * We unmap the streaming DMA mapping, free the skb, and wake the
 * TX queue so the stack can send another packet.
 *
 * This is the simple TX completion model: one packet at a time,
 * no descriptor ring.
 */
static void vnet_tx_complete(struct vnet_priv *priv)
{
	struct device *dev = &priv->pdev->dev;

	if (!priv->tx_skb)
		return;

	/*
	 * Unmap the streaming DMA mapping.
	 * After this call, the CPU owns the memory again and the
	 * DMA address is no longer valid for device access.
	 */
	dma_unmap_single(dev, priv->tx_dma_addr,
			 priv->tx_len, DMA_TO_DEVICE);

	priv->stats.tx_packets++;
	priv->stats.tx_bytes += priv->tx_len;

	/* Free the transmitted skb */
	dev_kfree_skb_irq(priv->tx_skb);
	priv->tx_skb = NULL;
	priv->tx_dma_addr = 0;
	priv->tx_len = 0;
	priv->tx_busy = false;

	/* Wake the queue -- we can accept another packet now */
	netif_wake_queue(priv->ndev);
}

/* ---- Interrupt Handler ---- */

/*
 * vnet_isr -- Hardware interrupt handler
 *
 * Reads INT_STATUS to determine which events fired, then dispatches
 * to the appropriate handler.  Acknowledges handled bits by writing
 * them back to INT_STATUS (write-1-to-clear semantics).
 */
static irqreturn_t vnet_isr(int irq, void *dev_id)
{
	struct vnet_priv *priv = dev_id;
	u32 status;

	status = ioread32(priv->regs + VNET_INT_STATUS);
	if (!status)
		return IRQ_NONE;

	/* Acknowledge all pending bits (write-1-to-clear) */
	iowrite32(status, priv->regs + VNET_INT_STATUS);

	if (status & VNET_INT_TX_COMPLETE)
		vnet_tx_complete(priv);

	if (status & VNET_INT_RX_PACKET)
		vnet_rx_process(priv);

	if (status & VNET_INT_LINK_CHANGE) {
		u32 hw_status = ioread32(priv->regs + VNET_STATUS);

		if (hw_status & VNET_STATUS_LINK_UP) {
			netif_carrier_on(priv->ndev);
			netdev_info(priv->ndev, "link up\n");
		} else {
			netif_carrier_off(priv->ndev);
			netdev_info(priv->ndev, "link down\n");
		}
	}

	if (status & VNET_INT_ERROR) {
		priv->stats.rx_errors++;
		netdev_err(priv->ndev, "hardware error interrupt\n");
	}

	return IRQ_HANDLED;
}

/* ---- Net Device Operations ---- */

/*
 * vnet_open -- Bring the interface up
 *
 * Allocates DMA rings, registers with the platform, enables interrupts,
 * and starts the hardware.
 */
static int vnet_open(struct net_device *ndev)
{
	struct vnet_priv *priv = netdev_priv(ndev);
	u32 status;
	int err;

	/* Allocate RX ring and buffers (coherent DMA) */
	err = vnet_rx_alloc_ring(priv);
	if (err) {
		netdev_err(ndev, "failed to allocate RX ring (%d)\n", err);
		return err;
	}

	/* Initialize simple TX state (no ring allocation needed) */
	priv->tx_skb = NULL;
	priv->tx_dma_addr = 0;
	priv->tx_len = 0;
	priv->tx_busy = false;

	/*
	 * Register the RX ring with the platform simulator.
	 * The platform needs kernel VAs to write synthetic packets
	 * into the RX buffers.
	 */
	vnet_hw_set_rx_ring(priv->pdev, priv->rx_descs, priv->rx_count,
			    priv->rx_bufs_va, VNET_MAX_PKT_LEN);

	/*
	 * Start the platform's simple TX completion engine.
	 * In this mode the driver writes DMA addr + length to TX
	 * registers and rings a doorbell. The platform detects the
	 * doorbell and fires TX_COMPLETE — no descriptor ring needed.
	 */
	vnet_hw_start_simple_tx(priv->pdev);

	/* Program RX ring registers */
	iowrite32(priv->rx_descs_dma, priv->regs + VNET_RX_RING_ADDR);
	iowrite32(priv->rx_count, priv->regs + VNET_RX_RING_SIZE);
	iowrite32(0, priv->regs + VNET_RX_RING_HEAD);
	iowrite32(0, priv->regs + VNET_RX_RING_TAIL);

	/* Clear TX registers (no ring, used for single-shot TX) */
	iowrite32(0, priv->regs + VNET_TX_RING_ADDR);
	iowrite32(0, priv->regs + VNET_TX_RING_SIZE);
	iowrite32(0, priv->regs + VNET_TX_RING_HEAD);

	/* Enable interrupts */
	vnet_irq_enable(priv);

	/*
	 * Enable controller: TX + RX + ring mode (for RX ring).
	 * TX uses simple register-based mode despite RING_ENABLE being
	 * set (the platform distinguishes by whether a TX ring VA was
	 * registered).
	 */
	iowrite32(VNET_CTRL_ENABLE | VNET_CTRL_TX_ENABLE |
		  VNET_CTRL_RX_ENABLE | VNET_CTRL_RING_ENABLE,
		  priv->regs + VNET_CTRL);

	/* Check link status */
	status = ioread32(priv->regs + VNET_STATUS);
	if (status & VNET_STATUS_LINK_UP)
		netif_carrier_on(ndev);
	else
		netif_carrier_off(ndev);

	/* Allow the stack to start sending packets */
	netif_start_queue(ndev);

	netdev_info(ndev, "interface opened (RX ring=%u, simple TX)\n",
		    priv->rx_count);
	return 0;
}

/*
 * vnet_stop -- Bring the interface down
 *
 * Stops the hardware, disables interrupts, unregisters rings from the
 * platform, and frees all DMA allocations.
 */
static int vnet_stop(struct net_device *ndev)
{
	struct vnet_priv *priv = netdev_priv(ndev);

	/* Stop the stack from sending more packets */
	netif_stop_queue(ndev);

	/* Disable interrupts */
	vnet_irq_disable(priv);

	/* Disable the controller */
	iowrite32(0, priv->regs + VNET_CTRL);

	netif_carrier_off(ndev);

	/* Stop platform simulation engines */
	vnet_hw_clear_rx_ring(priv->pdev);
	vnet_hw_stop_simple_tx(priv->pdev);

	/* If a TX is in flight, unmap and free it */
	if (priv->tx_skb) {
		dma_unmap_single(&priv->pdev->dev, priv->tx_dma_addr,
				 priv->tx_len, DMA_TO_DEVICE);
		dev_kfree_skb(priv->tx_skb);
		priv->tx_skb = NULL;
	}

	/* Free RX DMA resources */
	vnet_rx_free_ring(priv);

	netdev_info(ndev, "interface stopped\n");
	return 0;
}

/*
 * vnet_xmit -- Transmit a packet using streaming DMA (no ring)
 *
 * Simple register-based TX:
 *   1. Map skb->data with dma_map_single (DMA_TO_DEVICE)
 *   2. Write the DMA address to VNET_TX_RING_ADDR register
 *   3. Write the packet length to VNET_TX_RING_SIZE register
 *   4. Write 1 to VNET_TX_RING_HEAD as doorbell
 *   5. Stop the TX queue (we can only handle one packet at a time)
 *
 * The platform detects the doorbell, "transmits" the packet, and
 * fires TX_COMPLETE. Our ISR then unmaps the DMA, frees the skb,
 * and wakes the queue.
 */
static netdev_tx_t vnet_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct vnet_priv *priv = netdev_priv(ndev);
	struct device *dev = &priv->pdev->dev;
	dma_addr_t dma_addr;

	/* Single TX slot -- should not be called when busy */
	if (priv->tx_busy) {
		netdev_warn(ndev, "xmit called while TX busy\n");
		return NETDEV_TX_BUSY;
	}

	/*
	 * Map the skb data for DMA (streaming mapping).
	 *
	 * dma_map_single() pins the memory and returns a DMA address
	 * that the device can use to read the data. DMA_TO_DEVICE
	 * means the device will only read (TX direction).
	 *
	 * After this call, the CPU must NOT touch skb->data until
	 * dma_unmap_single() is called in the completion handler.
	 */
	dma_addr = dma_map_single(dev, skb->data, skb->len, DMA_TO_DEVICE);
	if (dma_mapping_error(dev, dma_addr)) {
		priv->stats.tx_dropped++;
		dev_kfree_skb(skb);
		return NETDEV_TX_OK;
	}

	/* Save skb and mapping info for the completion handler */
	priv->tx_skb = skb;
	priv->tx_dma_addr = dma_addr;
	priv->tx_len = skb->len;
	priv->tx_busy = true;

	/*
	 * Write the DMA address and length to hardware registers.
	 * In a real simple NIC, these registers tell the TX DMA
	 * engine where to find the packet data and how long it is.
	 */
	iowrite32((u32)dma_addr, priv->regs + VNET_TX_RING_ADDR);
	iowrite32(skb->len, priv->regs + VNET_TX_RING_SIZE);

	/*
	 * Stop the queue -- we can only handle one packet at a time.
	 * The TX_COMPLETE ISR will call netif_wake_queue().
	 */
	netif_stop_queue(ndev);

	/*
	 * Ring the doorbell: writing TX_RING_HEAD tells the hardware
	 * "there is a packet to transmit." The hardware reads the
	 * address and length from the registers above.
	 */
	iowrite32(1, priv->regs + VNET_TX_RING_HEAD);

	return NETDEV_TX_OK;
}

/*
 * vnet_get_stats64 -- Return device statistics
 */
static void vnet_get_stats64(struct net_device *ndev,
			     struct rtnl_link_stats64 *s)
{
	struct vnet_priv *priv = netdev_priv(ndev);

	s->tx_packets = priv->stats.tx_packets;
	s->tx_bytes   = priv->stats.tx_bytes;
	s->rx_packets = priv->stats.rx_packets;
	s->rx_bytes   = priv->stats.rx_bytes;
	s->tx_errors  = priv->stats.tx_errors;
	s->rx_errors  = priv->stats.rx_errors;
	s->tx_dropped = priv->stats.tx_dropped;
	s->rx_dropped = priv->stats.rx_dropped;
}

/*
 * vnet_set_mac_address -- Change the device MAC address
 */
static int vnet_set_mac_address(struct net_device *ndev, void *addr)
{
	struct vnet_priv *priv = netdev_priv(ndev);
	struct sockaddr *sa = addr;

	if (!is_valid_ether_addr(sa->sa_data))
		return -EADDRNOTAVAIL;

	vnet_write_mac_to_hw(priv->regs, sa->sa_data);
	eth_hw_addr_set(ndev, sa->sa_data);

	netdev_info(ndev, "MAC address changed to %pM\n", ndev->dev_addr);
	return 0;
}

/* ---- Net Device Ops Table ---- */

static const struct net_device_ops vnet_netdev_ops = {
	.ndo_open            = vnet_open,
	.ndo_stop            = vnet_stop,
	.ndo_start_xmit      = vnet_xmit,
	.ndo_get_stats64     = vnet_get_stats64,
	.ndo_set_mac_address = vnet_set_mac_address,
};

/* ---- PCI Driver probe / remove ---- */

static int vnet_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct net_device *ndev;
	struct vnet_priv *priv;
	int err;

	/* Enable the PCI device */
	err = pci_enable_device(pdev);
	if (err)
		return err;

	/*
	 * Enable bus mastering -- required for DMA.
	 * This sets the Bus Master bit in the PCI command register,
	 * allowing the device to initiate DMA transfers on the bus.
	 */
	pci_set_master(pdev);

	/*
	 * Set DMA mask -- tell the kernel this device can address 32 bits.
	 * dma_set_mask_and_coherent sets both the streaming and coherent
	 * DMA masks.  For a 64-bit capable device, use DMA_BIT_MASK(64).
	 */
	err = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
	if (err) {
		dev_err(&pdev->dev, "32-bit DMA not supported\n");
		goto err_clear_master;
	}

	/* Allocate the net_device with private data */
	ndev = alloc_netdev(sizeof(struct vnet_priv), "vnet%d",
			    NET_NAME_USER, ether_setup);
	if (!ndev) {
		err = -ENOMEM;
		goto err_clear_master;
	}

	priv = netdev_priv(ndev);
	priv->ndev = ndev;
	priv->pdev = pdev;

	/* Map the hardware register file */
	priv->regs = vnet_hw_map_bar0(pdev);
	if (!priv->regs) {
		err = -EIO;
		goto err_free_ndev;
	}

	SET_NETDEV_DEV(ndev, &pdev->dev);
	pci_set_drvdata(pdev, ndev);

	ndev->netdev_ops = &vnet_netdev_ops;
	ndev->watchdog_timeo = 5 * HZ;
	ndev->min_mtu = VNET_MIN_MTU;
	ndev->max_mtu = VNET_MAX_MTU;

	/* Assign a random MAC and program it into hardware */
	eth_hw_addr_random(ndev);
	vnet_write_mac_to_hw(priv->regs, ndev->dev_addr);

	/*
	 * Request IRQ -- register our ISR with the kernel.
	 * IRQF_SHARED: allow sharing (standard for PCI).
	 * The last argument (dev_id) is passed to our ISR and must be
	 * non-NULL for shared interrupts (used to identify our device).
	 */
	err = request_irq(pdev->irq, vnet_isr, IRQF_SHARED,
			  DRV_NAME, priv);
	if (err) {
		netdev_err(ndev, "failed to request IRQ %d (%d)\n",
			   pdev->irq, err);
		goto err_unmap;
	}

	/* Start with interrupts disabled until open() */
	vnet_irq_disable(priv);

	err = register_netdev(ndev);
	if (err)
		goto err_free_irq;

	netdev_info(ndev, "IRQ+DMA driver loaded (IRQ %d, MAC %pM)\n",
		    pdev->irq, ndev->dev_addr);
	return 0;

err_free_irq:
	free_irq(pdev->irq, priv);
err_unmap:
	vnet_hw_unmap_bar0(pdev);
err_free_ndev:
	free_netdev(ndev);
err_clear_master:
	pci_clear_master(pdev);
	pci_disable_device(pdev);
	return err;
}

static void vnet_remove(struct pci_dev *pdev)
{
	struct net_device *ndev = pci_get_drvdata(pdev);
	struct vnet_priv *priv = netdev_priv(ndev);

	unregister_netdev(ndev);

	/* Free the IRQ -- must happen after unregister (no more ISR calls) */
	free_irq(pdev->irq, priv);

	vnet_hw_unmap_bar0(pdev);
	free_netdev(ndev);
	pci_clear_master(pdev);
	pci_disable_device(pdev);
}

/* ---- PCI ID Table & Driver Registration ---- */

static const struct pci_device_id vnet_pci_ids[] = {
	{ PCI_DEVICE(VNET_VENDOR_ID, VNET_DEVICE_ID) },
	{ 0 },
};
MODULE_DEVICE_TABLE(pci, vnet_pci_ids);

static struct pci_driver vnet_pci_driver = {
	.name     = DRV_NAME,
	.id_table = vnet_pci_ids,
	.probe    = vnet_probe,
	.remove   = vnet_remove,
};

module_pci_driver(vnet_pci_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Network Driver Curriculum");
MODULE_DESCRIPTION("Part 4: Interrupts, RX Ring & Simple TX DMA");
