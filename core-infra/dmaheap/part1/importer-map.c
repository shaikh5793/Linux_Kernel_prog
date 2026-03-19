/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 *
 * importer-map.c - DMA heap buffer import and DMA mapping (Part 1)
 *
 * This module receives a DMA heap buffer fd from userspace via ioctl,
 * imports it using the dma_buf framework, and retrieves the DMA address
 * via scatter-gather mapping.
 *
 * Key concepts:
 * - dma_buf_get() to import a userspace fd into kernel
 * - dma_buf_attach() / dma_buf_map_attachment() for DMA mapping
 * - Dummy platform device providing struct device with DMA mask
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>

#define MY_DMA_IOCTL_MAP _IOW('M', 2, int)

/* Global pointer to our dummy platform device */
static struct platform_device *dummy_dma_pdev;

/* --- IOCTL handler --- */

/**
 * my_dma_ioctl() - Import a DMA heap buffer and retrieve its DMA address
 * @file: File pointer (unused)
 * @cmd: IOCTL command (must be MY_DMA_IOCTL_MAP)
 * @arg: Pointer to userspace int holding the dma_buf file descriptor
 *
 * Calling context: Process context via ioctl() from userspace.
 *
 * Call chain:
 *   userspace ioctl(driver_fd, MY_DMA_IOCTL_MAP, &buf_fd)
 *     -> my_dma_ioctl()
 *       -> dma_buf_get(fd)         -- convert fd to struct dma_buf *
 *       -> dma_buf_attach()        -- attach buffer to our device
 *       -> dma_buf_map_attachment() -- map for DMA, returns sg_table
 *       -> sg_dma_address()        -- extract DMA address
 *       -> cleanup: unmap, detach, put
 *
 * Return: 0 on success, negative errno on failure.
 */
static long my_dma_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int user_fd;
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attachment;
	struct sg_table *sgt;
	enum dma_data_direction direction = DMA_BIDIRECTIONAL;
	int ret = 0;
	dma_addr_t dma_addr;
	struct device *dev = &dummy_dma_pdev->dev;  /* Use our dummy platform device's dev */

	switch (cmd) {
	case MY_DMA_IOCTL_MAP:
		if (copy_from_user(&user_fd, (int __user *)arg, sizeof(user_fd)))
			return -EFAULT;
		pr_info("dummy_dma: Received DMA buf fd: %d\n", user_fd);

		/* Step 1: Convert fd to dma_buf reference */
		dmabuf = dma_buf_get(user_fd);
		if (IS_ERR(dmabuf)) {
			pr_err("dummy_dma: Failed to import dma_buf\n");
			return PTR_ERR(dmabuf);
		}
		pr_info("dummy_dma: Imported dma_buf at %p, size: %zu\n", dmabuf, dmabuf->size);

		/* Step 2: Attach the buffer to our device */
		attachment = dma_buf_attach(dmabuf, dev);
		if (IS_ERR(attachment)) {
			pr_err("dummy_dma: Failed to attach dma_buf\n");
			ret = PTR_ERR(attachment);
			goto err_put;
		}

		/* Step 3: Map the attachment for DMA — returns scatter-gather table */
		sgt = dma_buf_map_attachment(attachment, direction);
		if (IS_ERR(sgt)) {
			pr_err("dummy_dma: Failed to map attachment\n");
			ret = PTR_ERR(sgt);
			dma_buf_detach(dmabuf, attachment);
			goto err_put;
		}

		/* Step 4: Extract DMA address from the first SG entry */
		if (sgt->sgl) {
			dma_addr = sg_dma_address(sgt->sgl);
			pr_info("dummy_dma: DMA address of buffer: %pad\n", &dma_addr);
		} else {
			pr_err("dummy_dma: sg_table is empty\n");
		}

		/* Steps 5-6: Unmap, detach, and release the dma_buf */
		dma_buf_unmap_attachment(attachment, sgt, direction);
		dma_buf_detach(dmabuf, attachment);
err_put:
		dma_buf_put(dmabuf);
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
	.name  = "dummy_dma_map_device",
	.fops  = &my_dma_fops,
};

/* --- Platform device and module lifecycle --- */

/**
 * dummy_dma_init() - Register dummy platform device and misc device
 *
 * Steps:
 *   1. Register a dummy platform device (provides struct device for DMA attach)
 *   2. Set a 32-bit DMA mask on the device
 *   3. Register a misc device exposing the ioctl interface
 *
 * Return: 0 on success, negative errno on failure.
 */
static int __init dummy_dma_init(void)
{
	int ret;

	/* Create a dummy platform device with no resources */
	dummy_dma_pdev = platform_device_register_simple("dummy_dma", -1, NULL, 0);
	if (IS_ERR(dummy_dma_pdev)) {
		pr_err("dummy_dma: Failed to register dummy platform device\n");
		return PTR_ERR(dummy_dma_pdev);
	}
	pr_info("dummy_dma: Dummy platform device registered\n");

	/* Set a DMA mask for the dummy device (adjust if 64-bit DMA is needed) */
	ret = dma_set_mask_and_coherent(&dummy_dma_pdev->dev, DMA_BIT_MASK(32));
	if (ret) {
		pr_err("dummy_dma: Failed to set DMA mask\n");
		platform_device_unregister(dummy_dma_pdev);
		return ret;
	}

	ret = misc_register(&my_dma_misc_device);
	if (ret) {
		pr_err("dummy_dma: Failed to register misc device\n");
		platform_device_unregister(dummy_dma_pdev);
		return ret;
	}
	pr_info("dummy_dma: Device registered as /dev/%s\n", my_dma_misc_device.name);
	return 0;
}

/**
 * dummy_dma_exit() - Deregister misc device and platform device
 *
 * Reverse of init: misc_deregister then platform_device_unregister.
 */
static void __exit dummy_dma_exit(void)
{
	misc_deregister(&my_dma_misc_device);
	platform_device_unregister(dummy_dma_pdev);
	pr_info("dummy_dma: Module unloaded\n");
}

module_init(dummy_dma_init);
module_exit(dummy_dma_exit);

/* --- Module metadata --- */

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("Dummy Platform Device for DMA mapping using dma_buf");
MODULE_LICENSE("GPL");
MODULE_IMPORT_NS("DMA_BUF");
