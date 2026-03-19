/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/io.h>
#include <linux/delay.h>

#define DRV_NAME "pci_mmio"

/* 
 * PCI Device ID table - Same pattern as basic driver
 * 
 * We use the same device matching approach as the basic driver.
 * This allows the driver to work with multiple device types for testing.
 */
static const struct pci_device_id pci_mmio_ids[] = {
    { PCI_DEVICE(0x8086, 0xa7a0) },     /* Intel Raptor Lake Graphics */
    { PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_ANY_ID) },
    { PCI_DEVICE(0x10ee, PCI_ANY_ID) },  /* Xilinx */
    { PCI_DEVICE(0x1234, PCI_ANY_ID) }, /* Test vendor */
    { 0, }
};
MODULE_DEVICE_TABLE(pci, pci_mmio_ids);

/* 
 * Private data structure
 * This structure is similar to the basic driver but focused on
 * MMIO operations. The bar0_addr field is used for register access.
 */
struct pci_mmio_priv {
    struct pci_dev *pdev;
    void __iomem *bar0_addr;  /* Virtual address of BAR0 */
    resource_size_t bar0_len; /* Length of BAR0 */
    bool bar0_mapped;         /* Whether BAR0 is mapped */
};

/* 
 * Example register offsets - These would be defined by your hardware
 * 
 * Register offsets define the memory layout of your device.
 * These are typically documented in the hardware datasheet. Each register
 * is accessed by adding its offset to the base address (BAR0).
 * 
 * TYPICAL REGISTER LAYOUT:
 * - Control register: Device enable/disable, configuration
 * - Status register: Device state, flags, error conditions
 * - Data register: Data transfer, FIFO access
 * - Configuration register: Device-specific settings
 * - Version register: Hardware/firmware version information
 * - Reset register: Device reset control
 */
#define REG_CONTROL     0x00    /* Control register */
#define REG_STATUS      0x04    /* Status register */
#define REG_DATA        0x08    /* Data register */
#define REG_CONFIG      0x0C    /* Configuration register */
#define REG_VERSION     0x10    /* Version register */
#define REG_RESET       0x14    /* Reset register */

/* 
 * Example register bit definitions
 * 
 * Register bits are typically defined as single-bit masks.
 * This makes it easy to set/clear specific bits without affecting others.
 * 
 * CONTROL REGISTER BITS:
 * - CTRL_ENABLE: Master enable bit (1=enable, 0=disable)
 * - CTRL_RESET: Reset bit (1=reset, 0=normal operation)
 * - CTRL_INTERRUPT: Interrupt enable bit (1=enable interrupts)
 * 
 * STATUS REGISTER BITS:
 * - STAT_READY: Device ready for operation
 * - STAT_BUSY: Device busy processing
 * - STAT_ERROR: Error condition detected
 */
#define CTRL_ENABLE     0x01    /* Enable bit */
#define CTRL_RESET      0x02    /* Reset bit */
#define CTRL_INTERRUPT  0x04    /* Interrupt enable bit */

#define STAT_READY      0x01    /* Ready bit */
#define STAT_BUSY       0x02    /* Busy bit */
#define STAT_ERROR      0x04    /* Error bit */

/*
 * Function to safely read a 32-bit register
 * 
 * This function demonstrates safe MMIO register access:
 * 1. Check if BAR is mapped and valid
 * 2. Validate offset is within BAR bounds
 * 3. Use ioread32() for proper memory ordering
 * 4. Add debug output for troubleshooting
 * 5. Return error value if access fails
 * 
 * IMPORTANT: Always use ioread32()/iowrite32() for MMIO access.
 * Direct pointer dereferencing can cause issues on some architectures.
 */
static u32 mmio_read32(struct pci_mmio_priv *priv, u32 offset)
{
    u32 value;
    
    /* Check if BAR0 is mapped and valid */
    if (!priv->bar0_mapped || !priv->bar0_addr) {
        pr_err(DRV_NAME ": BAR0 not mapped\n");
        return 0xFFFFFFFF;  /* Return error value */
    }
    
    /* Validate offset is within BAR bounds */
    if (offset >= priv->bar0_len) {
        pr_err(DRV_NAME ": Offset 0x%x out of range (BAR0 len: 0x%llx)\n",
               offset, (unsigned long long)priv->bar0_len);
        return 0xFFFFFFFF;  /* Return error value */
    }
    
    /* Read 32-bit value from register using proper MMIO access */
    value = ioread32(priv->bar0_addr + offset);
    
    /* Debug output for register reads */
    pr_debug(DRV_NAME ": Read 0x%08x from offset 0x%x\n", value, offset);
    
    return value;
}

