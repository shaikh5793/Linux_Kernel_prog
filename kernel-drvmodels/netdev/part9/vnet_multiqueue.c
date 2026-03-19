// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 *
 * vnet_multiqueue.c - Part 9: Multi-Queue TX/RX
 *
 * Builds on Part 8 (PHY/MDIO) and adds:
 *   - alloc_netdev_mqs() with 4 TX and 4 RX queues
 *   - Per-queue ring buffers (arrays of vnet_ring)
 *   - Per-queue NAPI instances
 *   - Per-queue TX completion
 *   - ndo_select_queue for queue selection
 *   - ethtool get_channels / set_channels
 */

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/phy.h>
#include <linux/mdio.h>
#include <linux/io.h>

#include "../vnet-platform/vnet_hw_interface.h"

#define DRV_NAME    "vnet_multiqueue"
#define DRV_VERSION "8.0.0"

#define VNET_RING_SIZE    256
#define VNET_MAX_PKT_LEN  1518

/* ---------- Data structures ---------- */

struct vnet_ring {
	struct vnet_hw_desc *desc;
	dma_addr_t desc_dma;
	struct sk_buff **skbs;
	dma_addr_t *dma_addrs;
	u32 head;
	u32 tail;
	u32 count;
};

/* RX ring -- coherent DMA for descriptors and buffers */
struct vnet_rx_ring {
	struct vnet_hw_desc *descs;
	dma_addr_t descs_dma;
	void **bufs_va;
	dma_addr_t *bufs_dma;
	u32 count;
	u32 tail;
};

struct vnet_priv;

struct vnet_queue {
	struct vnet_ring     tx_ring;
	struct vnet_rx_ring  rx_ring;
	struct napi_struct napi;
	struct vnet_priv  *priv;
	int                idx;
	bool               tx_stopped;
};

struct vnet_priv {
	struct net_device       *ndev;
	struct pci_dev          *pdev;
	void __iomem            *regs;
	struct net_device_stats  stats;
	struct mii_bus          *mii_bus;
	struct vnet_queue        queues[VNET_NUM_QUEUES];
	int                      num_queues;
	u64                      napi_polls;
	u64                      tx_ring_full_count;
};

/* ---------- Helpers ---------- */

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

static inline u32 vnet_ring_space(struct vnet_ring *ring)
{
	u32 used;

	if (ring->head >= ring->tail)
		used = ring->head - ring->tail;
	else
		used = ring->count - ring->tail + ring->head;

	return ring->count - used - 1;
}

/* ---------- Ring allocation / free ---------- */

static int vnet_alloc_tx_ring(struct pci_dev *pdev, struct vnet_ring *ring)
{
	ring->count = VNET_RING_SIZE;
	ring->head  = 0;
	ring->tail  = 0;

	ring->desc = dma_alloc_coherent(&pdev->dev,
					ring->count * sizeof(struct vnet_hw_desc),
					&ring->desc_dma, GFP_KERNEL);
	if (!ring->desc)
		return -ENOMEM;

	ring->skbs = kcalloc(ring->count, sizeof(struct sk_buff *), GFP_KERNEL);
	if (!ring->skbs)
		goto err_free_desc;

	ring->dma_addrs = kcalloc(ring->count, sizeof(dma_addr_t), GFP_KERNEL);
	if (!ring->dma_addrs)
		goto err_free_skbs;

	return 0;

err_free_skbs:
	kfree(ring->skbs);
	ring->skbs = NULL;
err_free_desc:
	dma_free_coherent(&pdev->dev,
			  ring->count * sizeof(struct vnet_hw_desc),
			  ring->desc, ring->desc_dma);
	ring->desc = NULL;
	return -ENOMEM;
}

