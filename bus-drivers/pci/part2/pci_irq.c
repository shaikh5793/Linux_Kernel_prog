/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/delay.h>

#define DRV_NAME "pci_irq"


static void clear_pending_interrupts(struct pci_irq_priv *priv)
{
    u32 interrupt_val;
    
    /* Read interrupt register to see what caused the interrupt */
    interrupt_val = mmio_read32(priv, REG_INTERRUPT);
    
    /* Clear all pending interrupts by writing back the same value */
    mmio_write32(priv, REG_INTERRUPT, interrupt_val);
    
    pr_debug(DRV_NAME ": Cleared pending interrupts: 0x%08x\n", interrupt_val);
}

/*
 * Interrupt handler function
 * 
 * This is the core interrupt service routine (ISR) that gets called
 * when the device generates an interrupt. Key design principles:
 * 
 * 1. FAST EXECUTION: ISRs run in interrupt context and should be fast
 * 2. NO SLEEPING: Cannot call functions that might sleep (kmalloc, mutex_lock, etc.)
 * 3. MINIMAL WORK: Do only essential work, defer complex processing
 * 4. PROPER RETURN: Return IRQ_HANDLED if you processed the interrupt
 * 5. THREAD SAFETY: Use spinlocks for shared data access
 * 
 * IMPORTANT: This function runs in interrupt context, not process context!
 */
static irqreturn_t irq_handler(int irq, void *dev_id)
{
    struct pci_dev *pdev = dev_id;
    struct pci_irq_priv *priv = pci_get_drvdata(pdev);
    u32 interrupt_val, status_val;
    unsigned long flags;
    
    /* Check if this interrupt is for our device */
    if (!priv || !priv->bar0_mapped) {
        return IRQ_NONE;  /* Not our interrupt */
    }
    
    /* Read interrupt register to see what caused the interrupt */
    interrupt_val = mmio_read32(priv, REG_INTERRUPT);
    
    /* If no interrupt bits are set, this might not be our interrupt */
    if (!interrupt_val) {
        return IRQ_NONE;
    }
    
    /* Update interrupt counter atomically */
    spin_lock_irqsave(&priv->irq_lock, flags);
    priv->irq_count++;
    spin_unlock_irqrestore(&priv->irq_lock, flags);
    
    pr_info(DRV_NAME ": Interrupt #%lu received: 0x%08x\n", 
            priv->irq_count, interrupt_val);
    
    /* Handle different types of interrupts */
    if (interrupt_val & IRQ_DATA_READY) {
        pr_info(DRV_NAME ": Data ready interrupt\n");
        
        /* Read status to confirm data is ready */
        status_val = mmio_read32(priv, REG_STATUS);
        if (status_val & STAT_READY) {
            pr_info(DRV_NAME ": Device is ready for data transfer\n");
            /* Here you would typically read data from the device */
        }
    }
    
    if (interrupt_val & IRQ_ERROR) {
        pr_warn(DRV_NAME ": Error interrupt\n");
        
        /* Read status to get error details */
        status_val = mmio_read32(priv, REG_STATUS);
        if (status_val & STAT_ERROR) {
            pr_warn(DRV_NAME ": Device reports error condition\n");
            /* Here you would typically handle the error */
        }
    }
    
    if (interrupt_val & IRQ_TIMEOUT) {
        pr_warn(DRV_NAME ": Timeout interrupt\n");
        
        /* Read status to check device state */
        status_val = mmio_read32(priv, REG_STATUS);
        if (status_val & STAT_BUSY) {
            pr_warn(DRV_NAME ": Device is still busy\n");
            /* Here you would typically handle the timeout */
        }
    }
    
    /* Clear the pending interrupts */
    clear_pending_interrupts(priv);
    
    /* Return IRQ_HANDLED to indicate we processed the interrupt */
    return IRQ_HANDLED;
}

/*
 * Function to request interrupt
 * 
 * This function registers our interrupt handler with the kernel:
 * 1. Get the interrupt number from the PCI device
 * 2. Request the interrupt using request_irq()
 * 3. Handle cases where no interrupt is available
 * 4. Use IRQF_SHARED if multiple drivers might use the same interrupt
 * 
 * IMPORTANT: request_irq() can fail if:
 * - The interrupt is already in use by another driver
 * - The interrupt number is invalid
 * - System resources are exhausted
 */
static int request_device_interrupt(struct pci_dev *pdev, struct pci_irq_priv *priv)
{
    int ret;
    
    /* Get the interrupt number from the PCI device */
    priv->irq = pdev->irq;
    
    if (priv->irq == 0) {
        pr_info(DRV_NAME ": Device has no interrupt assigned\n");
        return 0;  /* Not an error, just no interrupt */
    }
    
    pr_info(DRV_NAME ": Requesting interrupt %d\n", priv->irq);
    
    ret = request_irq(priv->irq, irq_handler, IRQF_SHARED,
                     DRV_NAME, pdev);
    if (ret) {
        pr_err(DRV_NAME ": Failed to request interrupt %d: %d\n", 
               priv->irq, ret);
        return ret;
    }
    
    priv->irq_requested = true;
    pr_info(DRV_NAME ": Successfully requested interrupt %d\n", priv->irq);
    
    return 0;
}

