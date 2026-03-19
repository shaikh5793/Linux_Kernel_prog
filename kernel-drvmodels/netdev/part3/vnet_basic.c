// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 *
 * Part 3: Net Device Ops & Calling Context
 *
 * This driver is a comprehensive net_device_ops skeleton that documents
 * the calling context for each operation. It serves as the reference map
 * for understanding when and how the kernel invokes each ndo callback.
 *
 * Concepts introduced (over Part 2):
 *   - Full net_device_ops table with 9 operations
 *   - Calling context documentation (RTNL, softirq, atomic, RCU)
 *   - Simple TX: count packets and free (no ring buffers, no DMA)
 *   - ndo_get_stats64 for statistics
 *   - ndo_set_mac_address with validation and HW register write
 *   - ndo_change_mtu with hardware limit validation
 *   - ndo_validate_addr for pre-open MAC validation
 *   - ndo_tx_timeout for stuck TX queue handling
 *   - ndo_set_rx_mode for multicast/promiscuous filter setup
 *
 * NO interrupts in this part.
 * NO DMA in this part.
 *
 * LOAD ORDER:
 *   sudo insmod ../vnet-platform/vnet_hw_platform.ko
 *   sudo insmod vnet_basic.ko
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/io.h>

#include "../vnet-platform/vnet_hw_interface.h"

#define DRV_NAME "vnet_basic"

/* Hardware MTU limits for this controller */
#define VNET_MIN_MTU	68	/* Minimum IPv4 MTU per RFC 791 */
#define VNET_MAX_MTU	9000	/* Jumbo frame support */

struct vnet_priv {
	struct net_device *ndev;
	struct pci_dev *pdev;
	void __iomem *regs;
	struct net_device_stats stats;
	unsigned long tx_timeout_count;
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

/*
 * ndo_open -- Bring the interface up
 *
 * Calling context: Process context, RTNL lock held.
 * Called when: User runs "ip link set <dev> up" or equivalent.
 * Driver must: Enable hardware, program registers, start TX queues.
 *              May sleep (process context). RTNL serializes against
 *              other configuration changes.
 *
 * Note: ndo_validate_addr is called before ndo_open to ensure the
 * MAC address is valid before we program it into hardware.
 */
static int vnet_open(struct net_device *ndev)
{
	struct vnet_priv *priv = netdev_priv(ndev);
	u32 status;

	/* Enable controller with TX and RX */
	iowrite32(VNET_CTRL_ENABLE | VNET_CTRL_TX_ENABLE |
		  VNET_CTRL_RX_ENABLE, priv->regs + VNET_CTRL);

	/* Check link status from hardware */
	status = ioread32(priv->regs + VNET_STATUS);
	if (status & VNET_STATUS_LINK_UP)
		netif_carrier_on(ndev);
	else
		netif_carrier_off(ndev);

	/* Allow the stack to start sending packets */
	netif_start_queue(ndev);

	netdev_info(ndev, "interface opened\n");
	return 0;
}

/*
 * ndo_stop -- Bring the interface down
 *
 * Calling context: Process context, RTNL lock held.
 * Called when: User runs "ip link set <dev> down" or equivalent.
 * Driver must: Stop TX queues, disable hardware, free resources
 *              allocated in open. May sleep.
 *
 * After ndo_stop returns, the stack guarantees no more calls to
 * ndo_start_xmit for this device.
 */
static int vnet_stop(struct net_device *ndev)
{
	struct vnet_priv *priv = netdev_priv(ndev);

	/* Stop the stack from sending more packets */
	netif_stop_queue(ndev);

	/* Disable the controller hardware */
	iowrite32(0, priv->regs + VNET_CTRL);

	netif_carrier_off(ndev);

	netdev_info(ndev, "interface stopped\n");
	return 0;
}

/*
 * ndo_start_xmit -- Transmit a packet
 *
 * Calling context: softirq (BH) context, per-CPU. NOT under RTNL.
 *                  Can be called concurrently on different CPUs for
 *                  different TX queues. Must not sleep.
 * Called when: The network stack has a packet to transmit.
 * Driver must: Either transmit the skb or free it. Must return
 *              NETDEV_TX_OK (skb consumed) or NETDEV_TX_BUSY
 *              (skb not consumed, stack will retry -- avoid this).
 *
 * This is a stub implementation: count the packet and free it.
 */
static netdev_tx_t vnet_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct vnet_priv *priv = netdev_priv(ndev);