static void vnet_free_tx_ring(struct pci_dev *pdev, struct vnet_ring *ring)
{
	u32 i;

	if (!ring->desc)
		return;

	for (i = 0; i < ring->count; i++) {
		if (ring->skbs && ring->skbs[i]) {
			if (ring->dma_addrs && ring->dma_addrs[i])
				dma_unmap_single(&pdev->dev,
						 ring->dma_addrs[i],
						 ring->skbs[i]->len,
						 DMA_TO_DEVICE);
			dev_kfree_skb_any(ring->skbs[i]);
			ring->skbs[i] = NULL;
		}
	}

	kfree(ring->dma_addrs);
	ring->dma_addrs = NULL;
	kfree(ring->skbs);
	ring->skbs = NULL;

	dma_free_coherent(&pdev->dev,
			  ring->count * sizeof(struct vnet_hw_desc),
			  ring->desc, ring->desc_dma);
	ring->desc = NULL;
}

/* ---------- RX ring (coherent DMA) allocation / free ---------- */

static void vnet_rx_ring_free(struct vnet_priv *priv, struct vnet_rx_ring *ring)
{
	struct device *dev = &priv->pdev->dev;
	u32 i;

	if (!ring->descs)
		return;

	/* Free per-slot coherent buffers */
	if (ring->bufs_va) {
		for (i = 0; i < ring->count; i++) {
			if (ring->bufs_va[i])
				dma_free_coherent(dev, VNET_MAX_PKT_LEN,
						  ring->bufs_va[i],
						  ring->bufs_dma[i]);
		}
	}

	kfree(ring->bufs_dma);
	ring->bufs_dma = NULL;
	kfree(ring->bufs_va);
	ring->bufs_va = NULL;

	dma_free_coherent(dev, ring->count * sizeof(struct vnet_hw_desc),
			  ring->descs, ring->descs_dma);
	ring->descs = NULL;
}

static int vnet_rx_ring_alloc(struct vnet_priv *priv, struct vnet_rx_ring *ring)
{
	struct device *dev = &priv->pdev->dev;
	u32 i;

	ring->count = VNET_RING_SIZE;
	ring->tail  = 0;

	/* Allocate descriptor ring (coherent) */
	ring->descs = dma_alloc_coherent(dev,
					 ring->count * sizeof(struct vnet_hw_desc),
					 &ring->descs_dma, GFP_KERNEL);
	if (!ring->descs)
		return -ENOMEM;

	ring->bufs_va = kcalloc(ring->count, sizeof(void *), GFP_KERNEL);
	if (!ring->bufs_va)
		goto err_free_descs;

	ring->bufs_dma = kcalloc(ring->count, sizeof(dma_addr_t), GFP_KERNEL);
	if (!ring->bufs_dma)
		goto err_free_bufs_va;

	/* Allocate per-slot coherent buffers */
	for (i = 0; i < ring->count; i++) {
		ring->bufs_va[i] = dma_alloc_coherent(dev, VNET_MAX_PKT_LEN,
						      &ring->bufs_dma[i],
						      GFP_KERNEL);
		if (!ring->bufs_va[i])
			goto err_free_all;

		ring->descs[i].addr   = (u32)ring->bufs_dma[i];
		ring->descs[i].len    = VNET_MAX_PKT_LEN;
		ring->descs[i].flags  = VNET_DESC_OWN;
		ring->descs[i].status = 0;
	}

	return 0;

err_free_all:
	/* Free any buffers allocated so far */
	while (i-- > 0) {
		dma_free_coherent(dev, VNET_MAX_PKT_LEN,
				  ring->bufs_va[i], ring->bufs_dma[i]);
	}
	kfree(ring->bufs_dma);
	ring->bufs_dma = NULL;
err_free_bufs_va:
	kfree(ring->bufs_va);
	ring->bufs_va = NULL;
err_free_descs:
	dma_free_coherent(dev, ring->count * sizeof(struct vnet_hw_desc),
			  ring->descs, ring->descs_dma);
	ring->descs = NULL;
	return -ENOMEM;
}

/* ---------- MDIO / PHY (from Part 8) ---------- */