/*
 * Function to safely write a 32-bit register
 * 
 * This function demonstrates safe MMIO register writing:
 * 1. Same safety checks as read function
 * 2. Use iowrite32() for proper memory ordering
 * 3. Debug output for register writes
 * 4. No return value (void function)
 * 
 * IMPORTANT: iowrite32() ensures proper memory ordering and cache coherency.
 * This is especially important on architectures with weak memory ordering.
 */
static void mmio_write32(struct pci_mmio_priv *priv, u32 offset, u32 value)
{
    /* Check if BAR0 is mapped and valid */
    if (!priv->bar0_mapped || !priv->bar0_addr) {
        pr_err(DRV_NAME ": BAR0 not mapped\n");
        return;
    }
    
    /* Validate offset is within BAR bounds */
    if (offset >= priv->bar0_len) {
        pr_err(DRV_NAME ": Offset 0x%x out of range (BAR0 len: 0x%llx)\n",
               offset, (unsigned long long)priv->bar0_len);
        return;
    }
    
    /* Write 32-bit value to register using proper MMIO access 
     * Writes are always Posted - consider using barriers for strict ordering
     */
    iowrite32(value, priv->bar0_addr + offset);
    
    /* Debug output for register writes */
    pr_debug(DRV_NAME ": Wrote 0x%08x to offset 0x%x\n", value, offset);
}

/*
 * Function to read a 16-bit register
 * 
 * Some devices have 16-bit registers. Use ioread16() for these.
 * The same safety principles apply: bounds checking and proper MMIO access.
 */
static u16 mmio_read16(struct pci_mmio_priv *priv, u32 offset)
{
    u16 value;
    
    /* Simplified bounds checking for brevity */
    if (!priv->bar0_mapped || !priv->bar0_addr || offset >= priv->bar0_len) {
        return 0xFFFF;  /* Return error value */
    }
    
    /* Read 16-bit value from register */
    value = ioread16(priv->bar0_addr + offset);
    pr_debug(DRV_NAME ": Read 0x%04x from offset 0x%x\n", value, offset);
    
    return value;
}

/*
 * Function to write a 16-bit register
 * 
 * Use iowrite16() for 16-bit register access.
 * This ensures proper byte ordering and memory barriers.
 */
static void mmio_write16(struct pci_mmio_priv *priv, u32 offset, u16 value)
{
    /* Simplified bounds checking for brevity */
    if (!priv->bar0_mapped || !priv->bar0_addr || offset >= priv->bar0_len) {
        return;
    }
    
    /* Write 16-bit value to register */
    iowrite16(value, priv->bar0_addr + offset);
    pr_debug(DRV_NAME ": Wrote 0x%04x to offset 0x%x\n", value, offset);
}

/*
 * Function to read an 8-bit register
 * 
 * Use ioread8() for 8-bit register access.
 * Common for configuration registers or status bytes.
 */
static u8 mmio_read8(struct pci_mmio_priv *priv, u32 offset)
{
    u8 value;
    
    /* Simplified bounds checking for brevity */
    if (!priv->bar0_mapped || !priv->bar0_addr || offset >= priv->bar0_len) {
        return 0xFF;  /* Return error value */
    }
    
    /* Read 8-bit value from register */
    value = ioread8(priv->bar0_addr + offset);
    pr_debug(DRV_NAME ": Read 0x%02x from offset 0x%x\n", value, offset);
    
    return value;
}

/*
 * Function to write an 8-bit register
 * 
 * Use iowrite8() for 8-bit register access.
 * Useful for control bits or configuration bytes.
 */
