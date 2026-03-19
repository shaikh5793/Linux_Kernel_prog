/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 *
 * importer-array.c - Array of DMA heap buffer imports (Part 2)
 *
 * This module receives an array of DMA heap buffer fds from userspace via
 * a single ioctl, imports each using the dma_buf framework, and retrieves
 * per-buffer DMA addresses via scatter-gather mapping.
 *
 * Key concepts:
 * - Custom ioctl struct carrying an array of dma_buf file descriptors
 * - Per-buffer import loop: dma_buf_get / attach / map for each fd
 * - Graceful per-buffer error handling (continue on individual failures)
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>

#define MAX_BUFFERS 4
#define MY_DMA_IOCTL_MAP_ARRAY _IOW('M', 3, struct dma_buffer_array)

/**
 * struct dma_buffer_array - Bundle of DMA buffer file descriptors
 * @count: Number of valid entries in @fds (max MAX_BUFFERS)
 * @fds:   Array of dma_buf file descriptors from userspace
 */
struct dma_buffer_array {
	__u32 count;
	int fds[MAX_BUFFERS];
};

/* Global pointer to our dummy platform device */
static struct platform_device *dummy_dma_pdev;

/* --- IOCTL handler --- */

/**
 * my_dma_ioctl() - Import and map an array of DMA heap buffers
 * @file: File pointer (unused)
 * @cmd: IOCTL command (must be MY_DMA_IOCTL_MAP_ARRAY)
 * @arg: Pointer to userspace struct dma_buffer_array
 *
 * Calling context: Process context via ioctl() from userspace.
 *
 * Call chain (per buffer in the array):
 *   copy_from_user(&buf_array)
 *   for each fd in buf_array.fds[]:
 *     -> dma_buf_get(fd)
 *     -> dma_buf_attach(dmabuf, dev)
 *     -> dma_buf_map_attachment(attachment, direction)
 *     -> sg_dma_address(sgt->sgl)
 *     -> unmap / detach / put
 *
 * On per-buffer failure, the loop continues to the next buffer rather
 * than aborting the entire ioctl.
 *
 * Return: 0 on success, negative errno on failure.
 */
static long my_dma_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0, i;
	struct dma_buffer_array buf_array;
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attachment;
	struct sg_table *sgt;
	enum dma_data_direction direction = DMA_BIDIRECTIONAL;
	dma_addr_t dma_addr;
	struct device *dev = &dummy_dma_pdev->dev;  /* Use our dummy device */

	switch (cmd) {
	case MY_DMA_IOCTL_MAP_ARRAY:
		if (copy_from_user(&buf_array, (void __user *)arg, sizeof(buf_array)))
			return -EFAULT;
		if (buf_array.count > MAX_BUFFERS) {
			pr_err("array count %u exceeds MAX_BUFFERS %d\n", buf_array.count, MAX_BUFFERS);
			return -EINVAL;
		}
		pr_info("dummy_dma_array: Received %u DMA buffer fds\n", buf_array.count);

		/* Loop over each buffer fd — import, attach, map, log DMA addr */
		for (i = 0; i < buf_array.count; i++) {
			pr_info("dummy_dma_array: Processing buffer %d, fd %d\n", i, buf_array.fds[i]);
			dmabuf = dma_buf_get(buf_array.fds[i]);
			if (IS_ERR(dmabuf)) {
				pr_err("dummy_dma_array: Failed to get dma_buf for fd %d\n", buf_array.fds[i]);
				ret = PTR_ERR(dmabuf);
				continue;
			}
			pr_info("dummy_dma_array: Imported dma_buf %p, size: %zu\n", dmabuf, dmabuf->size);

			attachment = dma_buf_attach(dmabuf, dev);
			if (IS_ERR(attachment)) {
				pr_err("dummy_dma_array: Failed to attach dma_buf for fd %d\n", buf_array.fds[i]);
				ret = PTR_ERR(attachment);
				dma_buf_put(dmabuf);
				continue;
			}

			sgt = dma_buf_map_attachment(attachment, direction);
			if (IS_ERR(sgt)) {
				pr_err("dummy_dma_array: Failed to map attachment for fd %d\n", buf_array.fds[i]);
				ret = PTR_ERR(sgt);
				dma_buf_detach(dmabuf, attachment);
				dma_buf_put(dmabuf);
				continue;
			}

			if (sgt->sgl) {
				dma_addr = sg_dma_address(sgt->sgl);
				pr_info("dummy_dma_array: Buffer %d DMA address: %pad\n", i, &dma_addr);
			} else {
				pr_err("dummy_dma_array: sg_table is empty for fd %d\n", buf_array.fds[i]);
			}
			dma_buf_unmap_attachment(attachment, sgt, direction);
			dma_buf_detach(dmabuf, attachment);
			dma_buf_put(dmabuf);
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

/* --- File operations --- */

static const struct file_operations my_dma_fops = {
	.owner          = THIS_MODULE,
	.unlocked_ioctl = my_dma_ioctl,
	.compat_ioctl   = my_dma_ioctl,
};

static struct miscdevice my_dma_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = "dummy_dma_array_device",
	.fops  = &my_dma_fops,
};

/* --- Platform device and module lifecycle --- */

/**
 * dummy_dma_array_init() - Register dummy platform device and misc device
 *
 * Steps:
 *   1. Register a dummy platform device (provides struct device for DMA attach)
 *   2. Set a 32-bit DMA mask on the device
 *   3. Register a misc device exposing the ioctl interface
 *
 * Return: 0 on success, negative errno on failure.
 */
static int __init dummy_dma_array_init(void)
{
	int ret;

	/* Create a dummy platform device with no resources */
	dummy_dma_pdev = platform_device_register_simple("dummy_dma_array", -1, NULL, 0);
	if (IS_ERR(dummy_dma_pdev)) {
		pr_err("dummy_dma_array: Failed to register dummy platform device\n");
		return PTR_ERR(dummy_dma_pdev);
	}
	pr_info("dummy_dma_array: Dummy platform device registered\n");

	/* Set a DMA mask for the dummy device (adjust if 64-bit DMA is needed) */
	ret = dma_set_mask_and_coherent(&dummy_dma_pdev->dev, DMA_BIT_MASK(32));
	if (ret) {
		pr_err("dummy_dma_array: Failed to set DMA mask\n");
		platform_device_unregister(dummy_dma_pdev);
		return ret;
	}

	ret = misc_register(&my_dma_misc_device);
	if (ret) {
		pr_err("dummy_dma_array: Failed to register misc device\n");
		platform_device_unregister(dummy_dma_pdev);
		return ret;
	}
	pr_info("dummy_dma_array: Device registered as /dev/%s\n", my_dma_misc_device.name);
	return 0;
}

/**
 * dummy_dma_array_exit() - Deregister misc device and platform device
 */
static void __exit dummy_dma_array_exit(void)
{
	misc_deregister(&my_dma_misc_device);
	platform_device_unregister(dummy_dma_pdev);
	pr_info("dummy_dma_array: Module unloaded\n");
}

module_init(dummy_dma_array_init);
module_exit(dummy_dma_array_exit);

/* --- Module metadata --- */

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("Driver that processes an array of DMA buffers allocated from dma_heap");
MODULE_LICENSE("GPL");
MODULE_IMPORT_NS("DMA_BUF");