static int vnet_mdio_read(struct mii_bus *bus, int phy_addr, int reg)
{
	struct vnet_priv *priv = bus->priv;

	return vnet_hw_mdio_read(priv->pdev, phy_addr, reg);
}

static int vnet_mdio_write(struct mii_bus *bus, int phy_addr, int reg, u16 val)
{
	struct vnet_priv *priv = bus->priv;

	return vnet_hw_mdio_write(priv->pdev, phy_addr, reg, val);
}

static void vnet_adjust_link(struct net_device *ndev)
{
	struct phy_device *phydev = ndev->phydev;

	if (!phydev)
		return;

	if (phydev->link)
		netif_carrier_on(ndev);
	else
		netif_carrier_off(ndev);

	phy_print_status(phydev);
}

static int vnet_mdio_register(struct vnet_priv *priv)
{
	struct mii_bus *bus;
	int err;

	bus = mdiobus_alloc();
	if (!bus)
		return -ENOMEM;

	bus->name  = DRV_NAME "_mdio";
	bus->read  = vnet_mdio_read;
	bus->write = vnet_mdio_write;
	bus->priv  = priv;
	bus->parent = &priv->pdev->dev;
	snprintf(bus->id, MII_BUS_ID_SIZE, "%s-%x",
		 pci_name(priv->pdev), 0);

	err = mdiobus_register(bus);
	if (err) {
		mdiobus_free(bus);
		return err;
	}

	priv->mii_bus = bus;
	return 0;
}

static int vnet_phy_connect(struct net_device *ndev)
{
	struct vnet_priv *priv = netdev_priv(ndev);
	struct phy_device *phydev;

	phydev = phy_find_first(priv->mii_bus);
	if (!phydev) {
		netdev_err(ndev, "no PHY found on MDIO bus\n");
		return -ENODEV;
	}

	phydev = phy_connect(ndev, phydev_name(phydev),
			     vnet_adjust_link, PHY_INTERFACE_MODE_MII);
	if (IS_ERR(phydev)) {
		netdev_err(ndev, "phy_connect failed: %ld\n", PTR_ERR(phydev));
		return PTR_ERR(phydev);
	}

	phy_attached_info(phydev);
	return 0;
}

/* ---------- TX completion ---------- */

static void vnet_tx_complete_queue(struct vnet_priv *priv, int qidx)
{
	struct vnet_queue *q = &priv->queues[qidx];
	struct vnet_ring *ring = &q->tx_ring;

	while (ring->tail != ring->head) {
		u32 idx = ring->tail % ring->count;
		struct vnet_hw_desc *desc = &ring->desc[idx];

		/* Descriptor is done when OWN bit is clear */
		if (desc->flags & VNET_DESC_OWN)
			break;

		if (ring->dma_addrs[idx]) {
			dma_unmap_single(&priv->pdev->dev,
					 ring->dma_addrs[idx],
					 ring->skbs[idx]->len,
					 DMA_TO_DEVICE);
			ring->dma_addrs[idx] = 0;
		}

		if (ring->skbs[idx]) {
			priv->stats.tx_packets++;
			priv->stats.tx_bytes += ring->skbs[idx]->len;
			dev_kfree_skb_any(ring->skbs[idx]);
			ring->skbs[idx] = NULL;
		}

		desc->addr = 0;
		desc->len = 0;
		desc->flags = 0;
		desc->status = 0;

		ring->tail++;
	}

	if (q->tx_stopped && vnet_ring_space(ring) >= 1) {
		q->tx_stopped = false;
		netif_tx_wake_queue(netdev_get_tx_queue(priv->ndev, qidx));
	}
}

/* ---------- NAPI poll ---------- */

