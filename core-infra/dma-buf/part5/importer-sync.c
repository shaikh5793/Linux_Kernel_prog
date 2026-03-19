/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 *
 * DMA-BUF Importer with synchronization (Part 5)
 *
 * Receives a DMA-BUF fd from userspace, exercises DMA (attach/map SG)
 * and CPU (begin_cpu_access → vmap → vunmap → end_cpu_access) paths.
 * The NEW concept here is the sync wrappers around CPU access.
 */

#include <linux/dma-buf.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/iosys-map.h>
#include <linux/ioctl.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>

MODULE_IMPORT_NS("DMA_BUF");

#define IMPORTER_SYNC_SET_DMABUF_FD _IOW('I', 2, int)

/* Platform device providing struct device for DMA attach */
static struct platform_device *importer_pdev;

/**
 * importer_test_dma() - Test DMA access to imported DMA-BUF
 *
 * Calling Context:
 *   Called from importer_ioctl() after dma_buf_get().
 *
 * Call Chain:
 *   importer_ioctl() -> importer_test_dma()
 *
 * Steps:
 *   1. dma_buf_attach() to bind our device to the buffer.
 *   2. dma_buf_map_attachment() to get the exporter's sg_table.
 *   3. Iterate the SG entries and log DMA addresses.
 *   4. dma_buf_unmap_attachment() + dma_buf_detach() to clean up.
 */
static int importer_test_dma(struct dma_buf *dmabuf)
{
	struct dma_buf_attachment *attachment;
	struct sg_table *sgt;
	struct scatterlist *sg;
	int i;

	pr_info("Sync-Importer: DMA test — attaching to buffer\n");

	attachment = dma_buf_attach(dmabuf, &importer_pdev->dev);
	if (IS_ERR(attachment)) {
		pr_err("Sync-Importer: dma_buf_attach failed: %ld\n",
		       PTR_ERR(attachment));
		return PTR_ERR(attachment);
	}

	sgt = dma_buf_map_attachment(attachment, DMA_FROM_DEVICE);
	if (IS_ERR(sgt)) {
		pr_err("Sync-Importer: dma_buf_map_attachment failed: %ld\n",
		       PTR_ERR(sgt));
		dma_buf_detach(dmabuf, attachment);
		return PTR_ERR(sgt);
	}

	pr_info("Sync-Importer: SG table received (nents=%u)\n", sgt->nents);
	for_each_sgtable_dma_sg(sgt, sg, i) {
		pr_info("Sync-Importer:   SG[%d] dma_addr=%pad len=%u\n",
			i, &sg_dma_address(sg), sg_dma_len(sg));
	}

	dma_buf_unmap_attachment(attachment, sgt, DMA_FROM_DEVICE);
	dma_buf_detach(dmabuf, attachment);
	pr_info("Sync-Importer: DMA test passed — detached\n");

	return 0;
}

/**
 * importer_test_cpu() - Test synchronized CPU access to imported DMA-BUF
 *
 * Calling Context:
 *   Called from importer_ioctl() after dma_buf_get().
 *
 * Call Chain:
 *   importer_ioctl() -> importer_test_cpu()
 *
 * Steps:
 *   1. dma_buf_begin_cpu_access() — triggers exporter's sync logic.
 *   2. dma_buf_vmap() to get a kernel virtual mapping.
 *   3. Read and log buffer content.
 *   4. dma_buf_vunmap() to release the mapping.
 *   5. dma_buf_end_cpu_access() — signals sync completion.
 */
static int importer_test_cpu(struct dma_buf *dmabuf)
{
	struct iosys_map map;
	int ret;

	pr_info("Sync-Importer: CPU test — synchronized access (size: %zu)\n",
		dmabuf->size);

	/* Begin CPU access synchronization */
	ret = dma_buf_begin_cpu_access(dmabuf, DMA_FROM_DEVICE);
	if (ret) {
		pr_err("Sync-Importer: begin_cpu_access failed: %d\n", ret);
		return ret;
	}

	ret = dma_buf_vmap(dmabuf, &map);
	if (ret) {
		pr_err("Sync-Importer: dma_buf_vmap failed: %d\n", ret);
		goto end_access;
	}

	if (iosys_map_is_null(&map)) {
		pr_err("Sync-Importer: vmap returned null mapping\n");
		dma_buf_vunmap(dmabuf, &map);
		ret = -ENOMEM;
		goto end_access;
	}

	pr_info("Sync-Importer: Buffer content: \"%s\"\n", (char *)map.vaddr);
	pr_info("Sync-Importer: CPU test passed\n");
	dma_buf_vunmap(dmabuf, &map);

end_access:
	/* End CPU access synchronization */
	dma_buf_end_cpu_access(dmabuf, DMA_FROM_DEVICE);
	return ret;
}

