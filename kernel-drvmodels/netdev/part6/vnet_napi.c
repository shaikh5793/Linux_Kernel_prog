// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 *
 * Part 6: NAPI Polling
 *
 * Concepts introduced (over Part 5):
 *   - napi_struct and netif_napi_add / netif_napi_del
 *   - napi_schedule_prep / __napi_schedule in ISR
 *   - NAPI poll function with budget handling
 *   - Interrupt coalescing: disable RX IRQ in ISR, re-enable after poll
 *   - napi_complete_done to signal polling is done
 *   - netif_receive_skb replaces netif_rx (NAPI-aware packet delivery)
 *
 * Carries forward from Part 5:
 *   - TX ring with streaming DMA (unchanged)
 *   - RX ring with coherent DMA (unchanged, but processed by NAPI now)
 *
 * The key insight: Part 5's interrupt-driven RX fires one interrupt per
 * packet. Under load, this wastes CPU on interrupt entry/exit overhead.
 * NAPI solves this: the first RX interrupt schedules a poll function,
 * which processes multiple packets in a single softirq pass without
 * per-packet interrupts.
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

#define DRV_NAME "vnet_napi"

#define VNET_TX_RING_ENTRIES	256	/* TX ring descriptor count */
#define VNET_RX_RING_ENTRIES	64	/* RX ring descriptor count */
#define VNET_MAX_PKT_LEN	1518

/* ---- Ring Buffer Data Structures ---- */

struct vnet_ring {
	struct vnet_hw_desc *desc;	/* descriptor array VA */
	dma_addr_t desc_dma;		/* descriptor array DMA */
	struct sk_buff **skbs;		/* parallel skb tracking (TX only) */
	dma_addr_t *dma_addrs;		/* per-descriptor DMA addr for unmap */
	u32 head;
	u32 tail;
	u32 count;
};

struct vnet_priv {
	struct net_device *ndev;
	struct pci_dev *pdev;
	void __iomem *regs;
	struct net_device_stats stats;

	struct vnet_ring tx_ring;
	bool tx_stopped;

	/* RX ring (coherent DMA, from Part 5) */
	struct vnet_hw_desc *rx_descs;
	dma_addr_t rx_descs_dma;
	void **rx_bufs_va;
	dma_addr_t *rx_bufs_dma;
	u32 rx_count;
	u32 rx_tail;

	/* NEW in Part 6: NAPI support */
	struct napi_struct napi;
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

static void vnet_enable_irqs(void __iomem *regs, u32 bits)
{
	iowrite32(ioread32(regs + VNET_INT_MASK) & ~bits,
		  regs + VNET_INT_MASK);
}

static void vnet_disable_irqs(void __iomem *regs, u32 bits)
{
	iowrite32(ioread32(regs + VNET_INT_MASK) | bits,
		  regs + VNET_INT_MASK);
}

/* ---- TX Ring Helpers ---- */

static bool vnet_ring_full(struct vnet_ring *ring)
{
	return ((ring->head + 1) % ring->count) == ring->tail;
}

static bool vnet_ring_empty(struct vnet_ring *ring)
{
	return ring->head == ring->tail;
}

/* ---- TX Ring Allocation ---- */

static int vnet_alloc_tx_ring(struct vnet_priv *priv)
{
	struct vnet_ring *ring = &priv->tx_ring;
	struct device *dev = &priv->pdev->dev;
	size_t desc_size;

	ring->count = VNET_TX_RING_ENTRIES;
	desc_size = ring->count * sizeof(struct vnet_hw_desc);

	/* Allocate descriptor array in DMA-coherent memory */
	ring->desc = dma_alloc_coherent(dev, desc_size, &ring->desc_dma,
					GFP_KERNEL);
	if (!ring->desc)
		return -ENOMEM;

	ring->skbs = kcalloc(ring->count, sizeof(struct sk_buff *), GFP_KERNEL);
	if (!ring->skbs)
		goto err_desc;

	ring->dma_addrs = kcalloc(ring->count, sizeof(dma_addr_t), GFP_KERNEL);
	if (!ring->dma_addrs)
		goto err_skbs;

	ring->head = 0;
	ring->tail = 0;
	return 0;

err_skbs:
	kfree(ring->skbs);
err_desc:
	dma_free_coherent(dev, desc_size, ring->desc, ring->desc_dma);
	ring->desc = NULL;
	return -ENOMEM;
}

static void vnet_free_tx_ring(struct vnet_priv *priv)
{
	struct vnet_ring *ring = &priv->tx_ring;
	struct device *dev = &priv->pdev->dev;
	u32 i;

	if (!ring->desc)
		return;

	for (i = 0; i < ring->count; i++) {
		if (ring->skbs[i]) {
			dma_unmap_single(dev, ring->dma_addrs[i],
					 ring->skbs[i]->len, DMA_TO_DEVICE);
			dev_kfree_skb_any(ring->skbs[i]);
		}
	}

	dma_free_coherent(dev, ring->count * sizeof(struct vnet_hw_desc),
			  ring->desc, ring->desc_dma);
	ring->desc = NULL;
	kfree(ring->skbs);
	kfree(ring->dma_addrs);
}

/* ---- RX Ring Allocation (carried from Part 5) ---- */

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

