/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 *
 * DMA-BUF Exporter with FD sharing (Part 4)
 *
 * This module exports a DMA-BUF and provides its file descriptor to
 * userspace via IOCTL. The userspace intermediary (userfd.c) then
 * passes the fd to the importer module.
 *
 * Key concepts:
 * - dma_buf_fd() for creating userspace-visible file descriptors
 * - FD-based buffer sharing pipeline: exporter -> user -> importer
 */

#include <linux/dma-buf.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/iosys-map.h>
#include <linux/ioctl.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>

/* IOCTL command to get DMA-BUF file descriptor */
#define EXPORTER_SHARE_GET_DMABUF_FD _IOR('E', 1, int)

/* Global DMA-BUF pointer */
static struct dma_buf *dmabuf_fd_exported;

/*
 * exporter_map_dma_buf() - Map buffer for DMA access via scatter-gather.
 *
 * Calling Context:
 *   Invoked by the DMA-BUF framework when an importer calls
 *   dma_buf_map_attachment().
 *
 * Call Chain:
 *   importer -> dma_buf_map_attachment() -> exporter_map_dma_buf()
 *
 * Steps:
 *   1. Allocate a 1-entry sg_table.
 *   2. dma_map_single() for the importer's device.
 *   3. Store DMA address in the sg entry.
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
static struct sg_table *exporter_map_dma_buf(struct dma_buf_attachment *attachment,
					     enum dma_data_direction dir)
{
	void *vaddr = attachment->dmabuf->priv;
	struct sg_table *sgt;
	dma_addr_t dma_addr;
	int ret;

	sgt = kmalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt)
		return ERR_PTR(-ENOMEM);

	ret = sg_alloc_table(sgt, 1, GFP_KERNEL);
	if (ret) {
		kfree(sgt);
		return ERR_PTR(ret);
	}

	dma_addr = dma_map_single(attachment->dev, vaddr, PAGE_SIZE, dir);
	if (dma_mapping_error(attachment->dev, dma_addr)) {
		sg_free_table(sgt);
		kfree(sgt);
		return ERR_PTR(-ENOMEM);
	}

	sg_dma_address(sgt->sgl) = dma_addr;
	sg_dma_len(sgt->sgl) = PAGE_SIZE;

	pr_info("FD-Exporter: Mapped buffer for DMA at %pad\n", &dma_addr);
	return sgt;
}

static void exporter_unmap_dma_buf(struct dma_buf_attachment *attachment,
				   struct sg_table *sgt,
				   enum dma_data_direction dir)
{
	dma_unmap_single(attachment->dev, sg_dma_address(sgt->sgl),
			 PAGE_SIZE, dir);
	sg_free_table(sgt);
	kfree(sgt);
}

/**
 * exporter_release() - Release DMA-BUF resources
 */
static void exporter_release(struct dma_buf *dmabuf)
{
	if (dmabuf && dmabuf->priv) {
		pr_info("FD-Exporter: Releasing buffer (size: %zu bytes)\n",
			dmabuf->size);
		kfree(dmabuf->priv);
		dmabuf->priv = NULL;
	}
	dmabuf_fd_exported = NULL;
}

/**
 * exporter_vmap() - Map buffer for kernel CPU access
 */
static int exporter_vmap(struct dma_buf *dmabuf, struct iosys_map *map)
{
	iosys_map_set_vaddr(map, dmabuf->priv);
	return 0;
}

/**
 * exporter_vunmap() - Unmap kernel CPU access
 */
static void exporter_vunmap(struct dma_buf *dmabuf, struct iosys_map *map)
{
	/* No cleanup needed */
}

/* DMA-BUF operations */
static const struct dma_buf_ops exp_dmabuf_ops = {
	.map_dma_buf = exporter_map_dma_buf,
	.unmap_dma_buf = exporter_unmap_dma_buf,
	.release = exporter_release,
	.vmap = exporter_vmap,
	.vunmap = exporter_vunmap,
};

/**
 * exporter_alloc_page() - Allocate buffer and export as DMA-BUF
 */
static struct dma_buf *exporter_alloc_page(void)
{
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	struct dma_buf *dmabuf;
	void *vaddr;

	vaddr = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (!vaddr) {
		pr_err("FD-Exporter: Failed to allocate buffer\n");
		return ERR_PTR(-ENOMEM);
	}

