/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 *
 * importer-sync.c - Synchronized CPU and DMA access to heap buffers (Part 3)
 *
 * This module demonstrates proper synchronization between CPU and DMA
 * access to a DMA heap buffer. It uses begin_cpu_access/end_cpu_access
 * for cache coherency, reads buffer contents via dma_buf_vmap(), and
 * then simulates programming a DMA transfer.
 *
 * Key concepts:
 * - dma_buf_begin_cpu_access() / dma_buf_end_cpu_access() for cache coherency
 * - dma_buf_vmap() with struct iosys_map for kernel virtual mapping
 * - Two-phase access: CPU read then DMA mapping
 * - Simulated DMA transfer programming
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

#define MY_DMA_IOCTL_TRANSFER _IOW('M', 5, int)

/* Global pointer to our dummy platform device */
static struct platform_device *dummy_dma_pdev;

/**
 * program_dma_transfer() - Simulate programming a DMA engine transfer
 * @dma_addr: The DMA address of the source buffer
 * @size:     The size of the DMA buffer in bytes
 *
 * In a real driver, this function would program your DMA engine by writing
 * to device registers (source/destination addresses, transfer size, control
 * parameters) and then starting the transfer. Here we simply print the
 * DMA address and size.
 *
 * Return: 0 (simulated success).
 */
static int program_dma_transfer(dma_addr_t dma_addr, size_t size)
{
	pr_info("program_dma_transfer: Programming DMA transfer from address %pad, size %zu\n",
			&dma_addr, size);
	/*
	 * Fictitious code (for illustration only):
	 *
	 *   // Write the source address into the DMA controller register
	 *   write_register(DMA_SRC_ADDR_REG, dma_addr);
	 *   // Write the destination address (assumed pre-determined)
	 *   write_register(DMA_DST_ADDR_REG, destination_address);
	 *   // Write the transfer size
	 *   write_register(DMA_TRANSFER_SIZE_REG, size);
	 *   // Start the DMA transfer with appropriate control flags
	 *   write_register(DMA_CONTROL_REG, DMA_START | other_flags);
	 *
	 *   // Optionally, wait for completion or set up an interrupt handler.
	 */
	return 0; /* Simulate a successful programming */
}

/* --- IOCTL handler --- */

/**
 * my_dma_ioctl() - Import a DMA buffer, read via CPU, then map for DMA transfer
 * @file: File pointer (unused)
 * @cmd: IOCTL command (must be MY_DMA_IOCTL_TRANSFER)
 * @arg: Pointer to userspace int holding the dma_buf file descriptor
 *
 * Calling context: Process context via ioctl() from userspace.
 *
 * Steps:
 *   Phase 1 — CPU access (cache-coherent read):
 *     1. dma_buf_get(fd)
 *     2. dma_buf_begin_cpu_access()  -- flush/invalidate caches
 *     3. dma_buf_vmap()              -- map into kernel virtual address space
 *     4. Read buffer contents
 *     5. dma_buf_vunmap() / dma_buf_end_cpu_access()
 *
 *   Phase 2 — DMA mapping:
 *     6. dma_buf_attach() / dma_buf_map_attachment()
 *     7. sg_dma_address() -- retrieve DMA address
 *     8. program_dma_transfer() -- simulate hardware programming
 *     9. Cleanup: unmap, detach, put
 *
 * Return: 0 on success, negative errno on failure.
 */
