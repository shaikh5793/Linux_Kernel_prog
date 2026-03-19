/*
 * Peripheral to Memory DMA Transfer Example
 * Author: Raghu Bharadwaj
 * Copyright (c) 2024 Techveda. Licensed under MIT.
 */

/*
 * Peripheral to memory DMA transfer demonstration
 *
 * This example simulates receiving data from a peripheral (e.g., UART)
 * into a memory buffer using the kernel's DMA subsystem.
 *
 * Key concepts:
 * - DMA channel allocation and configuration for peripheral to memory
 * - Memory mapping for DMA operations
 * - Asynchronous DMA transfer with completion callbacks
 * - Proper cleanup and error handling
 *
 * NOTE: This example uses DMA_MEMCPY to simulate a peripheral transfer,
 * since real DMA_SLAVE transfers require actual hardware peripheral
 * bindings (e.g., UART RX DMA channel via device tree). In a real
 * peripheral driver, you would use:
 *   - dma_request_chan(dev, "rx") instead of dma_request_channel()
 *   - dmaengine_slave_config() to set peripheral address and bus width
 *   - dmaengine_prep_slave_sg() instead of dmaengine_prep_dma_memcpy()
 *
 * BeagleBone Black specific:
 * - Uses EDMA (Enhanced DMA) controller
 * - Educational example for understanding peripheral DMA concepts
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

#define DMA_BUFF_SIZE 2048
#define SIMULATED_DATA "SIMULATED_PERIPHERAL_DATA: BeagleBone UART RX simulation with timestamps and incremental data packets 0123456789ABCDEF"

static struct dma_chan *dma_chan;
static dma_addr_t dma_dest_addr, dma_sim_addr;
static char *dest_buffer, *sim_source;

/*
 * DMA completion callback function
 * Called when DMA transfer completes successfully
 */
static void dma_rx_callback(void *completion)
{
    pr_info("Peripheral DMA transfer complete\n");
    complete((struct completion *)completion);
}

/*
 * Initialize and perform DMA peripheral to memory transfer
 */