/**
 * importer_ioctl() - Handle userspace IOCTL requests for synchronized buffers
 *
 * Calling Context:
 *   Called when userspace performs ioctl() on /dev/importer-sync.
 *
 * Call Chain:
 *   userspace ioctl() -> vfs_ioctl() -> importer_ioctl()
 *
 * Steps:
 *   1. Validate the IOCTL command.
 *   2. Copy the file descriptor from userspace.
 *   3. dma_buf_get(fd) to convert fd back to a dma_buf.
 *   4. Exercise DMA path (attach/map SG/unmap/detach).
 *   5. Exercise synchronized CPU path (begin/vmap/vunmap/end).
 *   6. dma_buf_put() to release the reference.
 */
static long importer_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int fd;
	struct dma_buf *dmabuf;
	int ret;

	if (cmd != IMPORTER_SYNC_SET_DMABUF_FD) {
		pr_err("Sync-Importer: Invalid IOCTL command: %u\n", cmd);
		return -EINVAL;
	}

	if (copy_from_user(&fd, (int __user *)arg, sizeof(fd))) {
		pr_err("Sync-Importer: Failed to copy fd from userspace\n");
		return -EFAULT;
	}

	pr_info("Sync-Importer: Received fd %d from userspace\n", fd);

	dmabuf = dma_buf_get(fd);
	if (IS_ERR(dmabuf)) {
		pr_err("Sync-Importer: dma_buf_get(%d) failed: %ld\n",
		       fd, PTR_ERR(dmabuf));
		return PTR_ERR(dmabuf);
	}

	pr_info("Sync-Importer: Imported DMA-BUF (size=%zu) from fd %d\n",
		dmabuf->size, fd);

	/* Test 1: DMA access (attach/map SG/unmap/detach) */
	ret = importer_test_dma(dmabuf);
	if (ret) {
		pr_err("Sync-Importer: DMA test failed: %d\n", ret);
		dma_buf_put(dmabuf);
		return ret;
	}

	/* Test 2: Synchronized CPU access (begin/vmap/read/vunmap/end) */
	ret = importer_test_cpu(dmabuf);
	if (ret) {
		pr_err("Sync-Importer: CPU test failed: %d\n", ret);
		dma_buf_put(dmabuf);
		return ret;
	}

	dma_buf_put(dmabuf);
	pr_info("Sync-Importer: Both DMA and synchronized CPU tests passed\n");

	return 0;
}

/* File operations for the misc device */
static const struct file_operations importer_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = importer_ioctl,
};

/* Misc device structure for /dev/importer-sync */
static struct miscdevice mdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "importer-sync",
	.fops = &importer_fops,
};

static int __init importer_init(void)
{
	int ret;

	/* Create platform device for DMA attach */
	importer_pdev = platform_device_register_simple("sync_importer", -1,
							NULL, 0);
	if (IS_ERR(importer_pdev)) {
		ret = PTR_ERR(importer_pdev);
		pr_err("Sync-Importer: platform_device_register failed: %d\n", ret);
		return ret;
	}

	ret = dma_set_mask_and_coherent(&importer_pdev->dev, DMA_BIT_MASK(64));
	if (ret) {
		pr_err("Sync-Importer: dma_set_mask failed: %d\n", ret);
		platform_device_unregister(importer_pdev);
		return ret;
	}

	ret = misc_register(&mdev);
	if (ret) {
		pr_err("Sync-Importer: misc_register failed: %d\n", ret);
		platform_device_unregister(importer_pdev);
		return ret;
	}

	pr_info("DMA-BUF Importer (sync) initialized — /dev/importer-sync ready\n");
	return 0;
}

static void __exit importer_exit(void)
{
	misc_deregister(&mdev);
	platform_device_unregister(importer_pdev);
	pr_info("DMA-BUF Importer (sync) exited\n");
}

module_init(importer_init);
module_exit(importer_exit);

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("DMA-BUF Importer with sync — DMA and synchronized CPU access");
MODULE_LICENSE("GPL v2");
