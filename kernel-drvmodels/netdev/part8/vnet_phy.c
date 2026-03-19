// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 *
 * vnet_phy.c - Part 8: PHY & MDIO Bus
 *
 * Builds on Part 7 (ethtool) and adds:
 *   - MDIO bus allocation and registration
 *   - PHY connection via phy_connect()
 *   - PHY-driven link state via adjust_link callback
 *   - phy_start/phy_stop in open/stop
 *   - ethtool link settings served by generic PHY helpers
 */

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/phy.h>
#include <linux/mdio.h>

#include "../vnet-platform/vnet_hw_interface.h"

#define DRV_NAME	"vnet_phy"
#define DRV_VERSION	"7.0.0"

#define VNET_RING_SIZE	256
#define VNET_BUF_SIZE	2048
#define VNET_MAX_PKT_LEN 1518
#define VNET_NAPI_WEIGHT 64

/* ---------- TX descriptor ring structure ---------- */

struct vnet_ring {
	struct vnet_hw_desc	*desc;
	dma_addr_t		desc_dma;
	struct sk_buff		**skbs;
	dma_addr_t		*dma_addrs;
	u32			head;
	u32			tail;
	u32			count;
};

/* ---------- private device structure ---------- */

struct vnet_priv {
	struct net_device	*ndev;
	struct pci_dev		*pdev;
	void __iomem		*regs;

	struct vnet_ring	tx_ring;

	/* RX ring -- coherent DMA (matches Part 6) */
	struct vnet_hw_desc	*rx_descs;
	dma_addr_t		rx_descs_dma;
	void			**rx_bufs_va;
	dma_addr_t		*rx_bufs_dma;
	u32			rx_count;
	u32			rx_tail;

	struct napi_struct	napi;

	struct mii_bus		*mii_bus;
};

/* ================================================================
 * Helpers
 * ================================================================ */

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

/* ================================================================
 * TX ring helpers
 * ================================================================ */

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

/* ================================================================
 * RX ring helpers (coherent DMA)
 * ================================================================ */

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
		priv->rx_bufs_va[i] = dma_alloc_coherent(dev,
					VNET_MAX_PKT_LEN,
					&priv->rx_bufs_dma[i],
					GFP_KERNEL);
		if (!priv->rx_bufs_va[i])
			goto err_bufs;

		priv->rx_descs[i].addr   = (u32)priv->rx_bufs_dma[i];
		priv->rx_descs[i].len    = VNET_MAX_PKT_LEN;
		priv->rx_descs[i].flags  = VNET_DESC_OWN;
		priv->rx_descs[i].status = 0;
	}

	priv->rx_tail = 0;
	return 0;

