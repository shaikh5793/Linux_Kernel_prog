/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 *
 * importer-async.c - Async DMA processing with SIGIO notification (Part 5)
 *
 * This module demonstrates an end-to-end asynchronous DMA buffer processing
 * pipeline. The application writes frame data into a shared buffer and
 * submits it to the driver, which processes it and notifies the application
 * via SIGIO when complete.
 *
 * Key concepts:
 * - fasync / kill_fasync / SIGIO for asynchronous notification
 * - Multi-frame processing loop (5 frames per test run)
 * - Simulated DMA transfer with msleep() delay
 * - Combined CPU access (vmap read) and DMA mapping in one ioctl
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
#include <linux/delay.h>
#include <linux/signal.h>
#include <linux/poll.h>

#define MY_DMA_IOCTL_PROCESS_ASYNC _IOW('M', 7, int)


/* Forward declaration for the fasync callback */
static int my_dma_fasync(int fd, struct file *filp, int on);
/* Global dummy platform device pointer */
static struct platform_device *dummy_dma_pdev;
/* Global pointer for asynchronous notification */
static struct fasync_struct *async_queue;

/**
 * simulate_dma_transfer() - Simulate programming a DMA transfer
 * @dma_addr: The DMA address of the buffer
 * @size:     The size of the DMA buffer in bytes
 *
 * In a real driver, you would program your DMA engine by writing to
 * device registers. This function prints the DMA address and size,
 * then simulates a transfer delay with msleep(100).
 *
 * Calling context: Process context (called from ioctl handler).
 *
 * Return: 0 (simulated success).
 */
static int simulate_dma_transfer(dma_addr_t dma_addr, size_t size)
{
	pr_info("simulate_dma_transfer: Programming DMA transfer:\n");
	pr_info("  DMA Source Address: %pad\n", &dma_addr);
	pr_info("  Transfer Size: %zu bytes\n", size);

	/*
	 * Real hardware code might look like:
	 *
	 *   writel(dma_addr, DMA_SRC_ADDR_REG);
	 *   writel(dest_addr, DMA_DST_ADDR_REG);
	 *   writel(size, DMA_TRANSFER_SIZE_REG);
	 *   writel(DMA_START | control_flags, DMA_CONTROL_REG);
	 *
	 *   // Optionally, wait for transfer completion or set an interrupt.
	 */

	msleep(100);  /* Simulate transfer delay */
	pr_info("simulate_dma_transfer: DMA transfer simulated successfully.\n");
	return 0;
}

/* --- IOCTL handler --- */

/**
 * my_dma_ioctl() - Import buffer, read content, simulate DMA, signal userspace
 * @file: File pointer (unused)
 * @cmd: IOCTL command (must be MY_DMA_IOCTL_PROCESS_ASYNC)
 * @arg: Pointer to userspace int holding the dma_buf file descriptor
 *
 * Calling context: Process context via ioctl() from userspace.
 *
 * Steps:
 *   1. dma_buf_get(fd) — import the buffer
 *   2. begin_cpu_access — cache sync
 *   3. attach / map_attachment — DMA mapping
 *   4. vmap — read buffer content string
 *   5. simulate_dma_transfer() — 100ms simulated transfer
 *   6. Cleanup: unmap, detach, end_cpu_access, put
 *   7. kill_fasync(SIGIO) — notify userspace of completion
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
	struct iosys_map map;
	struct device *dev = &dummy_dma_pdev->dev;

	switch (cmd) {
	case MY_DMA_IOCTL_PROCESS_ASYNC:
		if (copy_from_user(&user_fd, (int __user *)arg, sizeof(user_fd)))
			return -EFAULT;
		pr_info("dma_async: Received DMA buf fd: %d\n", user_fd);

		dmabuf = dma_buf_get(user_fd);
		if (IS_ERR(dmabuf)) {
			pr_err("dma_async: Failed to get dma_buf\n");
			return PTR_ERR(dmabuf);
		}
		pr_info("dma_async: Imported dma_buf at %p, size: %zu\n", dmabuf, dmabuf->size);
		size = dmabuf->size;

		/* Begin CPU access — flushes caches for coherent read */
		ret = dma_buf_begin_cpu_access(dmabuf, DMA_BIDIRECTIONAL);
		if (ret) {
			pr_err("dma_async: dma_buf_begin_cpu_access failed: %d\n", ret);
			dma_buf_put(dmabuf);
			return ret;
		}

		/* Attach and map for DMA address retrieval */
		attachment = dma_buf_attach(dmabuf, dev);
		if (IS_ERR(attachment)) {
			pr_err("dma_async: Failed to attach dma_buf\n");
			dma_buf_end_cpu_access(dmabuf, DMA_BIDIRECTIONAL);
			dma_buf_put(dmabuf);
			return PTR_ERR(attachment);
		}

		sgt = dma_buf_map_attachment(attachment, direction);
		if (IS_ERR(sgt)) {
			pr_err("dma_async: Failed to map attachment\n");
			dma_buf_detach(dmabuf, attachment);
			dma_buf_end_cpu_access(dmabuf, DMA_BIDIRECTIONAL);
			dma_buf_put(dmabuf);
			return PTR_ERR(sgt);
		}

		if (sgt->sgl) {
			dma_addr = sg_dma_address(sgt->sgl);
			pr_info("dma_async: Retrieved DMA address: %pad\n", &dma_addr);
		} else {
			pr_err("dma_async: sg_table is empty\n");
			ret = -EINVAL;
			goto unmap_detach;
		}

		/* Read the buffer content via vmap */
		ret = dma_buf_vmap(dmabuf, &map);
		if (ret) {
			pr_err("dma_async: dma_buf_vmap failed: %d\n", ret);
			goto unmap_detach;
		}
		pr_info("dma_async: Buffer content: %s\n", (char *)map.vaddr);
		dma_buf_vunmap(dmabuf, &map);

		/* Simulate a DMA transfer using the retrieved DMA address */
		ret = simulate_dma_transfer(dma_addr, size);
		if (ret)
			pr_err("dma_async: simulate_dma_transfer failed: %d\n", ret);

