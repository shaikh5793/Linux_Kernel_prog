// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 *
 * Part 7: ethtool Integration
 *
 * Concepts introduced (over Part 6):
 *   - ethtool_ops structure and registration
 *   - Driver info with pci_name() for bus_info
 *   - Link settings (get/set_link_ksettings)
 *   - Custom statistics (get_strings, get_sset_count, get_ethtool_stats)
 *   - Register dump via ioread32 (get_regs_len, get_regs)
 *   - Link detection (get_link)
 *
 * Builds on: Part 6 NAPI + Part 5 ring buffers
 *
 * LOAD ORDER:
 *   sudo insmod ../vnet-platform/vnet_hw_platform.ko
 *   sudo insmod vnet_ethtool.ko
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/ethtool.h>
#include <linux/io.h>

#include "../vnet-platform/vnet_hw_interface.h"

#define DRV_NAME    "vnet_ethtool"
#define DRV_VERSION "6.0.0"

#define VNET_RING_SIZE    256
#define VNET_MAX_PKT_LEN  1518

/* Ring buffer structure */
struct vnet_ring {
	struct vnet_hw_desc *desc;
	dma_addr_t desc_dma;
	struct sk_buff **skbs;
	dma_addr_t *dma_addrs;
	u32 head;
	u32 tail;
	u32 count;
};

/* NEW: ethtool custom statistics */
enum {
	VNET_STAT_TX_PACKETS,
	VNET_STAT_TX_BYTES,
	VNET_STAT_TX_ERRORS,
	VNET_STAT_TX_DROPPED,
	VNET_STAT_RX_PACKETS,
	VNET_STAT_RX_BYTES,
	VNET_STAT_RX_ERRORS,
	VNET_STAT_RX_DROPPED,
	VNET_STAT_NAPI_POLLS,
	VNET_STAT_TX_RING_FULL,
	VNET_STAT_COUNT,
};

static const char vnet_stat_strings[][ETH_GSTRING_LEN] = {
	"tx_packets",
	"tx_bytes",
	"tx_errors",
	"tx_dropped",
	"rx_packets",
	"rx_bytes",
	"rx_errors",
	"rx_dropped",
	"napi_polls",
	"tx_ring_full",
};

struct vnet_priv {
	struct net_device *ndev;
	struct pci_dev *pdev;
	void __iomem *regs;
	struct net_device_stats stats;

	struct vnet_ring tx_ring;

	/* RX ring -- coherent DMA (matches Part 6) */
	struct vnet_hw_desc *rx_descs;
	dma_addr_t rx_descs_dma;
	void **rx_bufs_va;
	dma_addr_t *rx_bufs_dma;
	u32 rx_count;
	u32 rx_tail;

	bool tx_stopped;

	struct napi_struct napi;

