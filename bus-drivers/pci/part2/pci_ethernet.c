/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/if_vlan.h>
#include <linux/crc32.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/uaccess.h>

#define DRV_NAME "pci_ethernet"
#define DRV_VERSION "1.0"
#define DRV_DESCRIPTION "Simplified Realtek 8139C+ PCI Ethernet Driver"


#define PKT_BUF_SZ 1536          

// Forward declaration for pci_set_dma_mask
extern int pci_set_dma_mask(struct pci_dev *dev, u64 mask);


    HiTxRingAddr = 0x28,         
    Cmd = 0x37,                  
    ChipVersion = 0x43,         
    RxMaxSize = 0xDA,           
    RxRingAddr = 0xE4,          
};


    
    
    
    
    TxEmpty = (1 << 7),         
    RxFIFOOvr = (1 << 6),       
    TxOK = (1 << 2),            
    RxErr = (1 << 1),           
    RxError = (1 << 20),        
struct cp_desc {
    __le32 opts1;               
    __le64 rx_ok;               
    __le32 rx_err;              
    __le32 tx_ok_mcol;          
    __le64 rx_ok_phys;          
    __le16 tx_underrun;         
} __packed;


    struct cp_dma_stats *stats; 
    unsigned tx_tail;           
    
    
    
    
static inline unsigned next_tx(unsigned n)
{
    return (n + 1) & (TX_RING_SIZE - 1);
}


static inline int tx_buffs_avail(struct cp_private *cp)
{
    if (cp->tx_tail <= cp->tx_head)
        return cp->tx_tail + (TX_RING_SIZE - 1) - cp->tx_head;
    else
        return cp->tx_tail - cp->tx_head - 1;
}


static void cp_stop_hw(struct cp_private *cp)
{
    
    cp->cpcmd &= ~(CpTxOn | CpRxOn);
    cp_write16(cp, CpCmd, cp->cpcmd);
    
    
static void cp_start_hw(struct cp_private *cp)
{
    
    cp->cpcmd |= (CpTxOn | CpRxOn);
    cp_write16(cp, CpCmd, cp->cpcmd);
    
    
    cp_write32(cp, TxConfig, (TX_DMA_BURST << 8) | IFG);
    
    
    cp_write8(cp, TxThresh, TX_EARLY_THRESH >> 8);
    
    
    for (i = 0; i < TX_RING_SIZE; i++) {
        struct cp_desc *desc = &cp->tx_ring[i];
        
        desc->opts1 = 0;
        desc->opts2 = 0;
        desc->addr = 0;
    }
    
    
static netdev_tx_t cp_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
    struct cp_private *cp = netdev_priv(dev);
    unsigned entry;
    struct cp_desc *txd;
    dma_addr_t mapping;
    
    
    if (tx_buffs_avail(cp) < 1) {
        netif_stop_queue(dev);
        return NETDEV_TX_BUSY;
    }
    
    
    entry = cp->tx_head;
    txd = &cp->tx_ring[entry];
    
    
    cp_write8(cp, TxPoll, NormalTxPoll);
    
    
    if (status & TxOK) {
        
        while (cp->tx_tail != cp->tx_head) {
            struct cp_desc *txd = &cp->tx_ring[cp->tx_tail];
            
            if (txd->opts1 & DescOwn)
                break;
            
            
        if (netif_queue_stopped(dev) && tx_buffs_avail(cp) > 2)
            netif_wake_queue(dev);
    }
    
    
    if (status & TxErr) {
        dev->stats.tx_errors++;
    }
    
    return IRQ_HANDLED;
}




    dev->min_mtu = 60;
    dev->max_mtu = 4096;

    /* Store device with PCI device */
    pci_set_drvdata(pdev, dev);

    /* Register network device */
    err = register_netdev(dev);
    if (err) {
        dev_err(&pdev->dev, "Failed to register network device: %d\n", err);
        goto err_iounmap;
    }

    /* Print device information */
    netdev_info(dev, "Realtek 8139C+ at %p, %pM, IRQ %d\n",
                cp->regs, dev->dev_addr, pdev->irq);

    return 0;

err_iounmap:
    pci_iounmap(pdev, cp->regs);
err_free_dev:
    free_netdev(dev);
err_release_regions:
    pci_release_regions(pdev);
err_disable_device:
    pci_disable_device(pdev);
    return err;
}

/* Remove function - called when device is removed or driver is unloaded */
static void eth_remove(struct pci_dev *pdev)
{
    struct net_device *dev = pci_get_drvdata(pdev);
    struct cp_private *cp = netdev_priv(dev);
    
    if (dev) {
        /* Unregister network device */
        unregister_netdev(dev);
        
        /* Unmap registers */
        pci_iounmap(pdev, cp->regs);
        
        /* Free network device */
        free_netdev(dev);
    }
    
    /* Release PCI regions */
    pci_release_regions(pdev);
    
    /* Disable PCI device */
    pci_disable_device(pdev);
}

/* PCI driver structure */
static struct pci_driver pci_ethernet_driver = {
    .name = DRV_NAME,
    .id_table = cp_pci_tbl,
    .probe = eth_probe,
    .remove = eth_remove,
};

/*
 * Module initialization
 */
module_pci_driver(pci_ethernet_driver);

/*
 * Module information
 */
MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("PCI Ethernet driver example");
MODULE_VERSION(DRV_VERSION);
MODULE_LICENSE("GPL");

 