	priv->rx_count = VNET_RX_RING_ENTRIES;
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

/* ---- NAPI Poll (NEW in Part 6) ---- */

/*
 * NAPI poll function -- called by the kernel's softirq when NAPI is scheduled.
 *
 * Part 5 called vnet_rx_process() directly in the ISR, which fired once per
 * packet. Under load (thousands of packets/sec), this wastes CPU on interrupt
 * entry/exit. NAPI batches the processing.
 *
 * Process up to budget packets. If we didn't exhaust the budget, we're
 * caught up -- tell NAPI we're done and re-enable interrupts.
 */
static int vnet_napi_poll(struct napi_struct *napi, int budget)
{
	struct vnet_priv *priv = container_of(napi, struct vnet_priv, napi);
	struct net_device *ndev = priv->ndev;
	u32 tail = priv->rx_tail;
	int work_done = 0;

	/* Process received packets from the RX ring, up to budget */
	while (work_done < budget) {
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

		/*
		 * NAPI-aware packet delivery: netif_receive_skb() instead of
		 * Part 5's netif_rx(). This is the correct call from NAPI
		 * poll context -- it feeds directly into the network stack
		 * without going through per-CPU backlog queues.
		 */
		netif_receive_skb(skb);

		priv->stats.rx_packets++;
		priv->stats.rx_bytes += pkt_len;
		work_done++;

repost:
		/* Give the descriptor back to hardware */
		desc->len = VNET_MAX_PKT_LEN;
		desc->status = 0;
		desc->flags = VNET_DESC_OWN;

		tail = (tail + 1) % priv->rx_count;
	}

	priv->rx_tail = tail;

	/*
	 * If we processed fewer packets than budget, we've drained the ring.
	 * Tell NAPI we're done polling and re-enable RX interrupts so the
	 * next arriving packet triggers a new interrupt -> NAPI cycle.
	 */
	if (work_done < budget) {
		napi_complete_done(napi, work_done);
		vnet_enable_irqs(priv->regs, VNET_INT_RX_PACKET);
	}

	return work_done;
}

/* ---- TX Completion (from Part 5, unchanged) ---- */

static void vnet_tx_complete(struct vnet_priv *priv)
{
	struct vnet_ring *ring = &priv->tx_ring;
	struct device *dev = &priv->pdev->dev;

	while (!vnet_ring_empty(ring)) {
		struct vnet_hw_desc *desc = &ring->desc[ring->tail];

		/* Hardware still owns this descriptor -- stop here */
		if (desc->flags & VNET_DESC_OWN)
			break;

		/* Unmap DMA and free the transmitted skb */
		if (ring->skbs[ring->tail]) {
			dma_unmap_single(dev, ring->dma_addrs[ring->tail],
					 ring->skbs[ring->tail]->len,
					 DMA_TO_DEVICE);
			dev_kfree_skb_any(ring->skbs[ring->tail]);
			ring->skbs[ring->tail] = NULL;
		}

		desc->addr = 0;
		desc->len = 0;
		desc->flags = 0;
		desc->status = 0;

		ring->tail = (ring->tail + 1) % ring->count;
	}

	/* Wake TX queue if we freed some ring entries */
	if (priv->tx_stopped && !vnet_ring_full(ring)) {
		priv->tx_stopped = false;
		netif_wake_queue(priv->ndev);
	}
}

/* ---- Interrupt Handler ---- */

static irqreturn_t vnet_interrupt(int irq, void *data)
{
	struct vnet_priv *priv = data;
	u32 status;

	status = ioread32(priv->regs + VNET_INT_STATUS);
	if (!status)
		return IRQ_NONE;

	/* TX completion: processed directly in ISR (unchanged from Part 5) */
	if (status & VNET_INT_TX_COMPLETE)
		vnet_tx_complete(priv);

	/*
	 * NAPI pattern: instead of processing RX inline (as Part 5 did),
	 * disable RX interrupts and schedule NAPI poll. This avoids
	 * per-packet interrupt overhead under load.
	 *
	 * Part 5 called vnet_rx_process() here, which fired once per packet.
	 * Under load (thousands of packets/sec), that wastes CPU on
	 * interrupt entry/exit. NAPI batches the processing into a single
	 * softirq pass.
	 */
	if (status & VNET_INT_RX_PACKET) {
		if (napi_schedule_prep(&priv->napi)) {
			vnet_disable_irqs(priv->regs, VNET_INT_RX_PACKET);
			__napi_schedule(&priv->napi);
		}
	}

	if (status & VNET_INT_LINK_CHANGE) {
		if (ioread32(priv->regs + VNET_STATUS) & VNET_STATUS_LINK_UP)
			netif_carrier_on(priv->ndev);
		else
			netif_carrier_off(priv->ndev);
	}

	if (status & VNET_INT_ERROR) {
		priv->stats.tx_errors++;
		priv->stats.rx_errors++;
	}

	/* Acknowledge all handled interrupts */
	iowrite32(0, priv->regs + VNET_INT_STATUS);

	return IRQ_HANDLED;
}

/* ---- Net Device Operations ---- */

static int vnet_open(struct net_device *ndev)
{
	struct vnet_priv *priv = netdev_priv(ndev);
	int err;

	/* Allocate TX ring buffer */
	err = vnet_alloc_tx_ring(priv);
	if (err)
		return err;

	/* Allocate RX ring and buffers (coherent DMA) */
	err = vnet_rx_alloc_ring(priv);
	if (err) {
		netdev_err(ndev, "failed to allocate RX ring (%d)\n", err);
		goto err_free_tx;
	}

	/*
	 * Tell the hardware where the TX descriptor ring lives.
	 * Write DMA address and size to registers, then pass the
	 * kernel virtual address to the platform module so its
	 * TX completion engine can walk descriptors directly.
	 */
	iowrite32((u32)priv->tx_ring.desc_dma,
		  priv->regs + VNET_TX_RING_ADDR);
	iowrite32(priv->tx_ring.count, priv->regs + VNET_TX_RING_SIZE);
	iowrite32(0, priv->regs + VNET_TX_RING_HEAD);
	iowrite32(0, priv->regs + VNET_TX_RING_TAIL);

	vnet_hw_set_tx_ring(priv->pdev, priv->tx_ring.desc,
			    priv->tx_ring.count);

	/*
	 * Tell the hardware where the RX descriptor ring and buffers live.
	 * Program RX ring registers and pass kernel VAs to the platform.
	 */
	iowrite32(priv->rx_descs_dma, priv->regs + VNET_RX_RING_ADDR);
	iowrite32(priv->rx_count, priv->regs + VNET_RX_RING_SIZE);
	iowrite32(0, priv->regs + VNET_RX_RING_HEAD);
	iowrite32(0, priv->regs + VNET_RX_RING_TAIL);

	vnet_hw_set_rx_ring(priv->pdev, priv->rx_descs, priv->rx_count,
			    priv->rx_bufs_va, VNET_MAX_PKT_LEN);

	/* Enable controller with TX, RX, and ring support */
	iowrite32(VNET_CTRL_ENABLE | VNET_CTRL_TX_ENABLE |
		  VNET_CTRL_RX_ENABLE | VNET_CTRL_RING_ENABLE,
		  priv->regs + VNET_CTRL);

	/*
	 * NEW in Part 6: Set up NAPI BEFORE enabling interrupts.
	 * This ensures the poll function is ready when the first
	 * RX interrupt fires and schedules NAPI.
	 */
	netif_napi_add(ndev, &priv->napi, vnet_napi_poll);
	napi_enable(&priv->napi);

	/* Enable interrupts */
	vnet_enable_irqs(priv->regs,
			 VNET_INT_TX_COMPLETE | VNET_INT_RX_PACKET |
			 VNET_INT_LINK_CHANGE | VNET_INT_ERROR);

	if (ioread32(priv->regs + VNET_STATUS) & VNET_STATUS_LINK_UP)
		netif_carrier_on(ndev);
	else
		netif_carrier_off(ndev);

	priv->tx_stopped = false;
	netif_start_queue(ndev);

	netdev_info(ndev, "interface opened (TX ring=%u, RX ring=%u, NAPI)\n",
		    priv->tx_ring.count, priv->rx_count);
	return 0;

err_free_tx:
	vnet_free_tx_ring(priv);
	return err;
}

static int vnet_stop(struct net_device *ndev)
{
	struct vnet_priv *priv = netdev_priv(ndev);

	netif_stop_queue(ndev);

	/* Disable all interrupts first */
	vnet_disable_irqs(priv->regs, ~0U);

	/*
	 * NEW in Part 6: Tear down NAPI AFTER disabling interrupts.
	 * napi_disable() waits for any in-progress poll to finish,
	 * so no more RX processing happens after this returns.
	 */
	napi_disable(&priv->napi);
	netif_napi_del(&priv->napi);

	/* Disable controller */
	iowrite32(0, priv->regs + VNET_CTRL);

	netif_carrier_off(ndev);

	/* Stop TX and RX engines and free rings */
	vnet_hw_clear_tx_ring(priv->pdev);
	vnet_hw_clear_rx_ring(priv->pdev);

	vnet_free_tx_ring(priv);
	vnet_rx_free_ring(priv);

	netdev_info(ndev, "interface stopped\n");
	return 0;
}

/*
 * Ring-based TX with DMA (unchanged from Part 5).
 *
 * 1. Check ring space
 * 2. Map packet data for DMA
 * 3. Fill descriptor (addr, len, flags with OWN)
 * 4. Advance head and notify hardware
 * 5. Stop queue if ring is full
 *
 * The hardware picks up the descriptor, clears OWN, and fires
 * a TX_COMPLETE interrupt. Our interrupt handler then frees the skb.
 */
static netdev_tx_t vnet_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct vnet_priv *priv = netdev_priv(ndev);
	struct vnet_ring *ring = &priv->tx_ring;
	struct vnet_hw_desc *desc;
	dma_addr_t dma_addr;

