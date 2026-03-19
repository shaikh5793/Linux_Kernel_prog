/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/delay.h>

/* 
 * Note: DMA mask functions are provided by the kernel's PCI subsystem.
 * No forward declarations needed - they're properly declared in pci.h.
 */

#define DRV_NAME "pci_dma"

/* 
 * PCI Device ID table - Same pattern as previous drivers
 * 
 * We use the same device matching approach for consistency.
 * This allows testing with various PCI devices.
 */
static const struct pci_device_id pci_dma_ids[] = {
    { PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_ANY_ID) },
    { PCI_DEVICE(0x10ee, PCI_ANY_ID) },  /* Xilinx */
    { PCI_DEVICE(0x1234, PCI_ANY_ID) }, /* Test vendor */
    { 0, }
};
MODULE_DEVICE_TABLE(pci, pci_dma_ids);

/* 
 * DMA buffer sizes for demonstration
 * 
 * These sizes are chosen for demonstration purposes.
 * Real DMA buffers should be sized based on:
 * - Device requirements and capabilities
 * - Performance considerations
 * - Memory constraints
 * - Transfer patterns (burst vs streaming)
 */
#define DMA_BUFFER_SIZE     4096    /* 4KB buffer */
#define DMA_BUFFER_COUNT    4       /* Number of buffers */

/* 
 * Private data structure - Extended with DMA capabilities
 * 
 * This structure manages both coherent and streaming DMA buffers:
 * - Coherent buffers: Cache-coherent, both CPU and device see same data
 * - Streaming buffers: Require explicit cache operations, more efficient
 * - DMA addresses: Physical addresses that the device can access
 * - Mapping flags: Track which buffers are currently mapped for DMA
 */
struct pci_dma_priv {
    struct pci_dev *pdev;
    void __iomem *bar0_addr;  /* Virtual address of BAR0 */
    resource_size_t bar0_len; /* Length of BAR0 */
    bool bar0_mapped;         /* Whether BAR0 is mapped */
    
    /* DMA coherent buffers */
    void *coherent_buf;       /* Coherent buffer virtual address */
    dma_addr_t coherent_dma;  /* Coherent buffer DMA address */
    size_t coherent_size;     /* Coherent buffer size */
    
    /* DMA streaming buffers */
    void *streaming_bufs[DMA_BUFFER_COUNT];     /* Streaming buffer virtual addresses */
    dma_addr_t streaming_dma[DMA_BUFFER_COUNT]; /* Streaming buffer DMA addresses */
    size_t streaming_size;    /* Size of each streaming buffer */
    bool streaming_mapped[DMA_BUFFER_COUNT];    /* Whether buffers are mapped */
};

/* 
 * Example register offsets for DMA operations
 * 
 * These registers control DMA transfers on the device:
 * - DMA_CONTROL: Start/stop transfers, direction, interrupt enable
 * - DMA_ADDRESS: Physical address for DMA transfer
 * - DMA_LENGTH: Number of bytes to transfer
 * - DMA_STATUS: Transfer status (busy, done, error)
 */
#define REG_DMA_CONTROL     0x00    /* DMA control register */
#define REG_DMA_ADDRESS     0x04    /* DMA address register */
#define REG_DMA_LENGTH      0x08    /* DMA length register */
#define REG_DMA_STATUS      0x0C    /* DMA status register */

/* DMA control register bits */
#define DMA_CTRL_START      0x01    /* Start DMA transfer */
#define DMA_CTRL_DIRECTION  0x02    /* 0=to device, 1=from device */
#define DMA_CTRL_INTERRUPT  0x04    /* Enable DMA interrupt */

/* DMA status register bits */
#define DMA_STAT_BUSY       0x01    /* DMA transfer in progress */
#define DMA_STAT_DONE       0x02    /* DMA transfer completed */
#define DMA_STAT_ERROR      0x04    /* DMA transfer error */

/*
 * Function to safely read a 32-bit register
 * 
 * Same MMIO access pattern as previous examples.
 * This function is used to read DMA control and status registers.
 */
static u32 mmio_read32(struct pci_dma_priv *priv, u32 offset)
{
    if (!priv->bar0_mapped || !priv->bar0_addr || offset >= priv->bar0_len) {
        return 0xFFFFFFFF;
    }
    return ioread32(priv->bar0_addr + offset);
}

/*
 * Function to safely write a 32-bit register
 * 
 * Same MMIO access pattern as previous examples.
 * This function is used to write DMA control registers.
 */