	/* Update statistics -- simple per-device counters */
	priv->stats.tx_packets++;
	priv->stats.tx_bytes += skb->len;

	/* Free the skb -- we consumed it (no real TX hardware yet) */
	dev_kfree_skb(skb);

	return NETDEV_TX_OK;
}

/*
 * ndo_get_stats64 -- Return device statistics
 *
 * Calling context: Can be called from ANY context, including under
 *                  RCU read-side lock, softirq, or process context.
 *                  Must not sleep. Must not take RTNL.
 * Called when: User queries stats via "ip -s link show", /proc/net/dev,
 *             ethtool -S, or SNMP polling.
 * Driver must: Fill the rtnl_link_stats64 structure. Should use
 *              per-CPU counters + u64_stats_sync in production for
 *              lock-free, tear-free 64-bit reads on 32-bit archs.
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
 * ndo_set_mac_address -- Change the device MAC address
 *
 * Calling context: Process context, RTNL lock held.
 * Called when: User runs "ip link set <dev> address XX:XX:XX:XX:XX:XX".
 * Driver must: Validate the address, program it into hardware registers,
 *              then update net_device->dev_addr via eth_hw_addr_set().
 *              Return -EADDRNOTAVAIL for invalid addresses.
 */
static int vnet_set_mac_address(struct net_device *ndev, void *addr)
{
	struct vnet_priv *priv = netdev_priv(ndev);
	struct sockaddr *sa = addr;

	if (!is_valid_ether_addr(sa->sa_data))
		return -EADDRNOTAVAIL;

	/* Program the new MAC into hardware registers */
	vnet_write_mac_to_hw(priv->regs, sa->sa_data);

	/* Update the kernel's copy */
	eth_hw_addr_set(ndev, sa->sa_data);

	netdev_info(ndev, "MAC address changed to %pM\n", ndev->dev_addr);
	return 0;
}

/*
 * ndo_change_mtu -- Change the Maximum Transmission Unit
 *
 * Calling context: Process context, RTNL lock held.
 * Called when: User runs "ip link set <dev> mtu <value>".
 *              The stack has already validated mtu against
 *              ndev->min_mtu and ndev->max_mtu (set in probe).
 * Driver must: Program the new MTU into hardware (MAX_FRAME register).
 *              If the device is running, may need to reallocate buffers.
 *              Return 0 on success, negative errno on failure.
 */
static int vnet_change_mtu(struct net_device *ndev, int new_mtu)
{
	struct vnet_priv *priv = netdev_priv(ndev);
	u32 max_frame;

	/*
	 * The stack already checked min_mtu <= new_mtu <= max_mtu.
	 * Program the hardware's max frame register.
	 * Frame size = MTU + Ethernet header (14) + FCS (4).
	 */
	max_frame = new_mtu + ETH_HLEN + ETH_FCS_LEN;
	iowrite32(max_frame, priv->regs + VNET_MAX_FRAME_REG);

	WRITE_ONCE(ndev->mtu, new_mtu);

	netdev_info(ndev, "MTU changed to %d (max frame %u)\n",
		    new_mtu, max_frame);
	return 0;
}

/*
 * ndo_validate_addr -- Validate the device MAC address
 *
 * Calling context: Process context, RTNL lock held.
 * Called when: Just before ndo_open. The stack calls this to ensure
 *             the MAC address in net_device->dev_addr is valid before
 *             bringing the interface up.
 * Driver must: Return 0 if the address is valid, -EADDRNOTAVAIL if not.
 *              Most Ethernet drivers use eth_validate_addr() which checks
 *              is_valid_ether_addr(dev->dev_addr).
 */
static int vnet_validate_addr(struct net_device *ndev)
{
	if (!is_valid_ether_addr(ndev->dev_addr)) {
		netdev_err(ndev, "invalid MAC address %pM\n", ndev->dev_addr);
		return -EADDRNOTAVAIL;
	}
	return 0;
}

/*
 * ndo_tx_timeout -- Handle a stuck TX queue
 *
 * Calling context: Called from the TX watchdog timer, which fires in
 *                  softirq/BH context. Must not sleep. RTNL is NOT held.
 * Called when: The watchdog detects that a TX queue has not made progress
 *             within dev->watchdog_timeo jiffies (default 5 seconds).
 * Driver must: Attempt recovery -- reset the TX queue, reset hardware,
 *              log the event. Should call netif_wake_queue() after
 *              recovery to resume transmissions.
 *
 * txqueue parameter: index of the stuck queue (0 for single-queue).
 */