static void mmio_write8(struct pci_mmio_priv *priv, u32 offset, u8 value)
{
    /* Simplified bounds checking for brevity */
    if (!priv->bar0_mapped || !priv->bar0_addr || offset >= priv->bar0_len) {
        return;
    }
    
    /* Write 8-bit value to register */
    iowrite8(value, priv->bar0_addr + offset);
    pr_debug(DRV_NAME ": Wrote 0x%02x to offset 0x%x\n", value, offset);
}

/*
 * Function to dump register contents
 * 
 * This function demonstrates how to read and display all
 * important registers for debugging purposes. This is very useful
 * during driver development and troubleshooting.
 */
static void dump_registers(struct pci_mmio_priv *priv)
{
    pr_info(DRV_NAME ": Register dump:\n");
    pr_info(DRV_NAME ":   CONTROL:   0x%08x\n", mmio_read32(priv, REG_CONTROL));
    pr_info(DRV_NAME ":   STATUS:    0x%08x\n", mmio_read32(priv, REG_STATUS));
    pr_info(DRV_NAME ":   DATA:      0x%08x\n", mmio_read32(priv, REG_DATA));
    pr_info(DRV_NAME ":   CONFIG:    0x%08x\n", mmio_read32(priv, REG_CONFIG));
    pr_info(DRV_NAME ":   VERSION:   0x%08x\n", mmio_read32(priv, REG_VERSION));
    pr_info(DRV_NAME ":   RESET:     0x%08x\n", mmio_read32(priv, REG_RESET));
}

/*
 * Function to demonstrate register operations
 * 
 * This function shows practical examples of:
 * 1. Reading current register state
 * 2. Modifying specific bits while preserving others
 * 3. Checking device status
 * 4. Writing test data and reading it back
 * 5. Different data width operations (8, 16, 32-bit)
 * 
 * This is a common pattern in device driver development.
 */
static void demonstrate_reg_ops(struct pci_mmio_priv *priv)
{
    u32 control_val, status_val;
    
    pr_info(DRV_NAME ": Demonstrating register operations...\n");
    
    /* Read current control register */
    control_val = mmio_read32(priv, REG_CONTROL);
    pr_info(DRV_NAME ": Initial CONTROL register: 0x%08x\n", control_val);
    
    /* Enable the device by setting the enable bit */
    mmio_write32(priv, REG_CONTROL, control_val | CTRL_ENABLE);
    pr_info(DRV_NAME ": Enabled device\n");
    
    /* Read status to check if device is ready */
    status_val = mmio_read32(priv, REG_STATUS);
    pr_info(DRV_NAME ": STATUS register: 0x%08x\n", status_val);
    
    /* Check device state based on status bits */
    if (status_val & STAT_READY) {
        pr_info(DRV_NAME ": Device is ready\n");
    } else if (status_val & STAT_BUSY) {
        pr_info(DRV_NAME ": Device is busy\n");
    } else if (status_val & STAT_ERROR) {
        pr_info(DRV_NAME ": Device has error\n");
    }
    
    /* Write some test data to the data register */
    mmio_write32(priv, REG_DATA, 0x12345678);
    pr_info(DRV_NAME ": Wrote test data 0x12345678\n");
    
    /* Read it back to verify */
    pr_info(DRV_NAME ": Read back data: 0x%08x\n", mmio_read32(priv, REG_DATA));
    
    /* Demonstrate 16-bit operations */
    mmio_write16(priv, REG_CONFIG, 0xABCD);
    pr_info(DRV_NAME ": Wrote 16-bit config: 0x%04x\n", mmio_read16(priv, REG_CONFIG));
    
    /* Demonstrate 8-bit operations */
    mmio_write8(priv, REG_VERSION, 0x42);
    pr_info(DRV_NAME ": Wrote 8-bit version: 0x%02x\n", mmio_read8(priv, REG_VERSION));
    
    /* Disable the device by clearing the enable bit */
    mmio_write32(priv, REG_CONTROL, control_val & ~CTRL_ENABLE);
    pr_info(DRV_NAME ": Disabled device\n");
}

/*
 * Function to map BAR0 if it's memory-mapped
 * 
 * This function extends the basic BAR mapping with additional
 * checks and error handling. It specifically looks for memory-mapped BARs
 * since this driver is focused on MMIO operations.
 */