	if (vnet_ring_full(ring)) {
		priv->tx_stopped = true;
		netif_stop_queue(ndev);
		return NETDEV_TX_BUSY;
	}

	/* Map packet data for DMA */
	dma_addr = dma_map_single(&priv->pdev->dev, skb->data, skb->len,
				  DMA_TO_DEVICE);
	if (dma_mapping_error(&priv->pdev->dev, dma_addr)) {
		priv->stats.tx_dropped++;
		dev_kfree_skb(skb);
		return NETDEV_TX_OK;
	}

	/* Fill descriptor and hand it to hardware */
	desc = &ring->desc[ring->head];
	desc->addr = (u32)dma_addr;
	desc->len = skb->len;
	desc->flags = VNET_DESC_OWN | VNET_DESC_SOP | VNET_DESC_EOP;
	desc->status = 0;

	ring->skbs[ring->head] = skb;
	ring->dma_addrs[ring->head] = dma_addr;
	ring->head = (ring->head + 1) % ring->count;

	/* Tell hardware about the new descriptor (doorbell write) */
	iowrite32(ring->head, priv->regs + VNET_TX_RING_HEAD);

	priv->stats.tx_packets++;
	priv->stats.tx_bytes += skb->len;

	if (vnet_ring_full(ring)) {
		priv->tx_stopped = true;
		netif_stop_queue(ndev);
	}