static void mmio_write32(struct pci_dma_priv *priv, u32 offset, u32 value)
{
    if (!priv->bar0_mapped || !priv->bar0_addr || offset >= priv->bar0_len) {
        return;
    }
    iowrite32(value, priv->bar0_addr + offset);
}

/*
 * Function to allocate DMA coherent buffer
 * 
 * Coherent buffers are cache-coherent, meaning the CPU and device
 * see the same data without explicit cache operations. This is convenient
 * but can be less efficient than streaming buffers.
 * 
 * COHERENT BUFFER CHARACTERISTICS:
 * - Cache-coherent: No explicit cache operations needed
 * - Slower access: May bypass CPU cache
 * - Higher memory usage: May require cache-line alignment
 * - Simpler programming: No cache management required
 * 
 * USE CASES:
 * - Small, frequently accessed buffers
 * - Buffers shared between CPU and device
 * - When cache management is complex
 */
static int allocate_coherent_buffer(struct pci_dev *pdev, struct pci_dma_priv *priv)
{
    pr_info(DRV_NAME ": Allocating coherent DMA buffer...\n");
    
    /* 
     * Allocate coherent DMA buffer
     * 
     * dma_alloc_coherent() allocates cache-coherent memory.
     * Parameters:
     * - dev: Device pointer (for DMA operations)
     * - size: Buffer size in bytes
     * - dma_handle: Returns the DMA address
     * - gfp: Memory allocation flags
     */
    priv->coherent_buf = dma_alloc_coherent(&pdev->dev, DMA_BUFFER_SIZE,
                                           &priv->coherent_dma, GFP_KERNEL);
    if (!priv->coherent_buf) {
        pr_err(DRV_NAME ": Failed to allocate coherent DMA buffer\n");
        return -ENOMEM;
    }
    
    priv->coherent_size = DMA_BUFFER_SIZE;
    
    pr_info(DRV_NAME ": Coherent buffer allocated:\n");
    pr_info(DRV_NAME ":   Virtual address: %p\n", priv->coherent_buf);
    pr_info(DRV_NAME ":   DMA address: 0x%llx\n", (unsigned long long)priv->coherent_dma);
    pr_info(DRV_NAME ":   Size: %zu bytes\n", priv->coherent_size);
    
    /* Initialize the buffer with test data */
    memset(priv->coherent_buf, 0xAA, priv->coherent_size);
    pr_info(DRV_NAME ": Initialized coherent buffer with 0xAA pattern\n");
    
    return 0;
}

/*
 * Function to free DMA coherent buffer
 * 
 * Proper cleanup of coherent buffers using dma_free_coherent().
 * This function must be called with the same parameters used for allocation.
 */
static void free_coherent_buffer(struct pci_dev *pdev, struct pci_dma_priv *priv)
{
    if (priv->coherent_buf) {
        pr_info(DRV_NAME ": Freeing coherent DMA buffer\n");
        dma_free_coherent(&pdev->dev, priv->coherent_size,
                         priv->coherent_buf, priv->coherent_dma);
        priv->coherent_buf = NULL;
        priv->coherent_dma = 0;
        priv->coherent_size = 0;
    }
}

/*
 * Function to allocate DMA streaming buffers
 * 
 * Streaming buffers require explicit cache operations (dma_sync_*)
 * but are more efficient for large transfers. They use normal kernel memory
 * that is temporarily mapped for DMA access.
 * 
 * STREAMING BUFFER CHARACTERISTICS:
 * - Normal memory: Uses CPU cache for better performance
 * - Explicit cache ops: Must call dma_sync_* functions
 * - More efficient: Better cache utilization
 * - More complex: Requires careful cache management
 * 
 * USE CASES:
 * - Large data transfers
 * - Performance-critical applications
 * - When you can manage cache operations
 */