	exp_info.ops = &exp_dmabuf_ops;
	exp_info.size = PAGE_SIZE;
	exp_info.flags = O_CLOEXEC;
	exp_info.priv = vaddr;

	dmabuf = dma_buf_export(&exp_info);
	if (IS_ERR(dmabuf)) {
		pr_err("FD-Exporter: Failed to export DMA-BUF: %ld\n", PTR_ERR(dmabuf));
		kfree(vaddr);
		return dmabuf;
	}

	strscpy(vaddr, "hello world!", PAGE_SIZE);
	pr_info("FD-Exporter: Created DMA-BUF with test data: '%s'\n", (char *)vaddr);

	return dmabuf;
}

/**
 * exporter_ioctl() - Deliver DMA-BUF file descriptor to userspace
 *
 * Calling Context:
 *   This function is part of the `file_operations` for the misc device. It is
 *   called when a userspace process performs an `ioctl()` on the device file
 *   (e.g., `/dev/exporter-share`).
 *
 * Call Chain:
 *   userspace ioctl() -> vfs_ioctl() -> exporter_ioctl()
 *
 * Steps:
 *   1. Validate the IOCTL command.
 *   2. Ensure the `dma_buf` has been created.
 *   3. Create a new file descriptor for the `dma_buf` using `dma_buf_fd()`.
 *   4. Copy the new file descriptor back to the userspace process.
 *
 * Note: The exporter and importer are independent subsystems
 * that cannot link to each other's symbols.  dma_buf_fd() converts the
 * kernel dma_buf into a userspace file descriptor so that the application
 * — the only entity that knows the pipeline topology — can pass it to
 * any consumer (importer) it chooses.  In production this is how V4L2
 * VIDIOC_EXPBUF, DRM drmPrimeHandleToFD, etc. deliver buffers to apps.
 */
static long exporter_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int fd;

	if (cmd != EXPORTER_SHARE_GET_DMABUF_FD) {
		pr_err("FD-Exporter: Invalid IOCTL command: %u\n", cmd);
		return -EINVAL;
	}

	if (!dmabuf_fd_exported) {
		pr_err("FD-Exporter: DMA-BUF not exported yet\n");
		return -ENOENT;
	}

	fd = dma_buf_fd(dmabuf_fd_exported, O_CLOEXEC);
	if (fd < 0) {
		pr_err("FD-Exporter: Failed to get DMA-BUF fd: %d\n", fd);
		return fd;
	}

	if (copy_to_user((int __user *)arg, &fd, sizeof(fd))) {
		pr_err("FD-Exporter: Failed to copy fd to userspace\n");
		return -EFAULT;
	}

	pr_info("FD-Exporter: Delivered DMA-BUF fd %d to userspace\n", fd);
	return 0;
}

/* File operations for the misc device */
static const struct file_operations exporter_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = exporter_ioctl,
};

/* Misc device: creates /dev/exporter-share */
static struct miscdevice mdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "exporter-share",
	.fops = &exporter_fops,
};

 static int __init exporter_init(void)
{
	int ret;

	dmabuf_fd_exported = exporter_alloc_page();
	if (IS_ERR(dmabuf_fd_exported)) {
		ret = PTR_ERR(dmabuf_fd_exported);
		pr_err("FD-Exporter: Initialization failed: %d\n", ret);
		return ret;
	}

	ret = misc_register(&mdev);
	if (ret) {
		pr_err("FD-Exporter: Failed to register misc device: %d\n", ret);
		dma_buf_put(dmabuf_fd_exported);
		return ret;
	}

	pr_info("DMA-BUF Exporter (FD share) initialized - /dev/exporter-share ready\n");
	return 0;
}

static void __exit exporter_exit(void)
{
	misc_deregister(&mdev);
	if (!IS_ERR_OR_NULL(dmabuf_fd_exported)) {
		pr_info("FD-Exporter: Dropping reference to DMA-BUF\n");
		dma_buf_put(dmabuf_fd_exported);
	}
	pr_info("DMA-BUF Exporter (FD share) exited\n");
}

module_init(exporter_init);
module_exit(exporter_exit);

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("DMA-BUF Exporter with FD sharing for pipeline");
MODULE_LICENSE("GPL v2");
MODULE_IMPORT_NS("DMA_BUF");
