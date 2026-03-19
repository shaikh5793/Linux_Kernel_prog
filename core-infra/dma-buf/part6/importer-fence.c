/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 *
 * DMA-BUF Importer with Fence Synchronization (Part 6)
 *
 * Receives a DMA-BUF fd from userspace, inspects and waits on any
 * attached dma_fence via the buffer's reservation object (dma_resv),
 * then exercises both DMA (attach/map SG) and synchronized CPU
 * (begin_cpu_access → vmap → vunmap → end_cpu_access) access paths.
 *
 */

#include <linux/dma-buf.h>
#include <linux/dma-fence.h>
#include <linux/dma-resv.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/iosys-map.h>
#include <linux/ioctl.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>

MODULE_IMPORT_NS("DMA_BUF");

#define IMPORTER_FENCE_SET_DMABUF_FD _IOW('I', 3, int)

/* Platform device providing struct device for DMA attach */
static struct platform_device *importer_pdev;

/**
 * importer_inspect_fences() - Inspect fences in the dma_buf's reservation object
 *
 * Calling Context:
 *   Called from importer_ioctl() after dma_buf_get().
 *
 * Call Chain:
 *   importer_ioctl() -> importer_inspect_fences()
 *
 * Steps:
 *   1. Begin iteration over the reservation object's fences.
 *   2. For each fence, log driver name, timeline, and signaled state.
 *   3. End iteration.
 */
static void importer_inspect_fences(struct dma_buf *dmabuf)
{
	struct dma_resv_iter cursor;
	struct dma_fence *fence;

	pr_info("fence_importer: Inspecting fences in dma_buf reservation object\n");

	dma_resv_iter_begin(&cursor, dmabuf->resv, DMA_RESV_USAGE_WRITE);
	dma_resv_for_each_fence(&cursor, dmabuf->resv, DMA_RESV_USAGE_WRITE, fence) {
		pr_info("fence_importer: Found fence — driver: %s, timeline: %s, signaled: %s\n",
			fence->ops->get_driver_name(fence),
			fence->ops->get_timeline_name(fence),
			dma_fence_is_signaled(fence) ? "yes" : "no");
	}
	dma_resv_iter_end(&cursor);
}

/**
 * importer_wait_fence() - Wait for all write fences to signal
 *
 * Calling Context:
 *   Called from importer_ioctl() after fence inspection.
 *
 * Call Chain:
 *   importer_ioctl() -> importer_wait_fence()
 *
 * Steps:
 *   1. Call dma_resv_wait_timeout() with DMA_RESV_USAGE_WRITE and 2s timeout.
 *   2. Return 0 on success, -ETIMEDOUT on timeout, or error code.
 *
 * In real GPU/media drivers, consumers call this before touching the
 * buffer to ensure the producer's hardware has finished writing.
 */
static int importer_wait_fence(struct dma_buf *dmabuf)
{
	long timeout_ret;

	pr_info("fence_importer: Waiting for fence (timeout=2s)...\n");

	timeout_ret = dma_resv_wait_timeout(dmabuf->resv, DMA_RESV_USAGE_WRITE,
					    true, msecs_to_jiffies(2000));
	if (timeout_ret == 0) {
		pr_err("fence_importer: Timeout waiting for fence!\n");
		return -ETIMEDOUT;
	}
	if (timeout_ret < 0) {
		pr_err("fence_importer: Error waiting for fence: %ld\n",
		       timeout_ret);
		return timeout_ret;
	}

	pr_info("fence_importer: Fence signaled! Remaining jiffies: %ld\n",
		timeout_ret);
	return 0;
}

