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

static struct platform_device *importer_pdev;
static struct device *importer_dev;

/* Simulate setting up a DMA transfer */
/*
 * setup_dma_transfer() - Simulates setting up a DMA transfer.
 *
 * Calling Context:
 *   This function is called from `importer_ioctl()` after a `dma_buf` has
 *   been imported from a file descriptor.
 *
 * Call Chain:
 *   importer_ioctl() -> setup_dma_transfer()
 *
 * Steps to be handled:
 *   1. Attach the importer's device to the shared `dma_buf`.
 *   2. Map the attachment for DMA, which returns a scatter-gather table.
 *      This call implicitly handles synchronization by waiting for any
 *      pending fences.
 *   3. Log information about the obtained `sg_table`.
 *   4. Unmap the DMA attachment to release the mapping.
 *   5. Detach from the `dma_buf`.
 */static int setup_dma_transfer(struct dma_buf *dmabuf)
{
    struct dma_buf_attachment *attachment;
    struct sg_table *sgt;
    enum dma_data_direction direction = DMA_TO_DEVICE; /* Device reads from buffer */

    attachment = dma_buf_attach(dmabuf, importer_dev);
    if (IS_ERR(attachment)) {
        pr_err("Importer: Failed to attach to DMA-BUF: %ld\n", PTR_ERR(attachment));
        return PTR_ERR(attachment);
    }

    /* 
     * Synchronization: dma_buf_map_attachment() waits for any existing fences in the
     * reservation object (e.g., set by user-space via DMA_BUF_SYNC_END), ensuring
     * that CPU access completes before the DMA operation begins.
     */
    sgt = dma_buf_map_attachment(attachment, direction);
    if (IS_ERR(sgt)) {
        pr_err("Importer: Failed to map DMA-BUF attachment: %ld\n", PTR_ERR(sgt));
        dma_buf_detach(dmabuf, attachment);
        return PTR_ERR(sgt);
    }

    /* Simulate DMA transfer (replace with actual dmaengine calls in a real driver) */
    pr_info("Importer: DMA transfer set up with %d scatterlist entries\n", sgt->nents);

    /* Cleanup */
    dma_buf_unmap_attachment(attachment, sgt, direction);
    dma_buf_detach(dmabuf, attachment);
    return 0;
}

/* Ioctl Handler */
/*
 * importer_ioctl() - Imports a DMA-BUF from a file descriptor.
 *
 * Calling Context:
 *   This function is called when a userspace process performs an `ioctl()`
 *   on the `/dev/importer` device file.
 *
 * Call Chain:
 *   userspace ioctl() -> vfs_ioctl() -> importer_ioctl()
 *
 * Steps to be handled:
 *   1. Get the file descriptor from the userspace argument.
 *   2. Convert the file descriptor into a `dma_buf` structure using
 *      `dma_buf_get()`.
 *   3. Call `setup_dma_transfer()` to simulate a DMA operation.
 *   4. Release the reference to the `dma_buf`.
 */static long importer_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    if (cmd == IMPORTER_IOC_SET_DMABUF) {
        int fd = (int)arg;
        struct dma_buf *dmabuf = dma_buf_get(fd);
        if (IS_ERR(dmabuf)) {
            pr_err("Importer: Failed to get DMA-BUF from fd %d: %ld\n", fd, PTR_ERR(dmabuf));
            return PTR_ERR(dmabuf);
        }
        pr_info("Importer: Received DMA-BUF fd %d\n", fd);
        int ret = setup_dma_transfer(dmabuf);
        dma_buf_put(dmabuf); /* Drop our reference */
        return ret;
    }
    return -EINVAL;
}

static const struct file_operations importer_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = importer_ioctl,
};

static struct miscdevice importer_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "importer",
    .fops = &importer_fops,
};

static int __init importer_init(void)
{
    int ret;
    importer_pdev = platform_device_register_simple("importer", -1, NULL, 0);
    if (IS_ERR(importer_pdev))
        return PTR_ERR(importer_pdev);
    importer_dev = &importer_pdev->dev;
    importer_misc.parent = importer_dev;
    ret = misc_register(&importer_misc);
    if (ret) {
        platform_device_unregister(importer_pdev);
        return ret;
    }
    pr_info("Importer module loaded\n");
    return 0;
}

static void __exit importer_exit(void)
{
    misc_deregister(&importer_misc);
    platform_device_unregister(importer_pdev);
    pr_info("Importer module unloaded\n");
}

module_init(importer_init);
module_exit(importer_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("DMA-BUF Importer Module with Synchronization");