static int vnet_napi_poll(struct napi_struct *napi, int budget)
{
	struct vnet_queue *q = container_of(napi, struct vnet_queue, napi);
	struct vnet_priv *priv = q->priv;
	struct vnet_rx_ring *ring = &q->rx_ring;
	struct net_device *ndev = priv->ndev;
	int work_done = 0;

	priv->napi_polls++;

	while (work_done < budget) {
		u32 idx = ring->tail % ring->count;
		struct vnet_hw_desc *desc = &ring->descs[idx];
		struct sk_buff *skb;
		u32 pkt_len;

		/* Descriptor done when OWN bit is clear */
		if (desc->flags & VNET_DESC_OWN)
			break;

		if (!(desc->status & VNET_DESC_STATUS_OK)) {
			priv->stats.rx_errors++;
			goto next;
		}

		pkt_len = desc->status & VNET_DESC_STATUS_LEN_MASK;

		/* Coherent DMA -- no sync needed, just memcpy */
		skb = netdev_alloc_skb_ip_align(ndev, pkt_len);
		if (unlikely(!skb)) {
			priv->stats.rx_dropped++;
			goto next;
		}

		memcpy(skb_put(skb, pkt_len), ring->bufs_va[idx], pkt_len);
		skb->protocol = eth_type_trans(skb, ndev);

		napi_gro_receive(napi, skb);

		priv->stats.rx_packets++;
		priv->stats.rx_bytes += pkt_len;
		work_done++;

next:
		/* Hand descriptor back to hardware */
		desc->addr   = (u32)ring->bufs_dma[idx];
		desc->len    = VNET_MAX_PKT_LEN;
		desc->flags  = VNET_DESC_OWN;
		desc->status = 0;

		ring->tail = (ring->tail + 1) % ring->count;
	}

	if (work_done < budget) {
		napi_complete_done(napi, work_done);
		vnet_enable_irqs(priv->regs, VNET_INT_RX_PACKET);
	}

	return work_done;
}

/* ---------- ISR ---------- */

static irqreturn_t vnet_interrupt(int irq, void *data)
{
	struct vnet_priv *priv = data;
	u32 status;
	int i;

	status = ioread32(priv->regs + VNET_INT_STATUS);
	if (!status)
		return IRQ_NONE;

	/* Per-queue TX completion */
	for (i = 0; i < priv->num_queues; i++) {
		if (status & VNET_INT_TXQ_COMPLETE(i))
			vnet_tx_complete_queue(priv, i);
	}

	/* RX via NAPI (schedule queue 0 NAPI for now) */
	if (status & VNET_INT_RX_PACKET) {
		if (napi_schedule_prep(&priv->queues[0].napi)) {
			vnet_disable_irqs(priv->regs, VNET_INT_RX_PACKET);
			__napi_schedule(&priv->queues[0].napi);
		}
	}

	/* Link change */
	if (status & VNET_INT_LINK_CHANGE) {
		if (priv->ndev->phydev)
			phy_mac_interrupt(priv->ndev->phydev);
	}

	/* Error */
	if (status & VNET_INT_ERROR)
		netdev_err(priv->ndev, "hardware error (status=0x%08x)\n",
			   status);

	/* ACK all */
	iowrite32(status, priv->regs + VNET_INT_STATUS);

	return IRQ_HANDLED;
}

/* ---------- ndo operations ---------- */