	return NETDEV_TX_OK;
}

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

static int vnet_set_mac_address(struct net_device *ndev, void *addr)
{
	struct vnet_priv *priv = netdev_priv(ndev);
	struct sockaddr *sa = addr;

	if (!is_valid_ether_addr(sa->sa_data))
		return -EADDRNOTAVAIL;

	vnet_write_mac_to_hw(priv->regs, sa->sa_data);
	eth_hw_addr_set(ndev, sa->sa_data);
	return 0;
}

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

	err = pci_enable_device(pdev);
	if (err)
		return err;

	/*
	 * Enable bus mastering -- required for DMA.
	 * This sets the Bus Master bit in the PCI command register,
	 * allowing the device to initiate DMA transactions.
	 */
	pci_set_master(pdev);

	/* Set 32-bit DMA addressing */
	err = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
	if (err)
		goto err_disable;

	ndev = alloc_netdev(sizeof(struct vnet_priv), "vnet%d",
			    NET_NAME_USER, ether_setup);
	if (!ndev) {
		err = -ENOMEM;
		goto err_disable;
	}

	priv = netdev_priv(ndev);
	priv->ndev = ndev;
	priv->pdev = pdev;

	priv->regs = vnet_hw_map_bar0(pdev);
	if (!priv->regs) {
		err = -EIO;
		goto err_free_ndev;
	}

	SET_NETDEV_DEV(ndev, &pdev->dev);
	pci_set_drvdata(pdev, ndev);

	err = request_irq(pdev->irq, vnet_interrupt, IRQF_SHARED,
			  DRV_NAME, priv);
	if (err)
		goto err_unmap;

	ndev->netdev_ops = &vnet_netdev_ops;
	eth_hw_addr_random(ndev);
	vnet_write_mac_to_hw(priv->regs, ndev->dev_addr);

	err = register_netdev(ndev);
	if (err)
		goto err_free_irq;

	netdev_info(ndev, "NAPI driver loaded (MAC %pM)\n", ndev->dev_addr);
	return 0;

err_free_irq:
	free_irq(pdev->irq, priv);
err_unmap:
	vnet_hw_unmap_bar0(pdev);
err_free_ndev:
	free_netdev(ndev);
err_disable:
	pci_disable_device(pdev);
	return err;
}

static void vnet_remove(struct pci_dev *pdev)
{
	struct net_device *ndev = pci_get_drvdata(pdev);
	struct vnet_priv *priv = netdev_priv(ndev);

	unregister_netdev(ndev);
	free_irq(pdev->irq, priv);
	vnet_hw_unmap_bar0(pdev);
	free_netdev(ndev);
	pci_disable_device(pdev);
}

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
MODULE_DESCRIPTION("Part 6: NAPI Polling");
