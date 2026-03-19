/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 *
 * importer-multisg.c - Multi-entry scatter-gather table traversal (Part 4)
 *
 * This module demonstrates how to iterate over all scatter-gather entries
 * when importing a DMA heap buffer. A 4MB system heap allocation produces
 * multiple SG entries because the system heap's page pool uses order-8
 * (1MB) as its largest page order — so 4MB requires 4 separate page
 * allocations, each becoming its own SG entry.
 *
 * Key concepts:
 * - for_each_sgtable_dma_sg() to iterate all DMA-mapped SG entries
 * - orig_nents (pre-DMA-map count) vs nents (post-DMA-map, IOMMU may coalesce)
 * - System heap page pool orders: [8 (1MB), 4 (64KB), 0 (4KB)]
 * - sg_dma_address() / sg_dma_len() per SG entry
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/iosys-map.h>
#include <linux/scatterlist.h>

#define MY_DMA_IOCTL_MULTISG _IOW('M', 8, int)

/* Global pointer to our dummy platform device */
static struct platform_device *dummy_dma_pdev;

/**
 * print_sg_entries() - Iterate and print all scatter-gather table entries
 * @sgt: Pointer to the DMA-mapped scatter-gather table
 *
 * This demonstrates the difference between orig_nents (before DMA mapping)
 * and nents (after DMA mapping, which may be coalesced by IOMMU).
 * The system heap's page pool caps individual allocations at order 8
 * (1MB), so a 4MB buffer requires 4 separate pages = 4 SG entries.
 *
 * Calling context: Process context (called from ioctl handler).
 */
static void print_sg_entries(struct sg_table *sgt)
{
	struct scatterlist *sg;
	int i;
	size_t total_size = 0;

	pr_info("dma_multisg: === Scatter-Gather Table ===\n");
	pr_info("dma_multisg: orig_nents (before DMA map): %u\n", sgt->orig_nents);
	pr_info("dma_multisg: nents (after DMA map):       %u\n", sgt->nents);

	if (sgt->nents == 1)
		pr_info("dma_multisg: Buffer is physically contiguous (single chunk)\n");
	else
		pr_info("dma_multisg: Buffer is scattered across %u segments\n",
			sgt->nents);

	/* Iterate all DMA-mapped SG entries (uses nents, not orig_nents) */
	for_each_sgtable_dma_sg(sgt, sg, i) {
		dma_addr_t addr = sg_dma_address(sg);
		unsigned int len = sg_dma_len(sg);

		pr_info("dma_multisg: SG[%d]: dma_addr=%pad, length=%u bytes\n",
			i, &addr, len);
		total_size += len;
	}

	pr_info("dma_multisg: Total mapped size: %zu bytes\n", total_size);
	pr_info("dma_multisg: === End SG Table ===\n");
}

/* --- IOCTL handler --- */

/**
 * my_dma_ioctl() - Import a DMA buffer and traverse its SG table entries
 * @file: File pointer (unused)
 * @cmd: IOCTL command (must be MY_DMA_IOCTL_MULTISG)
 * @arg: Pointer to userspace int holding the dma_buf file descriptor
 *
 * Calling context: Process context via ioctl() from userspace.
 *
 * Steps:
 *   1. dma_buf_get(fd) — import the buffer
 *   2. begin_cpu_access / vmap — read buffer content string
 *   3. attach / map_attachment — DMA mapping
 *   4. print_sg_entries() — iterate ALL SG entries (core of this example)
 *   5. Cleanup: unmap, detach, put
 *
 * Return: 0 on success, negative errno on failure.
 */