/**
 * importer_test_dma() - Test DMA access to imported DMA-BUF
 *
 * Calling Context:
 *   Called from importer_ioctl() after fence wait completes.
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

	pr_info("fence_importer: DMA test — attaching to buffer\n");

	attachment = dma_buf_attach(dmabuf, &importer_pdev->dev);
	if (IS_ERR(attachment)) {
		pr_err("fence_importer: dma_buf_attach failed: %ld\n",
		       PTR_ERR(attachment));
		return PTR_ERR(attachment);
	}

	sgt = dma_buf_map_attachment(attachment, DMA_FROM_DEVICE);
	if (IS_ERR(sgt)) {
		pr_err("fence_importer: dma_buf_map_attachment failed: %ld\n",
		       PTR_ERR(sgt));
		dma_buf_detach(dmabuf, attachment);
		return PTR_ERR(sgt);
	}

	pr_info("fence_importer: SG table received (nents=%u)\n", sgt->nents);
	for_each_sgtable_dma_sg(sgt, sg, i) {
		pr_info("fence_importer:   SG[%d] dma_addr=%pad len=%u\n",
			i, &sg_dma_address(sg), sg_dma_len(sg));
	}

	dma_buf_unmap_attachment(attachment, sgt, DMA_FROM_DEVICE);
	dma_buf_detach(dmabuf, attachment);
	pr_info("fence_importer: DMA test passed — detached\n");

	return 0;
}

/**
 * importer_test_cpu() - Test synchronized CPU access to imported DMA-BUF
 *
 * Calling Context:
 *   Called from importer_ioctl() after fence wait and DMA test.
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

	pr_info("fence_importer: CPU test — synchronized access (size: %zu)\n",
		dmabuf->size);

	ret = dma_buf_begin_cpu_access(dmabuf, DMA_FROM_DEVICE);
	if (ret) {
		pr_err("fence_importer: begin_cpu_access failed: %d\n", ret);
		return ret;
	}

	ret = dma_buf_vmap(dmabuf, &map);
	if (ret) {
		pr_err("fence_importer: dma_buf_vmap failed: %d\n", ret);
		goto end_access;
	}

	if (iosys_map_is_null(&map)) {
		pr_err("fence_importer: vmap returned null mapping\n");
		dma_buf_vunmap(dmabuf, &map);
		ret = -ENOMEM;
		goto end_access;
	}

	pr_info("fence_importer: Buffer content after fence: \"%s\"\n",
		(char *)map.vaddr);
	pr_info("fence_importer: CPU test passed\n");
	dma_buf_vunmap(dmabuf, &map);

end_access:
	dma_buf_end_cpu_access(dmabuf, DMA_FROM_DEVICE);
	return ret;
}

/**
 * importer_ioctl() - Handle userspace IOCTL requests for fence-synchronized buffers
 *
 * Calling Context:
 *   Called when userspace performs ioctl() on /dev/importer-fence.
 *
 * Call Chain:
 *   userspace ioctl() -> vfs_ioctl() -> importer_ioctl()
 *
 * Steps:
 *   1. Validate the IOCTL command.
 *   2. Copy the file descriptor from userspace.
 *   3. dma_buf_get(fd) to convert fd back to a dma_buf.
 *   4. Inspect fences attached to the buffer's reservation object.
 *   5. Wait for all write fences to signal (dma_resv_wait_timeout).
 *   6. Exercise DMA path (attach/map SG/unmap/detach).
 *   7. Exercise synchronized CPU path (begin/vmap/read/vunmap/end).
 *   8. dma_buf_put() to release the reference.
 */
static long importer_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int fd;
	struct dma_buf *dmabuf;
	int ret;

	if (cmd != IMPORTER_FENCE_SET_DMABUF_FD) {
		pr_err("fence_importer: Invalid IOCTL command: %u\n", cmd);
		return -EINVAL;
	}

	if (copy_from_user(&fd, (int __user *)arg, sizeof(fd))) {
		pr_err("fence_importer: Failed to copy fd from userspace\n");
		return -EFAULT;
	}

	pr_info("fence_importer: Received fd %d from userspace\n", fd);

	dmabuf = dma_buf_get(fd);
	if (IS_ERR(dmabuf)) {
		pr_err("fence_importer: dma_buf_get(%d) failed: %ld\n",
		       fd, PTR_ERR(dmabuf));
		return PTR_ERR(dmabuf);
	}

	pr_info("fence_importer: Imported DMA-BUF (size=%zu) from fd %d\n",
		dmabuf->size, fd);

	/* Step 1: Inspect fences attached to the buffer */
	importer_inspect_fences(dmabuf);

	/* Step 2: Wait for all write fences to signal */
	ret = importer_wait_fence(dmabuf);
	if (ret) {
		pr_err("fence_importer: Fence wait failed: %d\n", ret);
		dma_buf_put(dmabuf);
		return ret;
	}

	/* Step 3: DMA access (attach/map SG/unmap/detach) */
	ret = importer_test_dma(dmabuf);
	if (ret) {
		pr_err("fence_importer: DMA test failed: %d\n", ret);
		dma_buf_put(dmabuf);
		return ret;
	}

	/* Step 4: Synchronized CPU access (begin/vmap/read/vunmap/end) */
	ret = importer_test_cpu(dmabuf);
	if (ret) {
		pr_err("fence_importer: CPU test failed: %d\n", ret);
		dma_buf_put(dmabuf);
		return ret;
	}

	dma_buf_put(dmabuf);
	pr_info("fence_importer: All tests passed (fence wait + DMA + CPU)\n");

	return 0;
}

static const struct file_operations importer_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = importer_ioctl,
};

static struct miscdevice mdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "importer-fence",
	.fops = &importer_fops,
};

static int __init importer_fence_init(void)
{
	int ret;

	/* Create platform device for DMA attach */
	importer_pdev = platform_device_register_simple("fence_importer", -1,
							NULL, 0);
	if (IS_ERR(importer_pdev)) {
		ret = PTR_ERR(importer_pdev);
		pr_err("fence_importer: platform_device_register failed: %d\n", ret);
		return ret;
	}

	ret = dma_set_mask_and_coherent(&importer_pdev->dev, DMA_BIT_MASK(64));
	if (ret) {
		pr_err("fence_importer: dma_set_mask failed: %d\n", ret);
		platform_device_unregister(importer_pdev);
		return ret;
	}

	ret = misc_register(&mdev);
	if (ret) {
		pr_err("fence_importer: misc_register failed: %d\n", ret);
		platform_device_unregister(importer_pdev);
		return ret;
	}

	pr_info("fence_importer: Initialized — /dev/importer-fence ready\n");
	return 0;
}

static void __exit importer_fence_exit(void)
{
	misc_deregister(&mdev);
	platform_device_unregister(importer_pdev);
	pr_info("fence_importer: Module unloaded\n");
}

module_init(importer_fence_init);
module_exit(importer_fence_exit);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("DMA-BUF Fence Importer — fence wait + DMA + synced CPU access");