static int __init peripheral_dma_rx_init(void)
{
    dma_cap_mask_t dma_mask;
    struct dma_async_tx_descriptor *tx_desc;
    dma_cookie_t dma_cookie;
    DECLARE_COMPLETION_ONSTACK(dma_completion);
    int ret = 0;

    pr_info("=== Peripheral to Memory DMA Transfer Example ===\n");
    pr_info("Initializing DMA engine RX example for BeagleBone Black\n");

    /*
     * Using DMA_MEMCPY to simulate peripheral transfer.
     * A real peripheral driver would use DMA_SLAVE and bind to a
     * specific peripheral DMA channel via device tree.
     */
    dma_cap_zero(dma_mask);
    dma_cap_set(DMA_MEMCPY, dma_mask);

    dma_chan = dma_request_channel(dma_mask, NULL, NULL);
    if (!dma_chan) {
        pr_err("Failed to request DMA channel for peripheral simulation\n");
        return -ENODEV;
    }

    pr_info("Allocated DMA channel: %s\n",
            dma_chan_name(dma_chan));

    /* Allocate destination buffer and simulation source */
    dest_buffer = kzalloc(DMA_BUFF_SIZE, GFP_KERNEL);
    sim_source = kzalloc(DMA_BUFF_SIZE, GFP_KERNEL);

    if (!dest_buffer || !sim_source) {
        pr_err("Failed to allocate buffers\n");
        ret = -ENOMEM;
        goto err_release_channel;
    }

    /* Prepare simulated peripheral data */
    strncpy(sim_source, SIMULATED_DATA, min(strlen(SIMULATED_DATA), (size_t)(DMA_BUFF_SIZE - 1)));
    memset(dest_buffer, 0xCC, DMA_BUFF_SIZE);
    dest_buffer[DMA_BUFF_SIZE - 1] = '\0';

    pr_info("BEFORE: peripheral='%.40s...' dest=[0xCC pattern]\n", sim_source);

    /* Map both buffers for DMA access */
    dma_dest_addr = dma_map_single(dma_chan->device->dev, dest_buffer,
                                   DMA_BUFF_SIZE, DMA_FROM_DEVICE);
    if (dma_mapping_error(dma_chan->device->dev, dma_dest_addr)) {
        pr_err("Failed to map destination buffer for DMA\n");
        ret = -ENOMEM;
        goto err_free_buffer;
    }

    dma_sim_addr = dma_map_single(dma_chan->device->dev, sim_source,
                                  DMA_BUFF_SIZE, DMA_TO_DEVICE);
    if (dma_mapping_error(dma_chan->device->dev, dma_sim_addr)) {
        pr_err("Failed to map simulation source buffer for DMA\n");
        ret = -ENOMEM;
        goto err_unmap_dest;
    }

    /* For educational purposes, use memcpy to simulate peripheral->memory transfer */
    pr_info("Simulating peripheral DMA transfer (UART RX -> memory)...\n");

    tx_desc = dmaengine_prep_dma_memcpy(dma_chan, dma_dest_addr, dma_sim_addr,
                                        strlen(SIMULATED_DATA), DMA_PREP_INTERRUPT);
    if (!tx_desc) {
        pr_err("Failed to prepare DMA transfer descriptor\n");
        ret = -ENOMEM;
        goto err_unmap_sim;
    }

    /* Set up completion callback */
    tx_desc->callback = dma_rx_callback;
    tx_desc->callback_param = &dma_completion;

    /* Submit the DMA transaction */
    dma_cookie = dmaengine_submit(tx_desc);
    if (dma_submit_error(dma_cookie)) {
        pr_err("Failed to submit DMA RX transaction\n");
        ret = -EIO;
        goto err_unmap_sim;
    }

    pr_info("DMA transaction submitted, cookie: %d\n", dma_cookie);

    /* Start the DMA transfer */
    dma_async_issue_pending(dma_chan);
    pr_info("Waiting for peripheral DMA completion...\n");

    /* Wait for transfer completion */
    if (!wait_for_completion_timeout(&dma_completion, msecs_to_jiffies(5000))) {
        pr_err("DMA RX transfer timeout\n");
        dmaengine_terminate_all(dma_chan);
        ret = -ETIMEDOUT;
        goto err_unmap_sim;
    }

    /* Unmap buffers to make data accessible to CPU */
    dma_unmap_single(dma_chan->device->dev, dma_dest_addr, DMA_BUFF_SIZE, DMA_FROM_DEVICE);
    dma_unmap_single(dma_chan->device->dev, dma_sim_addr, DMA_BUFF_SIZE, DMA_TO_DEVICE);

    /* Verify peripheral data transfer */
    pr_info("AFTER:  received='%.40s...'\n", dest_buffer);

    if (memcmp(sim_source, dest_buffer, strlen(SIMULATED_DATA)) == 0) {
        pr_info("[OK] Peripheral DMA SUCCESS - %zu bytes received from simulated UART\n", strlen(SIMULATED_DATA));
    } else {
        pr_err("[FAIL] Peripheral DMA FAILED - data mismatch detected\n");
    }
    /* Clean up */
    kfree(dest_buffer);
    kfree(sim_source);
    dma_release_channel(dma_chan);

    return 0;

err_unmap_sim:
    dma_unmap_single(dma_chan->device->dev, dma_sim_addr, DMA_BUFF_SIZE, DMA_TO_DEVICE);
err_unmap_dest:
    dma_unmap_single(dma_chan->device->dev, dma_dest_addr, DMA_BUFF_SIZE, DMA_FROM_DEVICE);
err_free_buffer:
    kfree(dest_buffer);
    kfree(sim_source);
err_release_channel:
    dma_release_channel(dma_chan);
    return ret;
}

/*
 * Module cleanup function
 */
static void __exit peripheral_dma_rx_exit(void)
{
    pr_info("Unloading peripheral DMA RX example module\n");
}

module_init(peripheral_dma_rx_init);
module_exit(peripheral_dma_rx_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("Peripheral-to-Memory DMA Transfer (simulated with DMA_MEMCPY)");
MODULE_VERSION("1.0");
