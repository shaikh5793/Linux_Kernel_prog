/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include "dmabuf_example.h"

#define BUFFER_SIZE 4096

static struct platform_device *exporter_pdev;
static struct device *exporter_dev;

struct exporter_buffer {
    void *buf;
    dma_addr_t dma_handle;
};

/* DMA-BUF Operations */
static int exporter_attach(struct dma_buf *dmabuf, struct dma_buf_attachment *attachment)
{
    pr_info("Exporter: Importer attached to DMA-BUF\n");
    return 0;
}

static void exporter_detach(struct dma_buf *dmabuf, struct dma_buf_attachment *attachment)
{
    pr_info("Exporter: Importer detached from DMA-BUF\n");
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
 *   3. Set the scatter-gather entry to point to the exported buffer.
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
    struct exporter_buffer *exbuf = attachment->dmabuf->priv;
    struct sg_table *sgt = kmalloc(sizeof(*sgt), GFP_KERNEL);
    if (!sgt)
        return ERR_PTR(-ENOMEM);
    if (sg_alloc_table(sgt, 1, GFP_KERNEL)) {
        kfree(sgt);
        return ERR_PTR(-ENOMEM);
    }
    sg_set_page(sgt->sgl, virt_to_page(exbuf->buf), BUFFER_SIZE, offset_in_page(exbuf->buf));
    dma_map_sg(attachment->dev, sgt->sgl, 1, direction);
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
 *   dropped.
 *
 * Call Chain:
 *   dma_buf_put() -> ... -> exporter_release_dmabuf()
 *
 * Steps to be handled:
 *   1. Free the coherent DMA memory.
 *   2. Free the private buffer structure.
 */static void exporter_release_dmabuf(struct dma_buf *dmabuf)
{
    struct exporter_buffer *exbuf = dmabuf->priv;
    dma_free_coherent(exporter_dev, BUFFER_SIZE, exbuf->buf, exbuf->dma_handle);
    kfree(exbuf);
    pr_info("Exporter: DMA-BUF released\n");
}

static const struct dma_buf_ops exporter_dmabuf_ops = {
    .attach = exporter_attach,
    .detach = exporter_detach,
    .map_dma_buf = exporter_map_dmabuf,
    .unmap_dma_buf = exporter_unmap_dmabuf,
    .release = exporter_release_dmabuf,
};

/* Ioctl Handler */
/*
 * exporter_ioctl() - Creates and exports a DMA-BUF, returning its fd.
 *
 * Calling Context:
 *   This function is called when a userspace process performs an `ioctl()`
 *   on the `/dev/exporter` device file.
 *
 * Call Chain:
 *   userspace ioctl() -> vfs_ioctl() -> exporter_ioctl()
 *
 * Steps to be handled:
 *   1. Allocate a private structure to hold buffer info.
 *   2. Allocate a coherent DMA buffer.
 *   3. Define the `dma_buf_export_info` and export the buffer using
 *      `dma_buf_export()`.
 *   4. Create a file descriptor for the `dma_buf` using `dma_buf_fd()`.
 *   5. Return the file descriptor to the userspace process.
 *   6. Handle any errors by cleaning up allocated resources.
 */static long exporter_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    if (cmd == EXPORTER_IOC_GET_DMABUF) {
        struct exporter_buffer *exbuf = kmalloc(sizeof(*exbuf), GFP_KERNEL);
        if (!exbuf)
            return -ENOMEM;
        exbuf->buf = dma_alloc_coherent(exporter_dev, BUFFER_SIZE, &exbuf->dma_handle, GFP_KERNEL);
        if (!exbuf->buf) {
            kfree(exbuf);
            return -ENOMEM;
        }
        struct dma_buf *dmabuf = dma_buf_export(&(struct dma_buf_export_info){
            .exp_name = "exporter_buffer",
            .owner = THIS_MODULE,
            .ops = &exporter_dmabuf_ops,
            .size = BUFFER_SIZE,
            .flags = O_RDWR,
            .priv = exbuf,
        });
        if (IS_ERR(dmabuf)) {
            dma_free_coherent(exporter_dev, BUFFER_SIZE, exbuf->buf, exbuf->dma_handle);
            kfree(exbuf);
            return PTR_ERR(dmabuf);
        }
        int fd = dma_buf_fd(dmabuf, O_CLOEXEC);
        if (fd < 0) {
            dma_buf_put(dmabuf); /* Drop the reference if fd creation fails */
            return fd;
        }
        pr_info("Exporter: DMA-BUF fd %d created\n", fd);
        return fd; /* Return the fd to user-space */
    }
    return -EINVAL;
}

static const struct file_operations exporter_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = exporter_ioctl,
};

static struct miscdevice exporter_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "exporter",
    .fops = &exporter_fops,
};

static int __init exporter_init(void)
{
    int ret;
    exporter_pdev = platform_device_register_simple("exporter", -1, NULL, 0);
    if (IS_ERR(exporter_pdev))
        return PTR_ERR(exporter_pdev);
    exporter_dev = &exporter_pdev->dev;
    exporter_misc.parent = exporter_dev;
    ret = misc_register(&exporter_misc);
    if (ret) {
        platform_device_unregister(exporter_pdev);
        return ret;
    }
    pr_info("Exporter module loaded\n");
    return 0;
}

static void __exit exporter_exit(void)
{
    misc_deregister(&exporter_misc);
    platform_device_unregister(exporter_pdev);
    pr_info("Exporter module unloaded\n");
}

module_init(exporter_init);
module_exit(exporter_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("DMA-BUF Exporter Module");
