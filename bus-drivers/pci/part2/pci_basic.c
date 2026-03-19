/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/io.h>

#define DRV_NAME "pci_basic"

/* 
 * PCI Device ID table - This tells the kernel which devices this driver supports
 * 
 * The pci_device_id structure is used to match devices to drivers.
 * Each entry contains vendor ID and device ID. PCI_ANY_ID means "match any device
 * from this vendor". This table is registered with MODULE_DEVICE_TABLE.
 */
static const struct pci_device_id pci_basic_ids[] = {
    { PCI_DEVICE(0x8086, 0xa7a0) },                   /* Intel Raptor Lake Graphics */
    { PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_ANY_ID) },  /* Any Intel device */
    { PCI_DEVICE(0x10ee, PCI_ANY_ID) },               /* Any Xilinx device */
    { PCI_DEVICE(0x1234, PCI_ANY_ID) },               /* Any test vendor device */
    { 0, }  /* Terminating entry - must be all zeros */
};
MODULE_DEVICE_TABLE(pci, pci_basic_ids);

/* 
 * Private data structure - Stores driver-specific information for each device
 * 
 * This structure holds all the information needed to manage a PCI device.
 * It's allocated per device instance and stored using pci_set_drvdata().
 * This allows access to device-specific data from any function.
 */
struct pci_basic_priv {
    struct pci_dev *pdev;           /* Pointer to the PCI device structure */
    void __iomem *bar0_addr;        /* Virtual address of BAR0 (memory-mapped) */
    resource_size_t bar0_len;       /* Length of BAR0 in bytes */
    bool bar0_mapped;               /* Whether BAR0 is successfully mapped */
};

/*
 * Function to print BAR (Base Address Register) information
 * 
 * PCI devices have up to 6 BARs (BAR0-BAR5) that define the device's
 * memory and I/O address spaces. This function shows how to:
 * - Get BAR start address, length, and flags
 * - Determine if a BAR is memory-mapped or I/O-mapped
 * - Understand the resource flags (IORESOURCE_MEM vs IORESOURCE_IO)
 */
static void print_bar_info(struct pci_dev *pdev, int bar_num)
{
    resource_size_t start, len;
    unsigned long flags;

    /* Get BAR information from the PCI device */
    start = pci_resource_start(pdev, bar_num);  /* Start address of BAR */
    len = pci_resource_len(pdev, bar_num);      /* Length of BAR */
    flags = pci_resource_flags(pdev, bar_num);  /* Flags (memory/I/O, etc.) */

    pr_info(DRV_NAME ": BAR%d: start=0x%llx, len=0x%llx, flags=0x%lx\n",
            bar_num, (unsigned long long)start, (unsigned long long)len, flags);

    /* Check if this BAR is memory-mapped or I/O-mapped */
    if (flags & IORESOURCE_MEM)
        pr_info(DRV_NAME ": BAR%d is memory-mapped\n", bar_num);
    else if (flags & IORESOURCE_IO)
        pr_info(DRV_NAME ": BAR%d is I/O mapped\n", bar_num);
}

/*
 * Probe function - Called when a matching PCI device is found
 * 
 * This is the main initialization function for a PCI driver.
 * It's called by the kernel when a device matching our ID table is discovered.
 * The probe function should:
 * 1. Allocate private data structure
 * 2. Enable the PCI device
 * 3. Request PCI regions (BARs)
 * 4. Map memory regions if needed
 * 5. Initialize the device
 * 6. Set up driver data
 */
static int basic_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
    struct pci_basic_priv *priv;
    int ret;
    int i;

    /* Print device information for debugging */
    pr_info(DRV_NAME ": Probing device %04x:%04x (subsys %04x:%04x)\n",
            pdev->vendor, pdev->device, pdev->subsystem_vendor, pdev->subsystem_device);

    /* 
     * Allocate private data structure
     * 
     * devm_kzalloc() is a managed allocation that automatically
     * frees memory when the device is removed. This prevents memory leaks.
     */
    priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
    if (!priv) {
        pci_disable_device(pdev);
        return -ENOMEM;
    }

    /* Store device pointer and set driver data */
    priv->pdev = pdev;
    pci_set_drvdata(pdev, priv);

    /* Print information about all BARs (BAR0-BAR5) */
    for (i = 0; i < 6; i++) {
        print_bar_info(pdev, i);
    }

    /* 
     * Enable the PCI device
     * 
     * pci_enable_device() enables the device and makes it ready
     * for use. This must be called before accessing any device resources.
     */
    ret = pci_enable_device(pdev);
    if (ret) {
        pr_err(DRV_NAME ": Failed to enable device\n");
        return ret;
    }

    /* 
     * Request PCI regions (BARs)
     * 
     * pci_request_regions() reserves the device's BARs so that
     * no other driver can use them. This is required before mapping BARs.
     */
    ret = pci_request_regions(pdev, DRV_NAME);
    if (ret) {
        pr_err(DRV_NAME ": Failed to request regions\n");
        goto out_disable;
    }

    /* 
     * Map BAR0 for memory-mapped I/O access
     * 
     * pci_iomap() creates a virtual address mapping for a BAR.
     * This allows access to device registers using normal memory operations.
     * The mapping is automatically unmapped when the device is removed.
     */
    priv->bar0_len = pci_resource_len(pdev, 0);
    priv->bar0_addr = pci_iomap(pdev, 0, priv->bar0_len);
    if (!priv->bar0_addr) {
        pr_err(DRV_NAME ": Failed to map BAR0\n");
        ret = -ENOMEM;
        goto out_release;
    }
    priv->bar0_mapped = true;

    pr_info(DRV_NAME ": Device successfully probed and initialized\n");
    return 0;

out_release:
    pci_release_regions(pdev);
out_disable:
    pci_disable_device(pdev);
    return ret;
}

static void basic_remove(struct pci_dev *pdev)
{
    struct pci_basic_priv *priv = pci_get_drvdata(pdev);

    pr_info(DRV_NAME ": Removing device %04x:%04x\n",
            pdev->vendor, pdev->device);

    if (priv) {
        /* 
         * Unmap BAR0 if it was mapped
         * 
         * pci_iounmap() unmaps a previously mapped BAR.
         * Note: With pci_iomap(), this is usually automatic, but it's
         * good practice to be explicit about cleanup.
         */
        if (priv->bar0_mapped)
            pci_iounmap(pdev, priv->bar0_addr);
        
        /* Release PCI regions */
        pci_release_regions(pdev);
    }

    /* Disable the PCI device */
    pci_disable_device(pdev);
}

static struct pci_driver pci_basic_driver = {
    .name = DRV_NAME,                    /* Driver name */
    .id_table = pci_basic_ids,          /* Device ID table */
    .probe = basic_probe,           /* Probe function */
    .remove = basic_remove,         /* Remove function */
};

module_pci_driver(pci_basic_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Raghu Bharadwaj<raghu@techveda.org>");
MODULE_DESCRIPTION("Basic PCI driver with BARs handling");
MODULE_VERSION("2.0");