static int vnet_open(struct net_device *ndev)
{
	struct vnet_priv *priv = netdev_priv(ndev);
	int i, err;

	for (i = 0; i < priv->num_queues; i++) {
		struct vnet_queue *q = &priv->queues[i];

		/* Allocate TX ring */
		err = vnet_alloc_tx_ring(priv->pdev, &q->tx_ring);
		if (err) {
			netdev_err(ndev, "failed to alloc TX ring %d\n", i);
			goto err_free_rings;
		}

		/* Allocate RX ring (coherent DMA) */
		err = vnet_rx_ring_alloc(priv, &q->rx_ring);
		if (err) {
			netdev_err(ndev, "failed to alloc RX ring %d\n", i);
			vnet_free_tx_ring(priv->pdev, &q->tx_ring);
			goto err_free_rings;
		}

		/* Program per-queue TX registers */
		iowrite32((u32)q->tx_ring.desc_dma,
			  priv->regs + VNET_TXQ_ADDR(i));
		iowrite32(q->tx_ring.count,
			  priv->regs + VNET_TXQ_SIZE(i));
		iowrite32(0, priv->regs + VNET_TXQ_HEAD(i));
		iowrite32(0, priv->regs + VNET_TXQ_TAIL(i));

		/* Register TX ring with platform */
		vnet_hw_set_tx_ring_queue(priv->pdev, i,
					  q->tx_ring.desc,
					  q->tx_ring.count);

		/* Program per-queue RX registers */
		iowrite32((u32)q->rx_ring.descs_dma,
			  priv->regs + VNET_RXQ_ADDR(i));
		iowrite32(q->rx_ring.count,
			  priv->regs + VNET_RXQ_SIZE(i));
		iowrite32(0, priv->regs + VNET_RXQ_HEAD(i));
		iowrite32(0, priv->regs + VNET_RXQ_TAIL(i));

		/* Register RX ring with platform (queue 0 only for legacy API) */
		if (i == 0)
			vnet_hw_set_rx_ring(priv->pdev, q->rx_ring.descs,
					    q->rx_ring.count,
					    q->rx_ring.bufs_va,
					    VNET_MAX_PKT_LEN);

		/* Register NAPI for this queue */
		netif_napi_add(ndev, &q->napi, vnet_napi_poll);
		napi_enable(&q->napi);
	}

	/* IRQ registered in probe */

	/* Connect PHY and start it */
	err = vnet_phy_connect(ndev);
	if (err)
		goto err_free_all;

	phy_start(ndev->phydev);

	/* Enable controller */
	iowrite32(VNET_CTRL_ENABLE | VNET_CTRL_TX_ENABLE |
		  VNET_CTRL_RX_ENABLE | VNET_CTRL_RING_ENABLE,
		  priv->regs + VNET_CTRL);

	/* Enable interrupts */
	vnet_enable_irqs(priv->regs, VNET_INT_RX_PACKET | VNET_INT_LINK_CHANGE |
			 VNET_INT_ERROR);
	for (i = 0; i < priv->num_queues; i++)
		vnet_enable_irqs(priv->regs, VNET_INT_TXQ_COMPLETE(i));

	/* Start all TX queues */
	netif_tx_start_all_queues(ndev);

	netdev_info(ndev, "opened with %d queues\n", priv->num_queues);
	return 0;

err_free_all:
	i = priv->num_queues;
err_free_rings:
	while (i-- > 0) {
		struct vnet_queue *q = &priv->queues[i];

		napi_disable(&q->napi);
		netif_napi_del(&q->napi);
		vnet_hw_clear_tx_ring_queue(priv->pdev, i);
		vnet_hw_clear_rx_ring(priv->pdev);
		vnet_rx_ring_free(priv, &q->rx_ring);
		vnet_free_tx_ring(priv->pdev, &q->tx_ring);
	}
	return err;
}

static int vnet_stop(struct net_device *ndev)
{
	struct vnet_priv *priv = netdev_priv(ndev);
	int i;

	netif_tx_stop_all_queues(ndev);

	/* Stop PHY */
	if (ndev->phydev) {
		phy_stop(ndev->phydev);
		phy_disconnect(ndev->phydev);
	}

	/* Disable all interrupts */
	vnet_disable_irqs(priv->regs, ~0U);
	/* IRQ freed in remove */

	/* Disable controller */
	iowrite32(0, priv->regs + VNET_CTRL);

	/* Tear down each queue */
	for (i = 0; i < priv->num_queues; i++) {
		struct vnet_queue *q = &priv->queues[i];

		napi_disable(&q->napi);
		netif_napi_del(&q->napi);
		vnet_hw_clear_tx_ring_queue(priv->pdev, i);
		vnet_hw_clear_rx_ring(priv->pdev);
		vnet_free_tx_ring(priv->pdev, &q->tx_ring);
		vnet_rx_ring_free(priv, &q->rx_ring);
	}

	netdev_info(ndev, "stopped\n");
	return 0;
}

