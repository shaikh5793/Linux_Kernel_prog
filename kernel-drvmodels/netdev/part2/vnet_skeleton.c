// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 *
 * Part 2: PCI Driver Skeleton
 *
 * Concepts introduced:
 *   - PCI driver registration with module_pci_driver()
 *   - PCI ID table matching (VID/DID)
 *   - probe() / remove() lifecycle
 *   - pci_enable_device / pci_disable_device
 *   - Register mapping via vnet_hw_map_bar0 (equivalent to pci_iomap)
 *   - Register access with ioread32 / iowrite32
 *   - net_device allocation with alloc_etherdev
 *   - net_device_ops (open/stop/xmit stub)
 *   - SET_NETDEV_DEV for proper device hierarchy
 *   - Carrier on/off based on hardware link status
 *
 * This is the simplest possible PCI network driver. It registers a
 * net_device with the kernel but does not transmit or receive packets.
 * That comes in Part 3.
 *
 * LOAD ORDER:
 *   sudo insmod ../vnet-platform/vnet_hw_platform.ko   # hardware first
 *   sudo insmod vnet_skeleton.ko                # then this driver
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/io.h>

#include "../vnet-platform/vnet_hw_interface.h"

#define DRV_NAME "vnet_skeleton"

/* Private driver data, embedded in net_device via alloc_etherdev */
struct vnet_priv {
	struct net_device *ndev;
	struct pci_dev *pdev;
	void __iomem *regs;	/* hardware register base */
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

/* ---- Net Device Operations ---- */

static int vnet_open(struct net_device *ndev)
{
	struct vnet_priv *priv = netdev_priv(ndev);

	/* Enable the controller */
	iowrite32(VNET_CTRL_ENABLE, priv->regs + VNET_CTRL);

	/* Check link status from hardware */
	if (ioread32(priv->regs + VNET_STATUS) & VNET_STATUS_LINK_UP)
		netif_carrier_on(ndev);
	else
		netif_carrier_off(ndev);

	netif_start_queue(ndev);
	netdev_info(ndev, "interface opened\n");
	return 0;
}

static int vnet_stop(struct net_device *ndev)
{
	struct vnet_priv *priv = netdev_priv(ndev);

	netif_stop_queue(ndev);
	netif_carrier_off(ndev);

	/* Disable the controller */
	iowrite32(0, priv->regs + VNET_CTRL);

	netdev_info(ndev, "interface closed\n");
	return 0;
}

/*
 * Stub transmit -- just drop the packet.
 */
static netdev_tx_t vnet_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	dev_kfree_skb(skb);
	ndev->stats.tx_dropped++;
	return NETDEV_TX_OK;
}

static const struct net_device_ops vnet_netdev_ops = {
	.ndo_open       = vnet_open,
	.ndo_stop       = vnet_stop,
	.ndo_start_xmit = vnet_xmit,
};

/* ---- PCI Driver probe / remove ---- */

/*
 * probe() is called by the PCI core when it finds a device matching
 * our ID table. This is where we claim the device and set up the
 * network interface.
 *
 * In a real driver, this replaces module_init() -- the PCI subsystem
 * calls probe() automatically when the hardware is detected.
 */
static int vnet_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct net_device *ndev;
	struct vnet_priv *priv;
	int err;

	/* Enable the PCI device (sets up config space, power state) */
	err = pci_enable_device(pdev);
	if (err)
		return err;

	/* Allocate net_device with space for our private data */
	ndev = alloc_netdev(sizeof(struct vnet_priv), "vnet%d",
			    NET_NAME_USER, ether_setup);
	if (!ndev) {
		err = -ENOMEM;
		goto err_disable;
	}

	priv = netdev_priv(ndev);
	priv->ndev = ndev;
	priv->pdev = pdev;

	/*
	 * Map the hardware register file.
	 *
	 * A real driver would call:
	 *   priv->regs = pci_iomap(pdev, 0, 0);
	 *
	 * Our virtual hardware's registers live in kernel RAM,
	 * which pci_iomap cannot map (ioremap rejects RAM addresses).
	 * The platform module provides this equivalent function.
	 */
	priv->regs = vnet_hw_map_bar0(pdev);
	if (!priv->regs) {
		err = -EIO;
		goto err_free_ndev;
	}

	/* Link net_device to the PCI device for sysfs */
	SET_NETDEV_DEV(ndev, &pdev->dev);
	pci_set_drvdata(pdev, ndev);

	/* Configure the network interface */
	ndev->netdev_ops = &vnet_netdev_ops;
	eth_hw_addr_random(ndev);

	/* Program random MAC into hardware registers */
	vnet_write_mac_to_hw(priv->regs, ndev->dev_addr);

	err = register_netdev(ndev);
	if (err)
		goto err_unmap;

	netdev_info(ndev, "skeleton driver loaded (MAC %pM)\n",
		    ndev->dev_addr);
	return 0;

err_unmap:
	vnet_hw_unmap_bar0(pdev);
err_free_ndev:
	free_netdev(ndev);
err_disable:
	pci_disable_device(pdev);
	return err;
}

/*
 * remove() is called when the driver is unbound from the device.
 * This is where we undo everything probe() set up.
 */
static void vnet_remove(struct pci_dev *pdev)
{
	struct net_device *ndev = pci_get_drvdata(pdev);

	unregister_netdev(ndev);
	vnet_hw_unmap_bar0(pdev);
	free_netdev(ndev);
	pci_disable_device(pdev);
}

/* PCI ID table -- the PCI core matches this against devices on the bus */
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

/* module_pci_driver replaces module_init/module_exit boilerplate */
module_pci_driver(vnet_pci_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Network Driver Curriculum");
MODULE_DESCRIPTION("Part 2: PCI Driver Skeleton");