static int allocate_streaming_buffers(struct pci_dev *pdev, struct pci_dma_priv *priv)
{
    int i;
    
    pr_info(DRV_NAME ": Allocating streaming DMA buffers...\n");
    
    priv->streaming_size = DMA_BUFFER_SIZE;
    
    for (i = 0; i < DMA_BUFFER_COUNT; i++) {
        /* 
         * Allocate streaming buffer using normal kernel memory
         * 
         * We use kmalloc() for streaming buffers, not dma_alloc_coherent().
         * This allows the memory to use CPU cache for better performance.
         */
        priv->streaming_bufs[i] = kmalloc(priv->streaming_size, GFP_KERNEL);
        if (!priv->streaming_bufs[i]) {
            pr_err(DRV_NAME ": Failed to allocate streaming buffer %d\n", i);
            goto cleanup;
        }
        
        /* 
         * Map for DMA access
         * 
         * dma_map_single() creates a DMA mapping for normal memory.
         * The direction parameter specifies data flow:
         * - DMA_TO_DEVICE: CPU to device
         * - DMA_FROM_DEVICE: Device to CPU
         * - DMA_BIDIRECTIONAL: Both directions
         */
        priv->streaming_dma[i] = dma_map_single(&pdev->dev, priv->streaming_bufs[i],
                                               priv->streaming_size, DMA_BIDIRECTIONAL);
        if (dma_mapping_error(&pdev->dev, priv->streaming_dma[i])) {
            pr_err(DRV_NAME ": Failed to map streaming buffer %d for DMA\n", i);
            kfree(priv->streaming_bufs[i]);
            priv->streaming_bufs[i] = NULL;
            goto cleanup;
        }
        
        priv->streaming_mapped[i] = true;
        
        /* Initialize with test data */
        memset(priv->streaming_bufs[i], 0x55 + i, priv->streaming_size);
        
        pr_info(DRV_NAME ": Streaming buffer %d allocated:\n", i);
        pr_info(DRV_NAME ":   Virtual address: %p\n", priv->streaming_bufs[i]);
        pr_info(DRV_NAME ":   DMA address: 0x%llx\n", (unsigned long long)priv->streaming_dma[i]);
        pr_info(DRV_NAME ":   Size: %zu bytes\n", priv->streaming_size);
    }
    
    pr_info(DRV_NAME ": All streaming buffers allocated successfully\n");
    return 0;
    
cleanup:
    /* Clean up any buffers that were successfully allocated */
    for (i = 0; i < DMA_BUFFER_COUNT; i++) {
        if (priv->streaming_mapped[i]) {
            dma_unmap_single(&pdev->dev, priv->streaming_dma[i],
                            priv->streaming_size, DMA_BIDIRECTIONAL);
            priv->streaming_mapped[i] = false;
        }
        if (priv->streaming_bufs[i]) {
            kfree(priv->streaming_bufs[i]);
            priv->streaming_bufs[i] = NULL;
        }
    }
    return -ENOMEM;
}

/*
 * Function to free DMA streaming buffers
 * 
 * Proper cleanup of streaming buffers requires:
 * 1. Unmapping the DMA mapping
 * 2. Freeing the kernel memory
 * 3. Clearing the mapping flags
 */
static void free_streaming_buffers(struct pci_dev *pdev, struct pci_dma_priv *priv)
{
    int i;
    
    pr_info(DRV_NAME ": Freeing streaming DMA buffers...\n");
    
    for (i = 0; i < DMA_BUFFER_COUNT; i++) {
        if (priv->streaming_mapped[i]) {
            /* 
             * Unmap DMA mapping
             * 
             * dma_unmap_single() removes the DMA mapping.
             * Must use the same parameters as dma_map_single().
             */
            dma_unmap_single(&pdev->dev, priv->streaming_dma[i],
                            priv->streaming_size, DMA_BIDIRECTIONAL);
            priv->streaming_mapped[i] = false;
        }
        if (priv->streaming_bufs[i]) {
            kfree(priv->streaming_bufs[i]);
            priv->streaming_bufs[i] = NULL;
        }
    }
}

/*
 * Function to demonstrate DMA operations
 * 
 * This function shows practical examples of:
 * 1. Setting up DMA transfers using device registers
 * 2. Monitoring DMA transfer status
 * 3. Using both coherent and streaming buffers
 * 4. DMA transfer control and completion detection
 * 
 * This demonstrates the typical DMA programming pattern.
 */