err_bufs:
	while (i--) {
		dma_free_coherent(dev, VNET_MAX_PKT_LEN,
				  priv->rx_bufs_va[i],
				  priv->rx_bufs_dma[i]);
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

/* ================================================================
 * TX path
 * ================================================================ */

static netdev_tx_t vnet_start_xmit(struct sk_buff *skb,
				    struct net_device *ndev)
{
	struct vnet_priv *priv = netdev_priv(ndev);
	struct vnet_ring *tx = &priv->tx_ring;
	struct vnet_hw_desc *desc;
	dma_addr_t dma_addr;
	int idx;

	if (vnet_ring_full(tx)) {
		netif_stop_queue(ndev);
		return NETDEV_TX_BUSY;
	}

	idx = tx->head;

	dma_addr = dma_map_single(&priv->pdev->dev, skb->data, skb->len,
				  DMA_TO_DEVICE);
	if (dma_mapping_error(&priv->pdev->dev, dma_addr)) {
		dev_kfree_skb_any(skb);
		ndev->stats.tx_dropped++;
		return NETDEV_TX_OK;
	}

	desc = &tx->desc[idx];
	desc->addr = (u32)dma_addr;
	desc->len = skb->len;
	desc->flags = VNET_DESC_OWN | VNET_DESC_SOP | VNET_DESC_EOP;
	desc->status = 0;

	tx->skbs[idx] = skb;
	tx->dma_addrs[idx] = dma_addr;
	tx->head = (tx->head + 1) % tx->count;

	/* Notify hardware */
	iowrite32(tx->head, priv->regs + VNET_TX_RING_HEAD);

	if (vnet_ring_full(tx))
		netif_stop_queue(ndev);

	return NETDEV_TX_OK;
}

static void vnet_tx_complete(struct vnet_priv *priv)
{
	struct device *dev = &priv->pdev->dev;
	struct vnet_ring *tx = &priv->tx_ring;

	while (!vnet_ring_empty(tx)) {
		struct vnet_hw_desc *desc = &tx->desc[tx->tail];

		/* Check if hardware still owns this descriptor */
		if (desc->flags & VNET_DESC_OWN)
			break;

		if (tx->skbs[tx->tail]) {
			dma_unmap_single(dev, tx->dma_addrs[tx->tail],
					 tx->skbs[tx->tail]->len,
					 DMA_TO_DEVICE);
			dev_consume_skb_any(tx->skbs[tx->tail]);
			tx->skbs[tx->tail] = NULL;
		}

		desc->addr = 0;
		desc->len = 0;
		desc->flags = 0;
		desc->status = 0;

		priv->ndev->stats.tx_packets++;
		priv->ndev->stats.tx_bytes += desc->len;

		tx->tail = (tx->tail + 1) % tx->count;
	}

	if (netif_queue_stopped(priv->ndev) && !vnet_ring_full(tx))
		netif_wake_queue(priv->ndev);
}

/* ================================================================
 * RX path / NAPI
 * ================================================================ */

static int vnet_rx_poll(struct napi_struct *napi, int budget)
{
	struct vnet_priv *priv = container_of(napi, struct vnet_priv, napi);
	int work_done = 0;

	/* Also reclaim completed TX descriptors */
	vnet_tx_complete(priv);

	while (work_done < budget) {
		struct vnet_hw_desc *desc = &priv->rx_descs[priv->rx_tail];
		struct sk_buff *skb;
		u32 len;

		/* Check if hardware has released this descriptor */
		if (desc->flags & VNET_DESC_OWN)
			break;

		if (!(desc->status & VNET_DESC_STATUS_OK)) {
			priv->ndev->stats.rx_errors++;
			goto next;
		}

		len = desc->status & VNET_DESC_STATUS_LEN_MASK;

		/* Allocate skb and copy received data (coherent, no sync needed) */
		skb = netdev_alloc_skb(priv->ndev, len + NET_IP_ALIGN);
		if (!skb) {
			priv->ndev->stats.rx_dropped++;
			goto next;
		}

		skb_reserve(skb, NET_IP_ALIGN);
		memcpy(skb_put(skb, len),
		       priv->rx_bufs_va[priv->rx_tail], len);
		skb->protocol = eth_type_trans(skb, priv->ndev);

		napi_gro_receive(napi, skb);

		priv->ndev->stats.rx_packets++;
		priv->ndev->stats.rx_bytes += len;
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

/* ================================================================
 * Interrupt handler
 * ================================================================ */

static irqreturn_t vnet_irq_handler(int irq, void *data)
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
			vnet_disable_irqs(priv->regs,
					  VNET_INT_RX_PACKET | VNET_INT_TX_COMPLETE);
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
		priv->ndev->stats.tx_errors++;
		priv->ndev->stats.rx_errors++;
	}

	/* Acknowledge all handled interrupts */
	iowrite32(0, priv->regs + VNET_INT_STATUS);

	return IRQ_HANDLED;
}

/* ================================================================
 * MDIO bus callbacks
 * ================================================================ */

static int vnet_mdio_read(struct mii_bus *bus, int phy_addr, int reg)
{
	struct vnet_priv *priv = bus->priv;

	return vnet_hw_mdio_read(priv->pdev, phy_addr, reg);
}

static int vnet_mdio_write(struct mii_bus *bus, int phy_addr,
			   int reg, u16 val)
{
	struct vnet_priv *priv = bus->priv;

	return vnet_hw_mdio_write(priv->pdev, phy_addr, reg, val);
}

/* ================================================================
 * PHY adjust_link callback
 * ================================================================ */

static void vnet_adjust_link(struct net_device *ndev)
{
	struct phy_device *phydev = ndev->phydev;

	if (phydev->link)
		netif_carrier_on(ndev);
	else
		netif_carrier_off(ndev);
}

/* ================================================================
 * Net device operations
 * ================================================================ */

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

	/* Set up and enable NAPI before enabling interrupts */
	netif_napi_add(ndev, &priv->napi, vnet_rx_poll);
	napi_enable(&priv->napi);

	/* Enable interrupts (IRQ registered in probe) */
	vnet_enable_irqs(priv->regs,
			 VNET_INT_TX_COMPLETE | VNET_INT_RX_PACKET |
			 VNET_INT_LINK_CHANGE | VNET_INT_ERROR);

	/* Start PHY state machine */
	if (ndev->phydev)
		phy_start(ndev->phydev);

	netif_start_queue(ndev);

	return 0;
}