static long my_dma_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int user_fd;
	struct dma_buf *dmabuf;
	int ret = 0;
	struct dma_buf_attachment *attachment;
	struct sg_table *sgt;
	enum dma_data_direction direction = DMA_BIDIRECTIONAL;
	struct device *dev = &dummy_dma_pdev->dev;

	switch (cmd) {
	case MY_DMA_IOCTL_MULTISG:
		if (copy_from_user(&user_fd, (int __user *)arg, sizeof(user_fd)))
			return -EFAULT;
		pr_info("dma_multisg: Received DMA buf fd: %d\n", user_fd);

		/* Import the dma_buf */
		dmabuf = dma_buf_get(user_fd);
		if (IS_ERR(dmabuf)) {
			pr_err("dma_multisg: Failed to get dma_buf\n");
			return PTR_ERR(dmabuf);
		}
		pr_info("dma_multisg: Imported dma_buf at %p, size: %zu\n",
			dmabuf, dmabuf->size);

		/* CPU access: read buffer content */
		ret = dma_buf_begin_cpu_access(dmabuf, DMA_BIDIRECTIONAL);
		if (ret) {
			pr_err("dma_multisg: begin_cpu_access failed: %d\n", ret);
			dma_buf_put(dmabuf);
			return ret;
		}
		{
			struct iosys_map map;

			ret = dma_buf_vmap(dmabuf, &map);
			if (ret) {
				pr_err("dma_multisg: vmap failed: %d\n", ret);
				dma_buf_end_cpu_access(dmabuf, DMA_BIDIRECTIONAL);
				dma_buf_put(dmabuf);
				return ret;
			}
			pr_info("dma_multisg: Buffer content: %s\n",
				(char *)map.vaddr);
			dma_buf_vunmap(dmabuf, &map);
		}
		dma_buf_end_cpu_access(dmabuf, DMA_BIDIRECTIONAL);

		/* Attach to our device */
		attachment = dma_buf_attach(dmabuf, dev);
		if (IS_ERR(attachment)) {
			pr_err("dma_multisg: Failed to attach dma_buf\n");
			dma_buf_put(dmabuf);
			return PTR_ERR(attachment);
		}

		/* Map to get scatter-gather table */
		sgt = dma_buf_map_attachment(attachment, direction);
		if (IS_ERR(sgt)) {
			pr_err("dma_multisg: Failed to map attachment\n");
			dma_buf_detach(dmabuf, attachment);
			dma_buf_put(dmabuf);
			return PTR_ERR(sgt);
		}

		/* Core of this example: iterate ALL SG entries */
		print_sg_entries(sgt);

		/* Cleanup */
		dma_buf_unmap_attachment(attachment, sgt, direction);
		dma_buf_detach(dmabuf, attachment);
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
};

static struct miscdevice my_dma_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = "dummy_dma_multisg_device",
	.fops  = &my_dma_fops,
};

/* --- Platform device and module lifecycle --- */

/**
 * dummy_dma_multisg_init() - Register dummy platform device and misc device
 *
 * Return: 0 on success, negative errno on failure.
 */
static int __init dummy_dma_multisg_init(void)
{
	int ret;

	dummy_dma_pdev = platform_device_register_simple("dummy_dma_multisg",
							 -1, NULL, 0);
	if (IS_ERR(dummy_dma_pdev)) {
		pr_err("dma_multisg: Failed to register platform device\n");
		return PTR_ERR(dummy_dma_pdev);
	}
	pr_info("dma_multisg: Platform device registered\n");

	/*
	 * Use 64-bit DMA mask to bypass SWIOTLB bounce buffering.
	 * With a 32-bit mask, SWIOTLB must allocate contiguous bounce
	 * buffers for each SG segment — but its max single allocation
	 * is ~256KB, which fails for the system heap's large compound
	 * pages (order 8 = 1MB). A 64-bit mask lets the DMA mapping
	 * layer use physical addresses directly (no bouncing needed).
	 */
	ret = dma_set_mask_and_coherent(&dummy_dma_pdev->dev, DMA_BIT_MASK(64));
	if (ret) {
		pr_err("dma_multisg: Failed to set DMA mask\n");
		platform_device_unregister(dummy_dma_pdev);
		return ret;
	}

	ret = misc_register(&my_dma_misc_device);
	if (ret) {
		pr_err("dma_multisg: Failed to register misc device\n");
		platform_device_unregister(dummy_dma_pdev);
		return ret;
	}
	pr_info("dma_multisg: Device registered as /dev/%s\n",
		my_dma_misc_device.name);
	return 0;
}

/**
 * dummy_dma_multisg_exit() - Deregister misc device and platform device
 */
static void __exit dummy_dma_multisg_exit(void)
{
	misc_deregister(&my_dma_misc_device);
	platform_device_unregister(dummy_dma_pdev);
	pr_info("dma_multisg: Module unloaded\n");
}

module_init(dummy_dma_multisg_init);
module_exit(dummy_dma_multisg_exit);

/* --- Module metadata --- */

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("Driver demonstrating multi-entry SG table traversal with CMA/system heap");
MODULE_LICENSE("GPL");
MODULE_IMPORT_NS("DMA_BUF");