static netdev_tx_t vnet_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct vnet_priv *priv = netdev_priv(ndev);
	u16 qidx = skb_get_queue_mapping(skb);
	struct vnet_queue *q = &priv->queues[qidx];
	struct vnet_ring *ring = &q->tx_ring;
	struct vnet_hw_desc *desc;
	u32 idx;
	dma_addr_t dma;

	if (skb->len > VNET_MAX_PKT_LEN) {
		dev_kfree_skb_any(skb);
		priv->stats.tx_dropped++;
		return NETDEV_TX_OK;
	}

	if (vnet_ring_space(ring) < 1) {
		netif_tx_stop_queue(netdev_get_tx_queue(ndev, qidx));
		q->tx_stopped = true;
		priv->tx_ring_full_count++;
		return NETDEV_TX_BUSY;
	}

	dma = dma_map_single(&priv->pdev->dev, skb->data, skb->len,
			     DMA_TO_DEVICE);
	if (dma_mapping_error(&priv->pdev->dev, dma)) {
		dev_kfree_skb_any(skb);
		priv->stats.tx_dropped++;
		return NETDEV_TX_OK;
	}

	idx = ring->head % ring->count;
	ring->skbs[idx]      = skb;
	ring->dma_addrs[idx] = dma;

	desc = &ring->desc[idx];
	desc->addr  = (u32)dma;
	desc->len   = skb->len;
	desc->flags = VNET_DESC_OWN | VNET_DESC_SOP | VNET_DESC_EOP;
	desc->status = 0;

	ring->head++;

	/* Ring doorbell */
	iowrite32(ring->head, priv->regs + VNET_TXQ_HEAD(qidx));

	return NETDEV_TX_OK;
}

static struct net_device_stats *vnet_get_stats(struct net_device *ndev)
{
	struct vnet_priv *priv = netdev_priv(ndev);

	return &priv->stats;
}

static u16 vnet_select_queue(struct net_device *ndev, struct sk_buff *skb,
			     struct net_device *sb_dev)
{
	return skb_get_hash(skb) % ndev->real_num_tx_queues;
}

static const struct net_device_ops vnet_netdev_ops = {
	.ndo_open       = vnet_open,
	.ndo_stop       = vnet_stop,
	.ndo_start_xmit = vnet_xmit,
	.ndo_get_stats  = vnet_get_stats,
	.ndo_select_queue = vnet_select_queue,
};

/* ---------- ethtool ---------- */

static void vnet_get_drvinfo(struct net_device *ndev,
			     struct ethtool_drvinfo *info)
{
	struct vnet_priv *priv = netdev_priv(ndev);

	strscpy(info->driver, DRV_NAME, sizeof(info->driver));
	strscpy(info->version, DRV_VERSION, sizeof(info->version));
	strscpy(info->bus_info, pci_name(priv->pdev), sizeof(info->bus_info));
}

static void vnet_get_strings(struct net_device *ndev, u32 stringset, u8 *data)
{
	if (stringset == ETH_SS_STATS) {
		strscpy(data, "tx_packets", ETH_GSTRING_LEN);
		data += ETH_GSTRING_LEN;
		strscpy(data, "rx_packets", ETH_GSTRING_LEN);
		data += ETH_GSTRING_LEN;
		strscpy(data, "tx_bytes", ETH_GSTRING_LEN);
		data += ETH_GSTRING_LEN;
		strscpy(data, "rx_bytes", ETH_GSTRING_LEN);
		data += ETH_GSTRING_LEN;
		strscpy(data, "tx_dropped", ETH_GSTRING_LEN);
		data += ETH_GSTRING_LEN;
		strscpy(data, "napi_polls", ETH_GSTRING_LEN);
		data += ETH_GSTRING_LEN;
		strscpy(data, "tx_ring_full", ETH_GSTRING_LEN);
	}
}