static int vnet_stop(struct net_device *ndev)
{
	struct vnet_priv *priv = netdev_priv(ndev);
	struct device *dev = &priv->pdev->dev;

	netif_stop_queue(ndev);

	/* Stop PHY state machine */
	if (ndev->phydev)
		phy_stop(ndev->phydev);

	napi_disable(&priv->napi);
	netif_napi_del(&priv->napi);

	vnet_disable_irqs(priv->regs, ~0U);

	/* Disable controller (IRQ freed in remove) */
	iowrite32(0, priv->regs + VNET_CTRL);

	vnet_hw_clear_tx_ring(priv->pdev);
	vnet_hw_clear_rx_ring(priv->pdev);
	vnet_free_tx_ring(dev, &priv->tx_ring);
	vnet_rx_free_ring(priv);

	netif_carrier_off(ndev);

	return 0;
}

static const struct net_device_ops vnet_netdev_ops = {
	.ndo_open	= vnet_open,
	.ndo_stop	= vnet_stop,
	.ndo_start_xmit	= vnet_start_xmit,
};

/* ================================================================
 * Ethtool operations
 * ================================================================ */

static void vnet_get_drvinfo(struct net_device *ndev,
			     struct ethtool_drvinfo *info)
{
	struct vnet_priv *priv = netdev_priv(ndev);

	strscpy(info->driver, DRV_NAME, sizeof(info->driver));
	strscpy(info->version, DRV_VERSION, sizeof(info->version));
	strscpy(info->bus_info, pci_name(priv->pdev), sizeof(info->bus_info));
}

static u32 vnet_get_link(struct net_device *ndev)
{
	return ndev->phydev ? ndev->phydev->link : 0;
}

static const struct ethtool_ops vnet_ethtool_ops = {
	.get_drvinfo		= vnet_get_drvinfo,
	.get_link		= vnet_get_link,
	.get_link_ksettings	= phy_ethtool_get_link_ksettings,
	.set_link_ksettings	= phy_ethtool_set_link_ksettings,
};

/* ================================================================
 * PCI probe / remove
 * ================================================================ */