static void vnet_tx_timeout(struct net_device *ndev, unsigned int txqueue)
{
	struct vnet_priv *priv = netdev_priv(ndev);

	priv->tx_timeout_count++;
	priv->stats.tx_errors++;

	netdev_warn(ndev, "TX timeout on queue %u (count: %lu)\n",
		    txqueue, priv->tx_timeout_count);

	/*
	 * In a real driver, you would:
	 *   1. Dump TX ring state for debugging
	 *   2. Reset the TX hardware/ring
	 *   3. Re-enable the TX queue
	 *
	 * For this stub, just wake the queue to let the stack retry.
	 */
	netif_wake_queue(ndev);
}

/*
 * ndo_set_rx_mode -- Set multicast/promiscuous receive filters
 *
 * Calling context: Atomic context possible (may be called under
 *                  netif_addr_lock with BH disabled). Must not sleep.
 *                  Can also be called from process context.
 * Called when: The multicast list changes (ip maddr), promiscuous mode
 *             is toggled, or all-multicast mode changes.
 * Driver must: Read ndev->flags for IFF_PROMISC and IFF_ALLMULTI,
 *              iterate ndev->mc (multicast list) to program HW filters.
 *              Must be fast -- no sleeping, no MMIO that might block.
 *
 * For this virtual device, we write a filter mode to the CTRL register.
 */
static void vnet_set_rx_mode(struct net_device *ndev)
{
	struct vnet_priv *priv = netdev_priv(ndev);
	u32 ctrl;

	ctrl = ioread32(priv->regs + VNET_CTRL);

	if (ndev->flags & IFF_PROMISC) {
		/*
		 * Promiscuous mode: accept all frames.
		 * A real driver would set a HW promisc bit here.
		 */
		netdev_dbg(ndev, "entering promiscuous mode\n");
	} else if (ndev->flags & IFF_ALLMULTI ||
		   netdev_mc_count(ndev) > 64) {
		/*
		 * All-multicast or too many addresses for HW filter:
		 * accept all multicast frames.
		 */
		netdev_dbg(ndev, "entering all-multicast mode\n");
	} else {
		/*
		 * Program individual multicast addresses into HW filter.
		 * A real driver would iterate netdev_for_each_mc_addr()
		 * and write each address to the hardware filter table.
		 */
		netdev_dbg(ndev, "setting %d multicast filters\n",
			   netdev_mc_count(ndev));
	}

	/* Write back control register (preserve existing bits) */
	iowrite32(ctrl, priv->regs + VNET_CTRL);
}

/* ---- Net Device Ops Table ---- */

static const struct net_device_ops vnet_netdev_ops = {
	.ndo_open            = vnet_open,
	.ndo_stop            = vnet_stop,
	.ndo_start_xmit      = vnet_xmit,
	.ndo_get_stats64     = vnet_get_stats64,
	.ndo_set_mac_address = vnet_set_mac_address,
	.ndo_change_mtu      = vnet_change_mtu,
	.ndo_validate_addr   = vnet_validate_addr,
	.ndo_tx_timeout      = vnet_tx_timeout,
	.ndo_set_rx_mode     = vnet_set_rx_mode,
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

	/* Set up net_device_ops and hardware limits */
	ndev->netdev_ops = &vnet_netdev_ops;
	ndev->watchdog_timeo = 5 * HZ;

	/* Set MTU limits so the stack validates before calling ndo_change_mtu */
	ndev->min_mtu = VNET_MIN_MTU;
	ndev->max_mtu = VNET_MAX_MTU;

	/* Assign a random MAC and program it into hardware */
	eth_hw_addr_random(ndev);
	vnet_write_mac_to_hw(priv->regs, ndev->dev_addr);

	/* Program initial max frame size */
	iowrite32(ndev->mtu + ETH_HLEN + ETH_FCS_LEN,
		  priv->regs + VNET_MAX_FRAME_REG);

	err = register_netdev(ndev);
	if (err)
		goto err_unmap;

	netdev_info(ndev, "net_device_ops driver loaded (MAC %pM)\n",
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

static void vnet_remove(struct pci_dev *pdev)
{
	struct net_device *ndev = pci_get_drvdata(pdev);

	unregister_netdev(ndev);
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
MODULE_DESCRIPTION("Part 3: Net Device Ops & Calling Context");