static int vnet_get_sset_count(struct net_device *ndev, int sset)
{
	if (sset == ETH_SS_STATS)
		return 7;
	return -EOPNOTSUPP;
}

static void vnet_get_ethtool_stats(struct net_device *ndev,
				   struct ethtool_stats *estats, u64 *data)
{
	struct vnet_priv *priv = netdev_priv(ndev);

	data[0] = priv->stats.tx_packets;
	data[1] = priv->stats.rx_packets;
	data[2] = priv->stats.tx_bytes;
	data[3] = priv->stats.rx_bytes;
	data[4] = priv->stats.tx_dropped;
	data[5] = priv->napi_polls;
	data[6] = priv->tx_ring_full_count;
}

static u32 vnet_get_msglevel(struct net_device *ndev)
{
	return 0;
}

static void vnet_set_msglevel(struct net_device *ndev, u32 msglevel)
{
}

static void vnet_get_ringparam(struct net_device *ndev,
			       struct ethtool_ringparam *ring,
			       struct kernel_ethtool_ringparam *kernel_ring,
			       struct netlink_ext_ack *extack)
{
	ring->rx_max_pending = VNET_RING_SIZE;
	ring->tx_max_pending = VNET_RING_SIZE;
	ring->rx_pending     = VNET_RING_SIZE;
	ring->tx_pending     = VNET_RING_SIZE;
}

static int vnet_get_link_ksettings(struct net_device *ndev,
				   struct ethtool_link_ksettings *cmd)
{
	if (!ndev->phydev)
		return -ENODEV;

	phy_ethtool_ksettings_get(ndev->phydev, cmd);
	return 0;
}

static int vnet_set_link_ksettings(struct net_device *ndev,
				   const struct ethtool_link_ksettings *cmd)
{
	if (!ndev->phydev)
		return -ENODEV;

	return phy_ethtool_ksettings_set(ndev->phydev, cmd);
}

static void vnet_get_channels(struct net_device *ndev,
			      struct ethtool_channels *ch)
{
	struct vnet_priv *priv = netdev_priv(ndev);

	ch->max_combined    = VNET_NUM_QUEUES;
	ch->combined_count  = priv->num_queues;
	ch->max_rx          = 0;
	ch->max_tx          = 0;
	ch->rx_count        = 0;
	ch->tx_count        = 0;
	ch->max_other       = 0;
	ch->other_count     = 0;
}

static int vnet_set_channels(struct net_device *ndev,
			     struct ethtool_channels *ch)
{
	struct vnet_priv *priv = netdev_priv(ndev);

	if (ch->combined_count == 0 || ch->combined_count > VNET_NUM_QUEUES)
		return -EINVAL;

	if (ch->rx_count || ch->tx_count || ch->other_count)
		return -EINVAL;

	if (netif_running(ndev)) {
		netdev_err(ndev, "cannot change channels while interface is up\n");
		return -EBUSY;
	}

	priv->num_queues = ch->combined_count;
	netif_set_real_num_tx_queues(ndev, priv->num_queues);
	netif_set_real_num_rx_queues(ndev, priv->num_queues);

	netdev_info(ndev, "channels set to %d\n", priv->num_queues);
	return 0;
}

static const struct ethtool_ops vnet_ethtool_ops = {
	.get_drvinfo         = vnet_get_drvinfo,
	.get_strings         = vnet_get_strings,
	.get_sset_count      = vnet_get_sset_count,
	.get_ethtool_stats   = vnet_get_ethtool_stats,
	.get_msglevel        = vnet_get_msglevel,
	.set_msglevel        = vnet_set_msglevel,
	.get_ringparam       = vnet_get_ringparam,
	.get_link            = ethtool_op_get_link,
	.get_link_ksettings  = vnet_get_link_ksettings,
	.set_link_ksettings  = vnet_set_link_ksettings,
	.get_channels        = vnet_get_channels,
	.set_channels        = vnet_set_channels,
};

