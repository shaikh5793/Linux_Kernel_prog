/*
 * Simple DMA Memory-to-Memory Transfer Example
 * Author: Raghu Bharadwaj
 * Copyright (c) 2024 Techveda. Licensed under MIT.
 */

/*
 * Simple DMA memory-to-memory transfer demonstration
 *
 * This example demonstrates basic DMA engine usage for transferring
 * data between two memory buffers using the kernel's DMA subsystem.
 *
 * Key concepts:
 * - DMA channel allocation and configuration
 * - Memory mapping for DMA operations
 * - Asynchronous DMA transfer with completion callbacks
 * - Proper cleanup and error handling
 *
 * BeagleBone Black specific:
 * - Uses EDMA (Enhanced DMA) controller
 * - Demonstrates basic DMA_MEMCPY capability
 * - Educational example for understanding DMA fundamentals
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/completion.h>
#include <linux/string.h>

#define DMA_BUFF_SIZE 1024
#define TEST_PATTERN "Hello from DMA source buffer! This is a test pattern for memory-to-memory transfer."

static struct dma_chan *dma_chan;
static dma_addr_t dma_src_addr, dma_dest_addr;
static char *src_buffer, *dest_buffer;

/*
 * DMA completion callback function
 * Called when DMA transfer completes successfully
 */
static void dma_transfer_callback(void *completion)
{
    pr_info("DMA transfer completed successfully\n");
    complete((struct completion *)completion);
}

/*
 * Initialize and perform DMA memory-to-memory transfer
 */
static int __init simple_dma_m2m_init(void)
{
    dma_cap_mask_t dma_mask;
    struct dma_async_tx_descriptor *tx_desc = NULL;
    dma_cookie_t dma_cookie;
    DECLARE_COMPLETION_ONSTACK(dma_completion);
    int ret = 0;

    pr_info("=== Simple DMA Memory-to-Memory Transfer Example ===\n");
    pr_info("Initializing DMA engine example for BeagleBone Black\n");

    /* Set up DMA capability mask for memory copy operations */
    dma_cap_zero(dma_mask);
    dma_cap_set(DMA_MEMCPY, dma_mask);

    /* Request a DMA channel with memory copy capability */
    dma_chan = dma_request_channel(dma_mask, NULL, NULL);
    if (!dma_chan) {
        pr_err("Failed to request DMA channel for memory copy\n");
        return -ENODEV;
    }

    pr_info("Successfully allocated DMA channel: %s\n",
            dma_chan_name(dma_chan));

    /* Allocate source and destination buffers */
    src_buffer = kzalloc(DMA_BUFF_SIZE, GFP_KERNEL);
    dest_buffer = kzalloc(DMA_BUFF_SIZE, GFP_KERNEL);

    if (!src_buffer || !dest_buffer) {
        pr_err("Failed to allocate DMA buffers\n");
        ret = -ENOMEM;
        goto err_free_buffers;
    }

    /* Initialize source buffer with test pattern */
    strncpy(src_buffer, TEST_PATTERN, min(strlen(TEST_PATTERN), (size_t)(DMA_BUFF_SIZE - 1)));
    memset(dest_buffer, 0xAA, DMA_BUFF_SIZE);
    dest_buffer[DMA_BUFF_SIZE - 1] = '\0';

    pr_info("BEFORE: src='%.40s...' dest=[0xAA pattern]\n", src_buffer);

    /* Map buffers for DMA access */
    dma_src_addr = dma_map_single(dma_chan->device->dev, src_buffer,
                                  DMA_BUFF_SIZE, DMA_TO_DEVICE);
    if (dma_mapping_error(dma_chan->device->dev, dma_src_addr)) {
        pr_err("Failed to map source buffer for DMA\n");
        ret = -ENOMEM;
        goto err_free_buffers;
    }

    dma_dest_addr = dma_map_single(dma_chan->device->dev, dest_buffer,
                                   DMA_BUFF_SIZE, DMA_FROM_DEVICE);
    if (dma_mapping_error(dma_chan->device->dev, dma_dest_addr)) {
        pr_err("Failed to map destination buffer for DMA\n");
        ret = -ENOMEM;
        goto err_unmap_src;
    }

    /* Prepare DMA memory copy descriptor */
    tx_desc = dmaengine_prep_dma_memcpy(dma_chan, dma_dest_addr, dma_src_addr,
                                        DMA_BUFF_SIZE, DMA_PREP_INTERRUPT);
    if (!tx_desc) {
        pr_err("Failed to prepare DMA memory copy descriptor\n");
        ret = -ENOMEM;
        goto err_unmap_dest;
    }

    /* Set up completion callback */
    tx_desc->callback = dma_transfer_callback;
    tx_desc->callback_param = &dma_completion;

    /* Submit the DMA transaction */
    dma_cookie = dmaengine_submit(tx_desc);
    if (dma_submit_error(dma_cookie)) {
        pr_err("Failed to submit DMA transaction\n");
        ret = -EIO;
        goto err_unmap_dest;
    }

    pr_info("DMA transaction submitted, cookie: %d\n", dma_cookie);

    /* Start the DMA transfer */
    dma_async_issue_pending(dma_chan);
    pr_info("DMA transfer started, waiting for completion...\n");

    /* Wait for transfer completion */
    if (!wait_for_completion_timeout(&dma_completion, msecs_to_jiffies(5000))) {
        pr_err("DMA transfer timeout\n");
        dmaengine_terminate_all(dma_chan);
        ret = -ETIMEDOUT;
        goto err_unmap_dest;
    }

    /* Unmap buffers to make data accessible to CPU */
    dma_unmap_single(dma_chan->device->dev, dma_src_addr, DMA_BUFF_SIZE, DMA_TO_DEVICE);
    dma_unmap_single(dma_chan->device->dev, dma_dest_addr, DMA_BUFF_SIZE, DMA_FROM_DEVICE);

    /* Verify transfer results */
    pr_info("AFTER:  dest='%.40s...'\n", dest_buffer);

    if (memcmp(src_buffer, dest_buffer, strlen(TEST_PATTERN)) == 0) {
        pr_info("[OK] DMA transfer SUCCESS - %zu bytes transferred correctly\n", strlen(TEST_PATTERN));
    } else {
        pr_err("[FAIL] DMA transfer FAILED - data mismatch detected\n");
    }

    /* Clean up */
    kfree(src_buffer);
    kfree(dest_buffer);
    dma_release_channel(dma_chan);

    pr_info("Simple DMA M2M example completed successfully\n");
    return 0;

err_unmap_dest:
    dma_unmap_single(dma_chan->device->dev, dma_dest_addr, DMA_BUFF_SIZE, DMA_FROM_DEVICE);
err_unmap_src:
    dma_unmap_single(dma_chan->device->dev, dma_src_addr, DMA_BUFF_SIZE, DMA_TO_DEVICE);
err_free_buffers:
    kfree(src_buffer);
    kfree(dest_buffer);
    if (dma_chan)
        dma_release_channel(dma_chan);
    return ret;
}

/*
 * Module cleanup function
 */
static void __exit simple_dma_m2m_exit(void)
{
    pr_info("Unloading simple DMA M2M example module\n");
}

module_init(simple_dma_m2m_init);
module_exit(simple_dma_m2m_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("Simple DMA Memory-to-Memory Transfer Example for BeagleBone Black");
MODULE_VERSION("1.0");