static int vnet_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct net_device *ndev;
	struct vnet_priv *priv;
	struct phy_device *phydev;
	char phy_id[MII_BUS_ID_SIZE + 3];
	int err;

	err = pci_enable_device(pdev);
	if (err)
		return err;

	err = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
	if (err) {
		err = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
		if (err) {
			dev_err(&pdev->dev, "No usable DMA configuration\n");
			goto err_disable;
		}
	}

	pci_set_master(pdev);

	ndev = alloc_netdev(sizeof(struct vnet_priv), "vnet%d",
			    NET_NAME_USER, ether_setup);
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
		goto err_free_ndev;
	}

	/* Set a random MAC address and program it into hardware */
	eth_hw_addr_random(ndev);
	vnet_write_mac_to_hw(priv->regs, ndev->dev_addr);

	ndev->netdev_ops  = &vnet_netdev_ops;
	ndev->ethtool_ops = &vnet_ethtool_ops;

	pci_set_drvdata(pdev, ndev);

	err = request_irq(pdev->irq, vnet_irq_handler, IRQF_SHARED,
			  DRV_NAME, priv);
	if (err)
		goto err_unmap;

	err = register_netdev(ndev);
	if (err)
		goto err_free_irq;

	/* --- MDIO bus setup --- */
	priv->mii_bus = mdiobus_alloc();
	if (!priv->mii_bus) {
		err = -ENOMEM;
		goto err_unregister;
	}

	priv->mii_bus->name   = "vnet_mdio";
	snprintf(priv->mii_bus->id, MII_BUS_ID_SIZE, "vnet-mdio-%s",
		 pci_name(pdev));
	priv->mii_bus->priv   = priv;
	priv->mii_bus->parent = &pdev->dev;
	priv->mii_bus->read   = vnet_mdio_read;
	priv->mii_bus->write  = vnet_mdio_write;

	err = mdiobus_register(priv->mii_bus);
	if (err) {
		dev_err(&pdev->dev, "Failed to register MDIO bus\n");
		goto err_mdio_free;
	}

	/* --- Connect to PHY --- */
	snprintf(phy_id, sizeof(phy_id), PHY_ID_FMT,
		 priv->mii_bus->id, VNET_PHY_ADDR);

	phydev = phy_connect(ndev, phy_id, vnet_adjust_link,
			     PHY_INTERFACE_MODE_GMII);
	if (IS_ERR(phydev)) {
		dev_err(&pdev->dev, "Failed to connect PHY\n");
		err = PTR_ERR(phydev);
		goto err_mdio_unreg;
	}

	netdev_info(ndev, "%s: PHY connected at addr %d (MAC %pM)\n",
		    DRV_NAME, VNET_PHY_ADDR, ndev->dev_addr);

	return 0;

err_mdio_unreg:
	mdiobus_unregister(priv->mii_bus);
err_mdio_free:
	mdiobus_free(priv->mii_bus);
err_unregister:
	unregister_netdev(ndev);
err_free_irq:
	free_irq(pdev->irq, priv);
err_unmap:
	pci_set_drvdata(pdev, NULL);
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

	/* Disconnect PHY */
	if (ndev->phydev)
		phy_disconnect(ndev->phydev);

	/* Tear down MDIO bus */
	mdiobus_unregister(priv->mii_bus);
	mdiobus_free(priv->mii_bus);

	unregister_netdev(ndev);
	free_irq(pdev->irq, priv);
	vnet_hw_unmap_bar0(pdev);
	pci_set_drvdata(pdev, NULL);
	free_netdev(ndev);
	pci_disable_device(pdev);
}

/* ================================================================
 * PCI driver glue
 * ================================================================ */

static const struct pci_device_id vnet_pci_tbl[] = {
	{ PCI_DEVICE(VNET_VENDOR_ID, VNET_DEVICE_ID) },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, vnet_pci_tbl);

static struct pci_driver vnet_pci_driver = {
	.name     = DRV_NAME,
	.id_table = vnet_pci_tbl,
	.probe    = vnet_probe,
	.remove   = vnet_remove,
};

module_pci_driver(vnet_pci_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Network Driver Curriculum");
MODULE_DESCRIPTION("Part 8: PHY & MDIO Bus");
MODULE_VERSION(DRV_VERSION);