/*
 * Function to free interrupt
 * 
 * This function properly cleans up the interrupt registration:
 * 1. Disable device interrupts first
 * 2. Free the kernel interrupt handler
 * 3. Clear the request flag
 * 
 * IMPORTANT: Always disable device interrupts before freeing the handler
 * to prevent spurious interrupts after cleanup.
 */
static void free_device_interrupt(struct pci_irq_priv *priv)
{
    if (priv->irq_requested) {
        /* Disable device interrupts first */
        disable_device_interrupts(priv);
        
        /* Free the interrupt handler */
        free_irq(priv->irq, priv->pdev);
        priv->irq_requested = false;
        
        pr_info(DRV_NAME ": Freed interrupt %d\n", priv->irq);
    }
}

/*
 * Function to map BAR0 if it's memory-mapped
 * 
 * Same BAR mapping pattern as previous examples.
 * This is needed to access the interrupt control registers.
 */
static int map_bar0(struct pci_dev *pdev, struct pci_irq_priv *priv)
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
 * Same BAR unmapping pattern as previous examples.
 */
static void unmap_bar0(struct pci_dev *pdev, struct pci_irq_priv *priv)
{
    if (priv->bar0_mapped) {
        pci_iounmap(pdev, priv->bar0_addr);
        pci_release_mem_regions(pdev);
        priv->bar0_mapped = false;
        priv->bar0_addr = NULL;
    }
}

/*
 * Function to simulate device activity
 * 
 * This function demonstrates how to trigger device interrupts
 * for testing purposes. In a real driver, interrupts would be generated
 * by actual device events (data ready, errors, etc.).
 * 
 * IMPORTANT: This is for demonstration only. Real devices generate
 * interrupts based on hardware events.
 */
static void simulate_device_activity(struct pci_irq_priv *priv)
{
    if (!priv->bar0_mapped) {
        pr_info(DRV_NAME ": BAR0 not mapped, cannot simulate activity\n");
        return;
    }
    
    pr_info(DRV_NAME ": Simulating device activity...\n");
    
    /* Enable the device */
    mmio_write32(priv, REG_CONTROL, CTRL_ENABLE);
    
    /* Simulate data ready condition */
    mmio_write32(priv, REG_STATUS, STAT_READY);
    mmio_write32(priv, REG_INTERRUPT, IRQ_DATA_READY);
    
    pr_info(DRV_NAME ": Device activity simulation complete\n");
}

/*
 * Probe function - Enhanced with interrupt handling
 * 
 * This probe function adds interrupt handling to the MMIO driver:
 * 1. Initialize spinlock for interrupt statistics
 * 2. Map BAR0 for register access
 * 3. Request and enable interrupts
 * 4. Simulate device activity for testing
 * 5. Enhanced error handling and cleanup
 */
static int irq_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
    struct pci_irq_priv *priv;
    int ret;

    pr_info(DRV_NAME ": Probing interrupt device %04x:%04x\n",
            pdev->vendor, pdev->device);

    /* Allocate private data structure */
    priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
    if (!priv) {
        pci_disable_device(pdev);
        return -ENOMEM;
    }

    priv->pdev = pdev;
    pci_set_drvdata(pdev, priv);

    /* Initialize spinlock for interrupt statistics */
    spin_lock_init(&priv->irq_lock);

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

    /* Request interrupt if BAR0 is mapped */
    if (priv->bar0_mapped) {
        ret = request_device_interrupt(pdev, priv);
        if (ret) {
            pr_warn(DRV_NAME ": Failed to request interrupt, continuing without interrupts\n");
        } else {
            /* Enable device interrupts */
            enable_device_interrupts(priv);
            
            /* Simulate device activity for testing */
            simulate_device_activity(priv);
        }
    }

    pr_info(DRV_NAME ": Interrupt device successfully probed\n");
    return 0;

out_disable:
    pci_disable_device(pdev);
    return ret;
}

/*
 * Remove function - Enhanced cleanup for interrupt handling
 * 
 * This remove function properly cleans up all interrupt resources:
 * 1. Free the interrupt handler
 * 2. Unmap MMIO regions
 * 3. Disable the PCI device
 * 4. Ensure no resources are leaked
 */
static void irq_remove(struct pci_dev *pdev)
{
    struct pci_irq_priv *priv = pci_get_drvdata(pdev);

    pr_info(DRV_NAME ": Removing interrupt device %04x:%04x\n",
            pdev->vendor, pdev->device);

    if (priv) {
        /* Free interrupt handler */
        free_device_interrupt(priv);
        
        /* Unmap MMIO regions */
        unmap_bar0(pdev, priv);
    }

    pci_disable_device(pdev);
}

/* PCI driver structure */
static struct pci_driver pci_irq_driver = {
    .name = DRV_NAME,
    .id_table = pci_irq_ids,
    .probe = irq_probe,
    .remove = irq_remove,
};

module_pci_driver(pci_irq_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("PCI driver with interrupt handling");
MODULE_VERSION("2.0"); 