static int map_bar0(struct pci_dev *pdev, struct pci_mmio_priv *priv)
{
    resource_size_t start, len;
    unsigned long flags;
    
    /* Get BAR0 information */
    start = pci_resource_start(pdev, 0);
    len = pci_resource_len(pdev, 0);
    flags = pci_resource_flags(pdev, 0);
    
    /* Check if BAR0 is implemented and is memory-mapped */
    if (!start || !len || !(flags & IORESOURCE_MEM)) {
        pr_info(DRV_NAME ": BAR0 not available for MMIO\n");
        return 0;
    }
    
    /* Request the PCI region */
    if (pci_request_mem_regions(pdev, DRV_NAME)) {
        pr_err(DRV_NAME ": Failed to request PCI regions\n");
        return -EBUSY;
    }
    
    /* Map the BAR for MMIO access */
    priv->bar0_addr = pci_iomap(pdev, 0, len);
    if (!priv->bar0_addr) {
        pr_err(DRV_NAME ": Failed to map BAR0\n");
        pci_release_mem_regions(pdev);
        return -ENOMEM;
    }
    
    priv->bar0_len = len;
    priv->bar0_mapped = true;
    
    pr_info(DRV_NAME ": BAR0 mapped at %p, length 0x%llx\n",
            priv->bar0_addr, (unsigned long long)len);
    
    return 0;
}

/*
 * Function to unmap BAR0
 * 
 * Proper cleanup of MMIO mappings.
 * This ensures the virtual address space is freed.
 */
static void unmap_bar0(struct pci_dev *pdev, struct pci_mmio_priv *priv)
{
    if (priv->bar0_mapped) {
        pci_iounmap(pdev, priv->bar0_addr);
        pci_release_mem_regions(pdev);
        priv->bar0_mapped = false;
        priv->bar0_addr = NULL;
    }
}

/*
 * Probe function - Enhanced version with MMIO operations
 * 
 * This probe function builds on the basic driver by adding:
 * 1. MMIO-specific BAR mapping
 * 2. Register dump for debugging
 * 3. Demonstration of register operations
 * 4. Enhanced error handling
 */
static int mmio_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
    struct pci_mmio_priv *priv;
    int ret;

    pr_info(DRV_NAME ": Probing MMIO device %04x:%04x\n",
            pdev->vendor, pdev->device);

    /* Allocate private data structure */
    priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
    if (!priv) {
        pci_disable_device(pdev);
        return -ENOMEM;
    }

    priv->pdev = pdev;
    pci_set_drvdata(pdev, priv);

    /* Enable the PCI device */
    ret = pci_enable_device(pdev);
    if (ret) {
        pr_err(DRV_NAME ": Failed to enable device\n");
        return ret;
    }

    /* Map BAR0 for MMIO access */
    ret = map_bar0(pdev, priv);
    if (ret < 0) {
        pr_err(DRV_NAME ": Failed to map BAR0\n");
        goto out_disable;
    }

    /* If BAR0 is mapped, demonstrate MMIO operations */
    if (priv->bar0_mapped) {
        /* Dump initial register state */
        dump_registers(priv);
        
        /* Demonstrate register operations */
        demonstrate_reg_ops(priv);
        
        /* Dump final register state */
        dump_registers(priv);
    }

    pr_info(DRV_NAME ": MMIO device successfully probed\n");
    return 0;

out_disable:
    pci_disable_device(pdev);
    return ret;
}

/*
 * Remove function - Enhanced cleanup for MMIO operations
 * 
 * Proper cleanup includes unmapping MMIO regions
 * and ensuring all resources are freed.
 */
static void mmio_remove(struct pci_dev *pdev)
{
    struct pci_mmio_priv *priv = pci_get_drvdata(pdev);

    pr_info(DRV_NAME ": Removing MMIO device %04x:%04x\n",
            pdev->vendor, pdev->device);

    if (priv) {
        /* Unmap MMIO regions */
        unmap_bar0(pdev, priv);
    }

    pci_disable_device(pdev);
}

/* PCI driver structure */
static struct pci_driver pci_mmio_driver = {
    .name = DRV_NAME,
    .id_table = pci_mmio_ids,
    .probe = mmio_probe,
    .remove = mmio_remove,
};

module_pci_driver(pci_mmio_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("PCI driver with MMIO register access");
MODULE_VERSION("1.0"); 