static void demonstrate_dma_ops(struct pci_dma_priv *priv)
{
    u32 control_val, status_val;
    int i;
    
    if (!priv->bar0_mapped) {
        pr_info(DRV_NAME ": BAR0 not mapped, skipping DMA operations\n");
        return;
    }
    
    pr_info(DRV_NAME ": Demonstrating DMA operations...\n");
    
    /* Read current DMA control register */
    control_val = mmio_read32(priv, REG_DMA_CONTROL);
    pr_info(DRV_NAME ": Initial DMA control: 0x%08x\n", control_val);
    
    /* Set up DMA transfer to device using coherent buffer */
    if (priv->coherent_buf) {
        pr_info(DRV_NAME ": Setting up DMA transfer to device...\n");
        
        /* Set DMA address */
        mmio_write32(priv, REG_DMA_ADDRESS, (u32)priv->coherent_dma);
        pr_info(DRV_NAME ": Set DMA address: 0x%08x\n", (u32)priv->coherent_dma);
        
        /* Set DMA length */
        mmio_write32(priv, REG_DMA_LENGTH, priv->coherent_size);
        pr_info(DRV_NAME ": Set DMA length: %zu bytes\n", priv->coherent_size);
        
        /* Start DMA transfer (to device) */
        mmio_write32(priv, REG_DMA_CONTROL, 
                    control_val | DMA_CTRL_START | DMA_CTRL_INTERRUPT);
        pr_info(DRV_NAME ": Started DMA transfer to device\n");
        
        /* Wait for transfer to complete */
        do {
            status_val = mmio_read32(priv, REG_DMA_STATUS);
            if (status_val & DMA_STAT_ERROR) {
                pr_err(DRV_NAME ": DMA transfer error\n");
                break;
            }
        } while (status_val & DMA_STAT_BUSY);
        
        if (status_val & DMA_STAT_DONE) {
            pr_info(DRV_NAME ": DMA transfer to device completed\n");
        }
    }
    
    /* Demonstrate streaming buffer operations */
    for (i = 0; i < DMA_BUFFER_COUNT; i++) {
        if (!priv->streaming_mapped[i]) {
            continue;
        }
        
        pr_info(DRV_NAME ": Demonstrating streaming buffer %d...\n", i);
        
        /* 
         * Sync buffer for CPU access
         * 
         * dma_sync_single_for_cpu() ensures the CPU sees
         * the latest data from the device. This is needed for streaming
         * buffers when the device may have modified the data.
         */
        dma_sync_single_for_cpu(&priv->pdev->dev, priv->streaming_dma[i],
                               priv->streaming_size, DMA_FROM_DEVICE);
        
        /* Read some data from the buffer */
        pr_info(DRV_NAME ": Buffer %d first byte: 0x%02x\n", 
                i, *((u8*)priv->streaming_bufs[i]));
        
        /* 
         * Sync buffer for device access
         * 
         * dma_sync_single_for_device() ensures the device sees
         * the latest data from the CPU. This is needed before starting
         * a DMA transfer from CPU to device.
         */
        dma_sync_single_for_device(&priv->pdev->dev, priv->streaming_dma[i],
                                  priv->streaming_size, DMA_TO_DEVICE);
        
        /* Set up DMA transfer from device using streaming buffer */
        mmio_write32(priv, REG_DMA_ADDRESS, (u32)priv->streaming_dma[i]);
        mmio_write32(priv, REG_DMA_LENGTH, priv->streaming_size);
        
        /* Start DMA transfer (from device) */
        mmio_write32(priv, REG_DMA_CONTROL, 
                    control_val | DMA_CTRL_START | DMA_CTRL_DIRECTION | DMA_CTRL_INTERRUPT);
        pr_info(DRV_NAME ": Started DMA transfer from device to buffer %d\n", i);
        
        /* Wait for transfer to complete */
        do {
            status_val = mmio_read32(priv, REG_DMA_STATUS);
            if (status_val & DMA_STAT_ERROR) {
                pr_err(DRV_NAME ": DMA transfer error on buffer %d\n", i);
                break;
            }
        } while (status_val & DMA_STAT_BUSY);
        
        if (status_val & DMA_STAT_DONE) {
            pr_info(DRV_NAME ": DMA transfer from device to buffer %d completed\n", i);
            
            /* Sync buffer for CPU access to read the received data */
            dma_sync_single_for_cpu(&priv->pdev->dev, priv->streaming_dma[i],
                                   priv->streaming_size, DMA_FROM_DEVICE);
            
            pr_info(DRV_NAME ": Buffer %d first byte after transfer: 0x%02x\n", 
                    i, *((u8*)priv->streaming_bufs[i]));
        }
    }
    
    /* Stop DMA controller */
    mmio_write32(priv, REG_DMA_CONTROL, control_val & ~DMA_CTRL_START);
    pr_info(DRV_NAME ": DMA controller stopped\n");
}

/*
 * Function to map BAR0 if it's memory-mapped
 * 
 * Same BAR mapping pattern as previous examples.
 * This is needed to access the DMA control registers.
 */
