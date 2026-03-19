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

#define BUFFER_SIZE 4096

static struct platform_device *exporter_pdev;
static struct device *exporter_dev;
static void *buf;
static dma_addr_t dma_handle;
static struct dma_buf *dmabuf;

/* DMA-BUF operations */
static int exporter_attach(struct dma_buf *dmabuf, struct dma_buf_attachment *attachment)
{
    pr_info("Importer attached to DMA-BUF\n");
    return 0;
}

static void exporter_detach(struct dma_buf *dmabuf, struct dma_buf_attachment *attachment)
{
    pr_info("Importer detached from DMA-BUF\n");
}

/*
 * exporter_map_dmabuf() - Maps the DMA buffer for an importer.
 *
 * Calling Context:
 *   This function is a callback part of the `dma_buf_ops` structure. It is
 *   invoked by the DMA-BUF framework when an importer calls
 *   `dma_buf_map_attachment()`.
 *
 * Call Chain:
 *   importer -> dma_buf_map_attachment() -> exporter_map_dmabuf()
 *
 * Steps to be handled:
 *   1. Allocate a scatter-gather table (`sg_table`).
 *   2. Initialize the table to hold one entry.
 *   3. Set the scatter-gather entry to point to the pre-allocated buffer (`buf`).
 *   4. Map the scatter-gather list for DMA access by the importer's device.
 *   5. Return the mapped `sg_table` to the caller.
 *
 * The dma_buf_ops contract requires map_dma_buf to return
 * an sg_table, not a plain dma_addr_t.
 *   - Buffers may be physically non-contiguous (scattered pages); sg_table
 *     handles both contiguous and scattered cases uniformly.
 *   - Each importer has its own struct device / IOMMU context, so the
 *     mapping must be done per-importer via attachment->dev.
 *   - An IOMMU may coalesce entries (orig_nents vs nents).
 * Even for a single contiguous page we use streaming DMA (dma_map_single)
 * internally, then wrap the result in a 1-entry sg_table.
 *
 */
static struct sg_table *exporter_map_dmabuf(struct dma_buf_attachment *attachment,
                                            enum dma_data_direction direction)
{
    struct sg_table *sgt;

    sgt = kmalloc(sizeof(*sgt), GFP_KERNEL);
    if (!sgt)
        return ERR_PTR(-ENOMEM);

    if (sg_alloc_table(sgt, 1, GFP_KERNEL)) {
        kfree(sgt);
        return ERR_PTR(-ENOMEM);
    }

    sg_set_page(sgt->sgl, virt_to_page(buf), BUFFER_SIZE, offset_in_page(buf));

    if (dma_map_sg(attachment->dev, sgt->sgl, 1, direction) == 0) {
        sg_free_table(sgt);
        kfree(sgt);
        return ERR_PTR(-EIO);
    }

    return sgt;
}

static void exporter_unmap_dmabuf(struct dma_buf_attachment *attachment,
                                  struct sg_table *sgt,
                                  enum dma_data_direction direction)
{
    dma_unmap_sg(attachment->dev, sgt->sgl, 1, direction);
    sg_free_table(sgt);
    kfree(sgt);
}

/*
 * exporter_release_dmabuf() - Releases the DMA buffer.
 *
 * Calling Context:
 *   This function is a callback in the `dma_buf_ops` structure. It is called
 *   by the DMA-BUF framework when the last reference to the `dma_buf` is
 *   dropped (i.e., its reference count becomes zero).
 *
 * Call Chain:
 *   dma_buf_put() -> ... -> exporter_release_dmabuf()
 *
 * Steps to be handled:
 *   1. Free the coherent DMA memory that was allocated in `exporter_init()`.
 *   2. Log a message indicating the buffer has been released.
 */
static void exporter_release_dmabuf(struct dma_buf *dmabuf)
{
    dma_free_coherent(exporter_dev, BUFFER_SIZE, buf, dma_handle);
    pr_info("DMA-BUF released and memory freed\n");
}

static const struct dma_buf_ops exporter_dmabuf_ops = {
    .attach = exporter_attach,
    .detach = exporter_detach,
    .map_dma_buf = exporter_map_dmabuf,
    .unmap_dma_buf = exporter_unmap_dmabuf,
    .release = exporter_release_dmabuf,
};

/* Exported function for the importer to access the DMA-BUF */
struct dma_buf *get_exporter_dmabuf(void)
{
    return dmabuf;
}
EXPORT_SYMBOL(get_exporter_dmabuf);

static int __init exporter_init(void)

{
    int ret;

    /* Create a platform device to serve as the DMA device */
    exporter_pdev = platform_device_register_simple("dmabuf_exporter", -1, NULL, 0);
    if (IS_ERR(exporter_pdev)) {
        pr_err("Failed to register exporter platform device\n");
        return PTR_ERR(exporter_pdev);
    }
    exporter_dev = &exporter_pdev->dev;

    /* Set DMA mask for the device */
    ret = dma_set_mask_and_coherent(exporter_dev, DMA_BIT_MASK(32));
    if (ret) {
        pr_err("Failed to set DMA mask\n");
        goto err_unregister_pdev;
    }

    /* Allocate DMA memory */
    buf = dma_alloc_coherent(exporter_dev, BUFFER_SIZE, &dma_handle, GFP_KERNEL);
    if (!buf) {
        pr_err("Failed to allocate DMA memory\n");
        ret = -ENOMEM;
        goto err_unregister_pdev;
    }

    /* Create DMA-BUF */
    {
        struct dma_buf_export_info exp_info = {
            .exp_name = "exporter_buffer",
            .owner = THIS_MODULE,
            .ops = &exporter_dmabuf_ops,
            .size = BUFFER_SIZE,
            .flags = O_RDWR,
            .priv = buf,
        };

        dmabuf = dma_buf_export(&exp_info);
        if (IS_ERR(dmabuf)) {
            pr_err("Failed to export DMA-BUF\n");
            ret = PTR_ERR(dmabuf);
            goto err_free_dma;
        }
    }

    pr_info("Exporter module loaded, DMA-BUF created\n");
    return 0;

err_free_dma:
    dma_free_coherent(exporter_dev, BUFFER_SIZE, buf, dma_handle);
err_unregister_pdev:
    platform_device_unregister(exporter_pdev);
    return ret;
}

static void __exit exporter_exit(void)
{
    platform_device_unregister(exporter_pdev);
    pr_info("Exporter module unloaded\n");
}

module_init(exporter_init);
module_exit(exporter_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("DMA-BUF Exporter Module");
MODULE_IMPORT_NS("DMA_BUF");