	/* NEW: Extended statistics for ethtool */
	u64 napi_polls;
	u64 tx_ring_full_count;
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

/* ---- Ring Helpers ---- */

static bool vnet_ring_full(struct vnet_ring *ring)
{
	return ((ring->head + 1) % ring->count) == ring->tail;
}

static bool vnet_ring_empty(struct vnet_ring *ring)
{
	return ring->head == ring->tail;
}

static int vnet_alloc_tx_ring(struct device *dev, struct vnet_ring *ring)
{
	size_t desc_size;

	ring->count = VNET_RING_SIZE;
	desc_size = ring->count * sizeof(struct vnet_hw_desc);

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

static void vnet_free_tx_ring(struct device *dev, struct vnet_ring *ring)
{
	u32 i;

	if (!ring->desc)
		return;

	for (i = 0; i < ring->count; i++) {
		if (ring->skbs[i]) {
			dma_unmap_single(dev, ring->dma_addrs[i],
					 ring->skbs[i]->len, DMA_TO_DEVICE);
			dev_kfree_skb_any(ring->skbs[i]);
			ring->skbs[i] = NULL;
		}
	}

	dma_free_coherent(dev, ring->count * sizeof(struct vnet_hw_desc),
			  ring->desc, ring->desc_dma);
	ring->desc = NULL;
	kfree(ring->skbs);
	kfree(ring->dma_addrs);
}

/* ---- RX Ring (coherent DMA) ---- */

static int vnet_rx_alloc_ring(struct vnet_priv *priv)
{
	struct device *dev = &priv->pdev->dev;
	size_t desc_size;
	u32 i;

	priv->rx_count = VNET_RING_SIZE;
	desc_size = priv->rx_count * sizeof(struct vnet_hw_desc);

	priv->rx_descs = dma_alloc_coherent(dev, desc_size,
					    &priv->rx_descs_dma, GFP_KERNEL);
	if (!priv->rx_descs)
		return -ENOMEM;

	priv->rx_bufs_va = kcalloc(priv->rx_count, sizeof(void *), GFP_KERNEL);
	if (!priv->rx_bufs_va)
		goto err_descs;

	priv->rx_bufs_dma = kcalloc(priv->rx_count, sizeof(dma_addr_t),
				    GFP_KERNEL);
	if (!priv->rx_bufs_dma)
		goto err_bufs_va;

	for (i = 0; i < priv->rx_count; i++) {
		priv->rx_bufs_va[i] = dma_alloc_coherent(dev, VNET_MAX_PKT_LEN,
							  &priv->rx_bufs_dma[i],
							  GFP_KERNEL);
		if (!priv->rx_bufs_va[i])
			goto err_bufs;

		priv->rx_descs[i].addr = (u32)priv->rx_bufs_dma[i];
		priv->rx_descs[i].len = VNET_MAX_PKT_LEN;
		priv->rx_descs[i].flags = VNET_DESC_OWN;
		priv->rx_descs[i].status = 0;
	}

	priv->rx_tail = 0;
	return 0;

err_bufs:
	while (i--) {
		dma_free_coherent(dev, VNET_MAX_PKT_LEN,
				  priv->rx_bufs_va[i], priv->rx_bufs_dma[i]);
	}
	kfree(priv->rx_bufs_dma);
err_bufs_va:
	kfree(priv->rx_bufs_va);
err_descs:
	dma_free_coherent(dev, desc_size, priv->rx_descs, priv->rx_descs_dma);
	priv->rx_descs = NULL;
	return -ENOMEM;
}

static void vnet_rx_free_ring(struct vnet_priv *priv)
{
	struct device *dev = &priv->pdev->dev;
	u32 i;

	if (!priv->rx_descs)
		return;

	for (i = 0; i < priv->rx_count; i++) {
		if (priv->rx_bufs_va[i])
			dma_free_coherent(dev, VNET_MAX_PKT_LEN,
					  priv->rx_bufs_va[i],
					  priv->rx_bufs_dma[i]);
	}

	dma_free_coherent(dev, priv->rx_count * sizeof(struct vnet_hw_desc),
			  priv->rx_descs, priv->rx_descs_dma);
	priv->rx_descs = NULL;
	kfree(priv->rx_bufs_va);
	kfree(priv->rx_bufs_dma);
}

/* ---- ethtool Operations (NEW in Part 7) ---- */

static void vnet_get_drvinfo(struct net_device *ndev,
			     struct ethtool_drvinfo *info)
{
	struct vnet_priv *priv = netdev_priv(ndev);

	strscpy(info->driver, DRV_NAME, sizeof(info->driver));
	strscpy(info->version, DRV_VERSION, sizeof(info->version));
	/* Use PCI bus location string -- real drivers do this too */
	strscpy(info->bus_info, pci_name(priv->pdev), sizeof(info->bus_info));
}

static u32 vnet_get_link(struct net_device *ndev)
{
	struct vnet_priv *priv = netdev_priv(ndev);

	return !!(ioread32(priv->regs + VNET_STATUS) & VNET_STATUS_LINK_UP);
}

static int vnet_get_link_ksettings(struct net_device *ndev,
				   struct ethtool_link_ksettings *cmd)
{
	struct vnet_priv *priv = netdev_priv(ndev);
	u32 status = ioread32(priv->regs + VNET_STATUS);

	ethtool_link_ksettings_zero_link_mode(cmd, supported);
	ethtool_link_ksettings_zero_link_mode(cmd, advertising);

	ethtool_link_ksettings_add_link_mode(cmd, supported, 1000baseT_Full);
	ethtool_link_ksettings_add_link_mode(cmd, supported, Autoneg);
	ethtool_link_ksettings_add_link_mode(cmd, advertising, 1000baseT_Full);
	ethtool_link_ksettings_add_link_mode(cmd, advertising, Autoneg);

	if (status & VNET_STATUS_LINK_UP) {
		cmd->base.speed = SPEED_1000;
		cmd->base.duplex = DUPLEX_FULL;
	} else {
		cmd->base.speed = SPEED_UNKNOWN;
		cmd->base.duplex = DUPLEX_UNKNOWN;
	}

	cmd->base.autoneg = AUTONEG_ENABLE;
	cmd->base.port = PORT_TP;

	return 0;
}

static int vnet_set_link_ksettings(struct net_device *ndev,
				   const struct ethtool_link_ksettings *cmd)
{
	if (cmd->base.speed != SPEED_1000 || cmd->base.duplex != DUPLEX_FULL)
		return -EINVAL;

	return 0;
}

static void vnet_get_strings(struct net_device *ndev, u32 sset, u8 *data)
{
	if (sset == ETH_SS_STATS)
		memcpy(data, vnet_stat_strings, sizeof(vnet_stat_strings));
}

static int vnet_get_sset_count(struct net_device *ndev, int sset)
{
	if (sset == ETH_SS_STATS)
		return VNET_STAT_COUNT;
	return -EOPNOTSUPP;
}

static void vnet_get_ethtool_stats(struct net_device *ndev,
				   struct ethtool_stats *estats, u64 *data)
{
	struct vnet_priv *priv = netdev_priv(ndev);

	data[VNET_STAT_TX_PACKETS]   = priv->stats.tx_packets;
	data[VNET_STAT_TX_BYTES]     = priv->stats.tx_bytes;
	data[VNET_STAT_TX_ERRORS]    = priv->stats.tx_errors;
	data[VNET_STAT_TX_DROPPED]   = priv->stats.tx_dropped;
	data[VNET_STAT_RX_PACKETS]   = priv->stats.rx_packets;
	data[VNET_STAT_RX_BYTES]     = priv->stats.rx_bytes;
	data[VNET_STAT_RX_ERRORS]    = priv->stats.rx_errors;
	data[VNET_STAT_RX_DROPPED]   = priv->stats.rx_dropped;
	data[VNET_STAT_NAPI_POLLS]   = priv->napi_polls;
	data[VNET_STAT_TX_RING_FULL] = priv->tx_ring_full_count;
}

static int vnet_get_regs_len(struct net_device *ndev)
{
	return 64 * sizeof(u32);
}

/*
 * Register dump -- reads hardware registers via ioread32.
 * ethtool -d vnet0 displays these values.
 */
static void vnet_get_regs(struct net_device *ndev,
			  struct ethtool_regs *regs, void *data)
{
	struct vnet_priv *priv = netdev_priv(ndev);
	u32 *reg_data = data;
	int i;

	regs->version = 1;
	for (i = 0; i < 64; i++)
		reg_data[i] = ioread32(priv->regs + i * 4);
}

static const struct ethtool_ops vnet_ethtool_ops = {
	.get_drvinfo         = vnet_get_drvinfo,
	.get_link            = vnet_get_link,
	.get_link_ksettings  = vnet_get_link_ksettings,
	.set_link_ksettings  = vnet_set_link_ksettings,
	.get_strings         = vnet_get_strings,
	.get_sset_count      = vnet_get_sset_count,
	.get_ethtool_stats   = vnet_get_ethtool_stats,
	.get_regs_len        = vnet_get_regs_len,
	.get_regs            = vnet_get_regs,
};

/* ---- NAPI Poll (from Part 6) ---- */

static int vnet_napi_poll(struct napi_struct *napi, int budget)
{
	struct vnet_priv *priv = container_of(napi, struct vnet_priv, napi);
	int work_done = 0;

	priv->napi_polls++;

	while (work_done < budget) {
		struct vnet_hw_desc *desc = &priv->rx_descs[priv->rx_tail];
		struct sk_buff *skb;
		u32 len;

		if (desc->flags & VNET_DESC_OWN)
			break;

		if (!(desc->status & VNET_DESC_STATUS_OK)) {
			priv->stats.rx_errors++;
			goto next;
		}

		len = desc->status & VNET_DESC_STATUS_LEN_MASK;
		if (len > VNET_MAX_PKT_LEN) {
			priv->stats.rx_errors++;
			goto next;
		}

		skb = netdev_alloc_skb(priv->ndev, len + NET_IP_ALIGN);
		if (!skb) {
			priv->stats.rx_dropped++;
			goto next;
		}

		skb_reserve(skb, NET_IP_ALIGN);
		memcpy(skb_put(skb, len), priv->rx_bufs_va[priv->rx_tail],
		       len);
		skb->protocol = eth_type_trans(skb, priv->ndev);
		netif_receive_skb(skb);

		priv->stats.rx_packets++;
		priv->stats.rx_bytes += len;
		work_done++;

next:
		desc->flags = VNET_DESC_OWN;
		desc->status = 0;
		priv->rx_tail = (priv->rx_tail + 1) % priv->rx_count;
	}

	if (work_done < budget) {
		napi_complete_done(napi, work_done);
		vnet_enable_irqs(priv->regs, VNET_INT_RX_PACKET);
	}

	return work_done;
}

/* ---- TX Completion ---- */

static void vnet_tx_complete(struct vnet_priv *priv)
{
	struct vnet_ring *ring = &priv->tx_ring;
	struct device *dev = &priv->pdev->dev;

	while (!vnet_ring_empty(ring)) {
		struct vnet_hw_desc *desc = &ring->desc[ring->tail];

		if (desc->flags & VNET_DESC_OWN)
			break;

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

	if (status & VNET_INT_TX_COMPLETE)
		vnet_tx_complete(priv);

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

	iowrite32(0, priv->regs + VNET_INT_STATUS);

	return IRQ_HANDLED;
}

/* ---- Net Device Operations ---- */

static int vnet_open(struct net_device *ndev)
{
	struct vnet_priv *priv = netdev_priv(ndev);
	struct device *dev = &priv->pdev->dev;
	int err;

	err = vnet_alloc_tx_ring(dev, &priv->tx_ring);
	if (err)
		return err;

	err = vnet_rx_alloc_ring(priv);
	if (err) {
		vnet_free_tx_ring(dev, &priv->tx_ring);
		return err;
	}

	/* Set up TX ring in hardware */
	iowrite32((u32)priv->tx_ring.desc_dma,
		  priv->regs + VNET_TX_RING_ADDR);
	iowrite32(priv->tx_ring.count, priv->regs + VNET_TX_RING_SIZE);
	iowrite32(0, priv->regs + VNET_TX_RING_HEAD);
	iowrite32(0, priv->regs + VNET_TX_RING_TAIL);

	vnet_hw_set_tx_ring(priv->pdev, priv->tx_ring.desc,
			    priv->tx_ring.count);

	/* Set up RX ring in hardware */
	iowrite32((u32)priv->rx_descs_dma,
		  priv->regs + VNET_RX_RING_ADDR);
	iowrite32(priv->rx_count, priv->regs + VNET_RX_RING_SIZE);
	iowrite32(0, priv->regs + VNET_RX_RING_HEAD);
	iowrite32(0, priv->regs + VNET_RX_RING_TAIL);

	vnet_hw_set_rx_ring(priv->pdev, priv->rx_descs, priv->rx_count,
			    priv->rx_bufs_va, VNET_MAX_PKT_LEN);

	/* Enable controller */
	iowrite32(VNET_CTRL_ENABLE | VNET_CTRL_TX_ENABLE |
		  VNET_CTRL_RX_ENABLE | VNET_CTRL_RING_ENABLE,
		  priv->regs + VNET_CTRL);

	netif_napi_add(ndev, &priv->napi, vnet_napi_poll);
	napi_enable(&priv->napi);

	vnet_enable_irqs(priv->regs,
			 VNET_INT_TX_COMPLETE | VNET_INT_RX_PACKET |
			 VNET_INT_LINK_CHANGE | VNET_INT_ERROR);

	if (ioread32(priv->regs + VNET_STATUS) & VNET_STATUS_LINK_UP)
		netif_carrier_on(ndev);
	else
		netif_carrier_off(ndev);

	priv->tx_stopped = false;
	netif_start_queue(ndev);
	return 0;
}

static int vnet_stop(struct net_device *ndev)
{
	struct vnet_priv *priv = netdev_priv(ndev);
	struct device *dev = &priv->pdev->dev;

	netif_stop_queue(ndev);
	napi_disable(&priv->napi);
	netif_napi_del(&priv->napi);

	vnet_disable_irqs(priv->regs, ~0U);
	iowrite32(0, priv->regs + VNET_CTRL);

	vnet_hw_clear_tx_ring(priv->pdev);
	vnet_hw_clear_rx_ring(priv->pdev);
	vnet_free_tx_ring(dev, &priv->tx_ring);
	vnet_rx_free_ring(priv);

	netif_carrier_off(ndev);
	return 0;
}

static netdev_tx_t vnet_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct vnet_priv *priv = netdev_priv(ndev);
	struct vnet_ring *ring = &priv->tx_ring;
	struct vnet_hw_desc *desc;
	dma_addr_t dma_addr;

	if (vnet_ring_full(ring)) {
		priv->tx_stopped = true;
		priv->tx_ring_full_count++;
		netif_stop_queue(ndev);
		return NETDEV_TX_BUSY;
	}

	dma_addr = dma_map_single(&priv->pdev->dev, skb->data, skb->len,
				  DMA_TO_DEVICE);
	if (dma_mapping_error(&priv->pdev->dev, dma_addr)) {
		priv->stats.tx_dropped++;
		dev_kfree_skb(skb);
		return NETDEV_TX_OK;
	}

	desc = &ring->desc[ring->head];
	desc->addr = (u32)dma_addr;
	desc->len = skb->len;
	desc->flags = VNET_DESC_OWN | VNET_DESC_SOP | VNET_DESC_EOP;
	desc->status = 0;

	ring->skbs[ring->head] = skb;
	ring->dma_addrs[ring->head] = dma_addr;
	ring->head = (ring->head + 1) % ring->count;

	iowrite32(ring->head, priv->regs + VNET_TX_RING_HEAD);

	priv->stats.tx_packets++;
	priv->stats.tx_bytes += skb->len;

	if (vnet_ring_full(ring)) {
		priv->tx_stopped = true;
		priv->tx_ring_full_count++;
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

	pci_set_master(pdev);

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
	/* NEW: Register ethtool operations */
	ndev->ethtool_ops = &vnet_ethtool_ops;
	eth_hw_addr_random(ndev);
	vnet_write_mac_to_hw(priv->regs, ndev->dev_addr);

	err = register_netdev(ndev);
	if (err)
		goto err_free_irq;

	netdev_info(ndev, "ethtool driver loaded (MAC %pM)\n",
		    ndev->dev_addr);
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
MODULE_DESCRIPTION("Part 7: ethtool Integration");
MODULE_VERSION(DRV_VERSION);