/* ---------- PCI probe / remove ---------- */

static int vnet_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct net_device *ndev;
	struct vnet_priv *priv;
	int err, i;

	err = pci_enable_device(pdev);
	if (err)
		return err;

	pci_set_master(pdev);

	err = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
	if (err) {
		dev_err(&pdev->dev, "no usable DMA configuration\n");
		goto err_disable;
	}

	/* Allocate netdev with multi-queue support */
	ndev = alloc_netdev_mqs(sizeof(struct vnet_priv), "vnet%d",
				NET_NAME_USER, ether_setup,
				VNET_NUM_QUEUES, VNET_NUM_QUEUES);
	if (!ndev) {
		err = -ENOMEM;
		goto err_disable;
	}

	SET_NETDEV_DEV(ndev, &pdev->dev);
	priv = netdev_priv(ndev);
	priv->ndev = ndev;
	priv->pdev = pdev;

	priv->regs = vnet_hw_map_bar0(pdev);
	if (!priv->regs) {
		err = -EIO;
		goto err_free_netdev;
	}

	/* Initialize queue structures */
	priv->num_queues = VNET_NUM_QUEUES;
	for (i = 0; i < VNET_NUM_QUEUES; i++) {
		priv->queues[i].priv = priv;
		priv->queues[i].idx  = i;
		priv->queues[i].tx_stopped = false;
	}

	/* Generate random MAC address and program it to hardware */
	eth_hw_addr_random(ndev);
	vnet_write_mac_to_hw(priv->regs, ndev->dev_addr);

	ndev->netdev_ops  = &vnet_netdev_ops;
	ndev->ethtool_ops = &vnet_ethtool_ops;

	/* Register MDIO bus */
	err = vnet_mdio_register(priv);
	if (err) {
		dev_err(&pdev->dev, "MDIO registration failed: %d\n", err);
		goto err_unmap;
	}

	pci_set_drvdata(pdev, ndev);

	/* Request IRQ early (freed in remove) */
	err = request_irq(pdev->irq, vnet_interrupt, IRQF_SHARED,
			  DRV_NAME, priv);
	if (err) {
		dev_err(&pdev->dev, "request_irq failed: %d\n", err);
		goto err_mdio;
	}

	err = register_netdev(ndev);
	if (err) {
		dev_err(&pdev->dev, "register_netdev failed: %d\n", err);
		goto err_free_irq;
	}

	netdev_info(ndev, "%s v%s: %d TX/RX queues, MAC %pM\n",
		    DRV_NAME, DRV_VERSION, priv->num_queues, ndev->dev_addr);

	return 0;

err_free_irq:
	free_irq(pdev->irq, priv);
err_mdio:
	mdiobus_unregister(priv->mii_bus);
	mdiobus_free(priv->mii_bus);
err_unmap:
	vnet_hw_unmap_bar0(pdev);
err_free_netdev:
	free_netdev(ndev);
err_disable:
	pci_disable_device(pdev);
	return err;
}

static void vnet_remove(struct pci_dev *pdev)
{
	struct net_device *ndev = pci_get_drvdata(pdev);
	struct vnet_priv *priv = netdev_priv(ndev);

	free_irq(pdev->irq, priv);
	unregister_netdev(ndev);

	mdiobus_unregister(priv->mii_bus);
	mdiobus_free(priv->mii_bus);

	vnet_hw_unmap_bar0(pdev);
	free_netdev(ndev);
	pci_disable_device(pdev);
}

/* ---------- PCI ID table ---------- */

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
MODULE_DESCRIPTION("Part 9: Multi-Queue TX/RX");
MODULE_VERSION(DRV_VERSION);
