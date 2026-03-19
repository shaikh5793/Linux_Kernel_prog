/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

static struct platform_device *importer_pdev;
static struct device *importer_dev;

extern struct dma_buf *get_exporter_dmabuf(void);

/*
 * setup_dma_transfer() - Sets up and verifies a DMA transfer.
 *
 * Calling Context:
 *   This function is called from the module's initialization routine
 *   (`importer_init`) after obtaining a reference to the exporter's `dma_buf`.
 *
 * Call Chain:
 *   importer_init() -> setup_dma_transfer()
 *
 * Steps to be handled:
 *   1. Attach the importer's device to the shared `dma_buf`.
 *   2. Map the attachment for DMA, which triggers the exporter's
 *      `map_dma_buf` operation and returns a scatter-gather table (`sg_table`).
 *   3. Log the DMA address obtained from the `sg_table` to verify the mapping.
 *   4. Unmap the DMA attachment to release the mapping.
 *   5. Detach from the `dma_buf` to release the attachment.
 */
static int setup_dma_transfer(struct dma_buf *dmabuf)
{
    struct dma_buf_attachment *attachment;
    struct sg_table *sgt;
    enum dma_data_direction direction = DMA_BIDIRECTIONAL;

    /* Attach to the DMA-BUF */
    attachment = dma_buf_attach(dmabuf, importer_dev);
    if (IS_ERR(attachment)) {
        pr_err("Failed to attach to DMA-BUF\n");
        return PTR_ERR(attachment);
    }

    /* Map the attachment to get scatter-gather table */
    sgt = dma_buf_map_attachment(attachment, direction);
    if (IS_ERR(sgt)) {
        pr_err("Failed to map DMA-BUF attachment\n");
        dma_buf_detach(dmabuf, attachment);
        return PTR_ERR(sgt);
    }

    if (sgt->sgl) {
        dma_addr_t dma_addr = sg_dma_address(sgt->sgl);
        pr_info("Importer: DMA address of buffer: %pad, nents: %d\n",
                &dma_addr, sgt->nents);
    } else {
        pr_err("Importer: sg_table is empty\n");
    }

    /* Cleanup */
    dma_buf_unmap_attachment(attachment, sgt, direction);
    dma_buf_detach(dmabuf, attachment);
    return 0;
}

static int __init importer_init(void)
{
    struct dma_buf *dmabuf;
    int ret;

    /* Create a platform device to serve as the importer device */
    importer_pdev = platform_device_register_simple("dmabuf_importer", -1, NULL, 0);
    if (IS_ERR(importer_pdev)) {
        pr_err("Failed to register importer platform device\n");
        return PTR_ERR(importer_pdev);
    }
    importer_dev = &importer_pdev->dev;

    /* Set DMA mask for the device */
    ret = dma_set_mask_and_coherent(importer_dev, DMA_BIT_MASK(32));
    if (ret) {
        pr_err("Failed to set DMA mask\n");
        platform_device_unregister(importer_pdev);
        return ret;
    }

    /* Get the DMA-BUF from the exporter */
    dmabuf = get_exporter_dmabuf();
    if (!dmabuf) {
        pr_err("Failed to get DMA-BUF from exporter\n");
        platform_device_unregister(importer_pdev);
        return -ENODEV;
    }

    /* Set up the DMA transfer */
    ret = setup_dma_transfer(dmabuf);
    if (ret) {
        pr_err("Failed to set up DMA transfer\n");
        platform_device_unregister(importer_pdev);
        return ret;
    }

    pr_info("Importer module loaded and DMA transfer initiated\n");
    return 0;
}

static void __exit importer_exit(void)
{
    platform_device_unregister(importer_pdev);
    pr_info("Importer module unloaded\n");
}

module_init(importer_init);
module_exit(importer_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("DMA-BUF Importer Module");
MODULE_SOFTDEP("pre: exporter");
MODULE_IMPORT_NS("DMA_BUF");