static int map_bar0(struct pci_dev *pdev, struct pci_dma_priv *priv)
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
static void unmap_bar0(struct pci_dev *pdev, struct pci_dma_priv *priv)
{
    if (priv->bar0_mapped) {
        pci_iounmap(pdev, priv->bar0_addr);
        pci_release_mem_regions(pdev);
        priv->bar0_mapped = false;
        priv->bar0_addr = NULL;
    }
}

/*
 * Probe function - Enhanced with DMA capabilities
 * 
 * This probe function adds DMA functionality to the driver:
 * 1. Configure DMA masks for 32-bit and 64-bit systems
 * 2. Allocate both coherent and streaming DMA buffers
 * 3. Map BAR0 for DMA control register access
 * 4. Demonstrate DMA operations
 * 5. Enhanced error handling and cleanup
 */
static int dma_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
    struct pci_dma_priv *priv;
    int ret;

    pr_info(DRV_NAME ": Probing DMA device %04x:%04x\n",
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

    /* 
     * Configure DMA masks
     * 
     * DMA masks determine the address range the device can access.
     * - Try 64-bit first for better performance
     * - Fall back to 32-bit if 64-bit is not supported
     * - Both regular and coherent masks must be set
     */
    ret = dma_set_mask(&pdev->dev, DMA_BIT_MASK(64));
    if (ret) {
        ret = dma_set_mask(&pdev->dev, DMA_BIT_MASK(32));
        if (ret) {
            pr_err(DRV_NAME ": Failed to set DMA mask: %d\n", ret);
            goto out_disable;
        }
        pr_info(DRV_NAME ": Using 32-bit DMA addressing\n");
    } else {
        pr_info(DRV_NAME ": Using 64-bit DMA addressing\n");
    }
    
    ret = dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(64));
    if (ret) {
        ret = dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(32));
        if (ret) {
            pr_err(DRV_NAME ": Failed to set consistent DMA mask: %d\n", ret);
            goto out_disable;
        }
    }

    /* Enable bus mastering for DMA */
    pci_set_master(pdev);

    /* Map BAR0 for MMIO access */
    ret = map_bar0(pdev, priv);
    if (ret < 0) {
        pr_err(DRV_NAME ": Failed to map BAR0\n");
        goto out_disable;
    }

    /* Allocate DMA buffers */
    ret = allocate_coherent_buffer(pdev, priv);
    if (ret) {
        pr_err(DRV_NAME ": Failed to allocate coherent buffer\n");
        goto out_unmap;
    }

    ret = allocate_streaming_buffers(pdev, priv);
    if (ret) {
        pr_err(DRV_NAME ": Failed to allocate streaming buffers\n");
        goto out_free_coherent;
    }

    /* Demonstrate DMA operations if BAR0 is mapped */
    if (priv->bar0_mapped) {
        demonstrate_dma_ops(priv);
    }

    pr_info(DRV_NAME ": DMA device successfully probed\n");
    return 0;

out_free_coherent:
    free_coherent_buffer(pdev, priv);
out_unmap:
    unmap_bar0(pdev, priv);
out_disable:
    pci_disable_device(pdev);
    return ret;
}

/*
 * Remove function - Enhanced cleanup for DMA operations
 * 
 * This remove function properly cleans up all DMA resources:
 * 1. Free streaming DMA buffers
 * 2. Free coherent DMA buffers
 * 3. Unmap MMIO regions
 * 4. Disable the PCI device
 * 5. Ensure no DMA resources are leaked
 */
static void dma_remove(struct pci_dev *pdev)
{
    struct pci_dma_priv *priv = pci_get_drvdata(pdev);

    pr_info(DRV_NAME ": Removing DMA device %04x:%04x\n",
            pdev->vendor, pdev->device);

    if (priv) {
        /* Free DMA buffers */
        free_streaming_buffers(pdev, priv);
        free_coherent_buffer(pdev, priv);
        
        /* Unmap MMIO regions */
        unmap_bar0(pdev, priv);
    }

    pci_disable_device(pdev);
}

/* PCI driver structure */
static struct pci_driver pci_dma_driver = {
    .name = DRV_NAME,
    .id_table = pci_dma_ids,
    .probe = dma_probe,
    .remove = dma_remove,
};

module_pci_driver(pci_dma_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("PCI driver with DMA operations");
MODULE_VERSION("1.0"); 