unmap_detach:
		dma_buf_unmap_attachment(attachment, sgt, direction);
		dma_buf_detach(dmabuf, attachment);
		ret = dma_buf_end_cpu_access(dmabuf, DMA_BIDIRECTIONAL);
		if (ret)
			pr_err("dma_async: dma_buf_end_cpu_access failed: %d\n", ret);
		dma_buf_put(dmabuf);

		/* Signal the application that processing is complete */
		kill_fasync(&async_queue, SIGIO, POLL_IN);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

/* --- File operations --- */

static int my_dma_open(struct inode *inode, struct file *file)
{
	return 0;
}

/**
 * my_dma_release() - Deregister async notification on file close
 *
 * Ensures the fasync queue is cleaned up when userspace closes the fd.
 */
static int my_dma_release(struct inode *inode, struct file *file)
{
	my_dma_fasync(-1, file, 0);
	return 0;
}

/**
 * my_dma_fasync() - Register/deregister for SIGIO async notification
 * @fd:   File descriptor
 * @filp: File pointer
 * @on:   Non-zero to register, zero to deregister
 *
 * Called by the kernel when userspace sets O_ASYNC via fcntl(F_SETFL).
 *
 * Return: 0 on success, negative errno on failure.
 */
static int my_dma_fasync(int fd, struct file *filp, int on)
{
	return fasync_helper(fd, filp, on, &async_queue);
}

static const struct file_operations my_dma_fops = {
	.owner          = THIS_MODULE,
	.open           = my_dma_open,
	.release        = my_dma_release,
	.unlocked_ioctl = my_dma_ioctl,
	.fasync         = my_dma_fasync,
};

static struct miscdevice my_dma_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = "dummy_dma_async_device",
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
	dummy_dma_pdev = platform_device_register_simple("dummy_dma_async", -1, NULL, 0);
	if (IS_ERR(dummy_dma_pdev)) {
		pr_err("dma_async: Failed to register dummy platform device\n");
		return PTR_ERR(dummy_dma_pdev);
	}
	pr_info("dma_async: Dummy platform device registered\n");

	ret = dma_set_mask_and_coherent(&dummy_dma_pdev->dev, DMA_BIT_MASK(32));
	if (ret) {
		pr_err("dma_async: Failed to set DMA mask\n");
		platform_device_unregister(dummy_dma_pdev);
		return ret;
	}

	ret = misc_register(&my_dma_misc_device);
	if (ret) {
		pr_err("dma_async: Failed to register misc device\n");
		platform_device_unregister(dummy_dma_pdev);
		return ret;
	}
	pr_info("dma_async: Device registered as /dev/%s\n", my_dma_misc_device.name);
	return 0;
}

/**
 * dummy_dma_exit() - Deregister misc device, platform device, and async queue
 */
static void __exit dummy_dma_exit(void)
{
	misc_deregister(&my_dma_misc_device);
	platform_device_unregister(dummy_dma_pdev);
	kill_fasync(&async_queue, SIGIO, 0);
	pr_info("dma_async: Module unloaded\n");
}

module_init(dummy_dma_init);
module_exit(dummy_dma_exit);

/* --- Module metadata --- */

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("Dummy DMA driver with async processing, simulated DMA transfer, and SIGIO notification");
MODULE_LICENSE("GPL");
MODULE_IMPORT_NS("DMA_BUF");
