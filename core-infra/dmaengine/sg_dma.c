/*
 * Scatter-Gather DMA Example
 * Author: Raghu Bharadwaj
 * Copyright (c) 2024 Techveda. Licensed under MIT.
 */

/*
 * Scatter-Gather DMA demonstration
 *
 * This example demonstrates DMA transfers using scatter-gather lists,
 * which allow transferring data from/to non-contiguous memory locations
 * in a single DMA operation.
 *
 * Key concepts:
 * - Scatter-gather list creation and management
 * - Non-contiguous memory handling
 * - Descriptor chains for complex transfers
 * - Verification of fragmented data transfers
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/completion.h>
#include <linux/scatterlist.h>
#include <linux/string.h>

#define NUM_SEGS 4
#define DMA_SEG_SIZE 256
#define DMA_TOTAL_SIZE (NUM_SEGS * DMA_SEG_SIZE)
#define TEST_PATTERN_BASE "SG_SEGMENT_%d_DATA: This is scatter-gather test data for segment %d with unique pattern 0x%02X "

struct sg_dma_data {
    struct scatterlist src_sg[NUM_SEGS];
    struct scatterlist dst_sg[NUM_SEGS];
    struct dma_chan *chan;
    struct completion completion;
    void *src_segments[NUM_SEGS];
    void *dst_segments[NUM_SEGS];
};

static void sg_dma_callback(void *completion)
{
    pr_info("[OK] Scatter-Gather DMA transfer completed successfully\n");
    complete(completion);
}

static int __init sg_dma_init(void)
{
    struct sg_dma_data sg_data;
    dma_cap_mask_t mask;
    struct dma_async_tx_descriptor *tx = NULL;
    dma_cookie_t cookie;
    int i, result = 0;
    char pattern_buffer[DMA_SEG_SIZE];
    int total_mismatches = 0;

    pr_info("=== Scatter-Gather DMA Transfer Example ===\n");
    pr_info("Initializing scatter-gather DMA with %d segments of %d bytes each\n", NUM_SEGS, DMA_SEG_SIZE);

    memset(&sg_data, 0, sizeof(sg_data));

    /* Set up DMA capability mask for memory copy operations */
    dma_cap_zero(mask);
    dma_cap_set(DMA_MEMCPY, mask);

    sg_data.chan = dma_request_channel(mask, NULL, NULL);
    if (!sg_data.chan) {
        pr_err("Failed to request DMA channel\n");
        return -ENODEV;
    }

    pr_info("Successfully allocated DMA channel: %s\n", dma_chan_name(sg_data.chan));

    /* Allocate non-contiguous memory segments (simulating fragmented memory) */
    for (i = 0; i < NUM_SEGS; i++) {
        sg_data.src_segments[i] = kzalloc(DMA_SEG_SIZE, GFP_KERNEL);
        sg_data.dst_segments[i] = kzalloc(DMA_SEG_SIZE, GFP_KERNEL);

        if (!sg_data.src_segments[i] || !sg_data.dst_segments[i]) {
            pr_err("Failed to allocate segment %d\n", i);
            result = -ENOMEM;
            goto free_segments;
        }

        /* Initialize each source segment with unique pattern */
        snprintf(pattern_buffer, sizeof(pattern_buffer), TEST_PATTERN_BASE, i, i, (0xA0 + i));
        strncpy((char *)sg_data.src_segments[i], pattern_buffer, DMA_SEG_SIZE - 1);

        /* Fill destination with different pattern to show transfer */
        memset(sg_data.dst_segments[i], (0xF0 + i), DMA_SEG_SIZE);
    }

    /* Initialize scatter-gather lists */
    sg_init_table(sg_data.src_sg, NUM_SEGS);
    sg_init_table(sg_data.dst_sg, NUM_SEGS);

    for (i = 0; i < NUM_SEGS; i++) {
        sg_set_buf(&sg_data.src_sg[i], sg_data.src_segments[i], DMA_SEG_SIZE);
        sg_set_buf(&sg_data.dst_sg[i], sg_data.dst_segments[i], DMA_SEG_SIZE);
    }

    /* Map scatter-gather lists for DMA */
    if (dma_map_sg(sg_data.chan->device->dev, sg_data.src_sg, NUM_SEGS, DMA_TO_DEVICE) != NUM_SEGS) {
        pr_err("Failed to map source scatter-gather list\n");
        result = -ENOMEM;
        goto free_segments;
    }

    if (dma_map_sg(sg_data.chan->device->dev, sg_data.dst_sg, NUM_SEGS, DMA_FROM_DEVICE) != NUM_SEGS) {
        pr_err("Failed to map destination scatter-gather list\n");
        result = -ENOMEM;
        goto unmap_src_sg;
    }

    init_completion(&sg_data.completion);

    /* Prepare scatter-gather DMA transfers */
    pr_info("Transferring %d scatter-gather segments...\n", NUM_SEGS);

    /* For demonstration, we'll do segment-by-segment transfers */
    for (i = 0; i < NUM_SEGS; i++) {
        dma_addr_t src_addr = sg_dma_address(&sg_data.src_sg[i]);
        dma_addr_t dst_addr = sg_dma_address(&sg_data.dst_sg[i]);

        tx = dmaengine_prep_dma_memcpy(sg_data.chan, dst_addr, src_addr,
                                       DMA_SEG_SIZE, DMA_PREP_INTERRUPT);
        if (!tx) {
            pr_err("Failed to prepare DMA transfer for segment %d\n", i);
            result = -EIO;
            goto unmap_dst_sg;
        }

        if (i == NUM_SEGS - 1) {  /* Last segment */
            tx->callback = sg_dma_callback;
            tx->callback_param = &sg_data.completion;
        }

        cookie = dmaengine_submit(tx);
        dma_async_issue_pending(sg_data.chan);
    }

    pr_info("Waiting for scatter-gather DMA completion...\n");

    if (!wait_for_completion_timeout(&sg_data.completion, msecs_to_jiffies(10000))) {
        pr_err("Scatter-gather DMA transfer timeout\n");
        dmaengine_terminate_all(sg_data.chan);
        result = -ETIMEDOUT;
        goto unmap_dst_sg;
    }

    /* Unmap scatter-gather lists */
    dma_unmap_sg(sg_data.chan->device->dev, sg_data.src_sg, NUM_SEGS, DMA_TO_DEVICE);
    dma_unmap_sg(sg_data.chan->device->dev, sg_data.dst_sg, NUM_SEGS, DMA_FROM_DEVICE);

    /* Verify scatter-gather transfer */
    for (i = 0; i < NUM_SEGS; i++) {
        size_t pattern_len = strnlen((char *)sg_data.src_segments[i], DMA_SEG_SIZE);
        if (memcmp(sg_data.src_segments[i], sg_data.dst_segments[i], pattern_len) != 0) {
            total_mismatches++;
            pr_err("Segment %d: transfer FAILED\n", i);
        } else {
            pr_info("Segment %d: '%.30s...' -> SUCCESS\n", i, (char *)sg_data.src_segments[i]);
        }
    }

    if (total_mismatches == 0) {
        pr_info("[OK] Scatter-Gather DMA SUCCESS - all %d segments transferred correctly\n", NUM_SEGS);
    } else {
        pr_err("[FAIL] Scatter-Gather DMA FAILED - %d segments had errors\n", total_mismatches);
    }

    /* Clean up segments */
    for (i = 0; i < NUM_SEGS; i++) {
        kfree(sg_data.src_segments[i]);
        kfree(sg_data.dst_segments[i]);
    }

    dma_release_channel(sg_data.chan);
    pr_info("Scatter-gather DMA example completed\n");
    return 0;

unmap_dst_sg:
    dma_unmap_sg(sg_data.chan->device->dev, sg_data.dst_sg, NUM_SEGS, DMA_FROM_DEVICE);
unmap_src_sg:
    dma_unmap_sg(sg_data.chan->device->dev, sg_data.src_sg, NUM_SEGS, DMA_TO_DEVICE);
free_segments:
    for (i = 0; i < NUM_SEGS; i++) {
        kfree(sg_data.src_segments[i]);
        kfree(sg_data.dst_segments[i]);
    }
    if (sg_data.chan)
        dma_release_channel(sg_data.chan);
    return result;
}

static void __exit sg_dma_exit(void)
{
    pr_info("Exiting scatter-gather DMA example\n");
}

module_init(sg_dma_init);
module_exit(sg_dma_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("Scatter-Gather DMA Example");