static long my_dma_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int user_fd;
	struct dma_buf *dmabuf;
	int ret;
	struct dma_buf_attachment *attachment;
	struct sg_table *sgt;
	enum dma_data_direction direction = DMA_BIDIRECTIONAL;
	dma_addr_t dma_addr;
	size_t size;
	struct device *dev = &dummy_dma_pdev->dev;

	switch (cmd) {
	case MY_DMA_IOCTL_TRANSFER:
		if (copy_from_user(&user_fd, (int __user *)arg, sizeof(user_fd)))
			return -EFAULT;
		pr_info("dma_transfer: Received DMA buf fd: %d\n", user_fd);

		/* Import the dma_buf from the provided fd */
		dmabuf = dma_buf_get(user_fd);
		if (IS_ERR(dmabuf)) {
			pr_err("dma_transfer: Failed to get dma_buf\n");
			return PTR_ERR(dmabuf);
		}
		pr_info("dma_transfer: Imported dma_buf at %p, size: %zu\n", dmabuf, dmabuf->size);
		size = dmabuf->size;

		/* Phase 1: Synchronize and perform CPU access to read buffer contents */
		ret = dma_buf_begin_cpu_access(dmabuf, DMA_BIDIRECTIONAL);
		if (ret) {
			pr_err("dma_transfer: dma_buf_begin_cpu_access failed: %d\n", ret);
			dma_buf_put(dmabuf);
			return ret;
		}

		{
			struct iosys_map map;
			ret = dma_buf_vmap(dmabuf, &map);
			if (ret) {
				pr_err("dma_transfer: dma_buf_vmap failed: %d\n", ret);
				dma_buf_end_cpu_access(dmabuf, DMA_BIDIRECTIONAL);
				dma_buf_put(dmabuf);
				return ret;
			}
			pr_info("dma_transfer: Buffer content (CPU access): %s\n",
					(char *)map.vaddr);
			dma_buf_vunmap(dmabuf, &map);
		}

		ret = dma_buf_end_cpu_access(dmabuf, DMA_BIDIRECTIONAL);
		if (ret) {
			pr_err("dma_transfer: dma_buf_end_cpu_access failed: %d\n", ret);
			dma_buf_put(dmabuf);
			return ret;
		}

		/* Phase 2: Attach the dma_buf to our dummy device for DMA mapping */
		attachment = dma_buf_attach(dmabuf, dev);
		if (IS_ERR(attachment)) {
			pr_err("dma_transfer: Failed to attach dma_buf\n");
			dma_buf_put(dmabuf);
			return PTR_ERR(attachment);
		}

		/* Map the attachment for DMA access */
		sgt = dma_buf_map_attachment(attachment, direction);
		if (IS_ERR(sgt)) {
			pr_err("dma_transfer: Failed to map attachment\n");
			dma_buf_detach(dmabuf, attachment);
			dma_buf_put(dmabuf);
			return PTR_ERR(sgt);
		}

		if (sgt->sgl) {
			dma_addr = sg_dma_address(sgt->sgl);
			pr_info("dma_transfer: Retrieved DMA address: %pad\n", &dma_addr);
		} else {
			pr_err("dma_transfer: sg_table is empty\n");
			ret = -EINVAL;
			goto unmap_detach;
		}

		/* Program the simulated DMA transfer */
		ret = program_dma_transfer(dma_addr, size);
		if (ret)
			pr_err("dma_transfer: DMA transfer programming failed: %d\n", ret);
		else
			pr_info("dma_transfer: DMA transfer programmed successfully.\n");

unmap_detach:
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
	.name  = "dummy_dma_transfer_device",
	.fops  = &my_dma_fops,
};

/* --- Platform device and module lifecycle --- */

/**
 * dummy_dma_init() - Register dummy platform device and misc device
 *
 * Return: 0 on success, negative errno on failure.
 */
static int __init dummy_dma_init(void)
{
	int ret;

	/* Register a dummy platform device */
	dummy_dma_pdev = platform_device_register_simple("dummy_dma_transfer", -1, NULL, 0);
	if (IS_ERR(dummy_dma_pdev)) {
		pr_err("dma_transfer: Failed to register dummy platform device\n");
		return PTR_ERR(dummy_dma_pdev);
	}
	pr_info("dma_transfer: Dummy platform device registered\n");

	/* Set a DMA mask for the dummy device (adjust if 64-bit DMA is needed) */
	ret = dma_set_mask_and_coherent(&dummy_dma_pdev->dev, DMA_BIT_MASK(32));
	if (ret) {
		pr_err("dma_transfer: Failed to set DMA mask\n");
		platform_device_unregister(dummy_dma_pdev);
		return ret;
	}

	ret = misc_register(&my_dma_misc_device);
	if (ret) {
		pr_err("dma_transfer: Failed to register misc device\n");
		platform_device_unregister(dummy_dma_pdev);
		return ret;
	}
	pr_info("dma_transfer: Device registered as /dev/%s\n", my_dma_misc_device.name);
	return 0;
}

/**
 * dummy_dma_exit() - Deregister misc device and platform device
 */
static void __exit dummy_dma_exit(void)
{
	misc_deregister(&my_dma_misc_device);
	platform_device_unregister(dummy_dma_pdev);
	pr_info("dma_transfer: Module unloaded\n");
}

module_init(dummy_dma_init);
module_exit(dummy_dma_exit);

/* --- Module metadata --- */

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("Driver demonstrating DMA transfer programming with dma_buf");
MODULE_LICENSE("GPL");
MODULE_IMPORT_NS("DMA_BUF");
