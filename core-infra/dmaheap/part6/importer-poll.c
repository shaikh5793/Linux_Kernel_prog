/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 *
 * importer-poll.c - Poll-based completion using dma_fence (Part 6)
 *
 * This module demonstrates poll-based asynchronous completion using
 * dma_fence on DMA heap buffers. Instead of signals (SIGIO), it uses
 * poll() on the driver fd with a custom .poll implementation backed
 * by a dma_fence callback and wait queue.
 *
 * Key concepts:
 * - dma_fence lifecycle: alloc, init, signal, put
 * - dma_fence_add_callback() to get notified when fence signals
 * - Custom .poll fop with wait queue woken by fence callback
 * - Truly async processing via delayed_work (ioctl returns immediately)
 * - Userspace poll() on the driver fd (not the dma_buf fd)
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/dma-buf.h>
#include <linux/dma-fence.h>
#include <linux/dma-mapping.h>
#include <linux/iosys-map.h>
#include <linux/scatterlist.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/poll.h>

#define MY_DMA_IOCTL_POLL_SUBMIT _IOW('M', 9, int)

static struct platform_device *dummy_dma_pdev;
static DEFINE_SPINLOCK(poll_fence_lock);
static u64 poll_fence_context;
static u64 poll_fence_seqno;

/* Wait queue and flag for custom poll implementation */
static DECLARE_WAIT_QUEUE_HEAD(poll_wq);
static bool poll_ready;

/**
 * struct poll_work_ctx - Per-submission deferred work context
 * @dwork:  Delayed work struct scheduled from the ioctl handler
 * @fence:  dma_fence to signal when processing is complete
 * @dmabuf: dma_buf reference held for the duration of the work
 * @fcb:    Fence callback that wakes poll_wq when fence signals
 *
 * Allocated per ioctl call; freed after the work function completes.
 */
struct poll_work_ctx {
	struct delayed_work dwork;
	struct dma_fence *fence;
	struct dma_buf *dmabuf;
	struct dma_fence_cb fcb;
};

static struct poll_work_ctx *current_work_ctx;

/* --- dma_fence_ops --- */

static const char *poll_fence_get_driver_name(struct dma_fence *fence)
{
	return "dmaheap-poll-driver";
}

static const char *poll_fence_get_timeline_name(struct dma_fence *fence)
{
	return "poll-processing-timeline";
}

static const struct dma_fence_ops poll_fence_ops = {
	.get_driver_name = poll_fence_get_driver_name,
	.get_timeline_name = poll_fence_get_timeline_name,
};

/**
 * fence_signaled_cb() - dma_fence callback that wakes poll waiters
 * @fence: The fence that was signaled
 * @cb:    The callback struct registered with dma_fence_add_callback()
 *
 * Calling context: Called from dma_fence_signal(), which may be in any
 * context (process, softirq, etc.) depending on who signals the fence.
 * In this driver, it's called from workqueue context (process context).
 *
 * Sets poll_ready and wakes the poll wait queue so userspace poll()
 * returns EPOLLIN.
 */
static void fence_signaled_cb(struct dma_fence *fence, struct dma_fence_cb *cb)
{
	poll_ready = true;
	wake_up_interruptible(&poll_wq);
}

/**
 * poll_processing_work_fn() - Deferred work: process buffer then signal fence
 * @work: Pointer to the work_struct embedded in poll_work_ctx
 *
 * Calling context: Workqueue thread (process context, may sleep).
 *
 * Steps:
 *   1. begin_cpu_access / vmap — read and modify buffer
 *   2. Append " [processed]" suffix to buffer content
 *   3. dma_fence_signal() — triggers fence_signaled_cb → wakes poll_wq
 *   4. Release dma_buf and fence references, free context
 */
static void poll_processing_work_fn(struct work_struct *work)
{
	struct poll_work_ctx *ctx = container_of(work, struct poll_work_ctx,
						 dwork.work);
	struct iosys_map map;
	int ret;

	pr_info("dma_poll: Processing work started\n");

	/* Read and modify buffer to simulate processing */
	ret = dma_buf_begin_cpu_access(ctx->dmabuf, DMA_BIDIRECTIONAL);
	if (!ret) {
		ret = dma_buf_vmap(ctx->dmabuf, &map);
		if (!ret) {
			pr_info("dma_poll: Processing buffer: '%s'\n",
				(char *)map.vaddr);
			strlcat(map.vaddr, " [processed]", ctx->dmabuf->size);
			dma_buf_vunmap(ctx->dmabuf, &map);
		}
		dma_buf_end_cpu_access(ctx->dmabuf, DMA_BIDIRECTIONAL);
	}

	/*
	 * Signal the fence — this triggers fence_signaled_cb() which
	 * sets poll_ready=true and wakes poll_wq, causing userspace
	 * poll() on the driver fd to return EPOLLIN.
	 */
	dma_fence_signal(ctx->fence);
	pr_info("dma_poll: Fence signaled, poll() should return for userspace\n");

	/* Release references taken in the ioctl handler */
	dma_buf_put(ctx->dmabuf);
	dma_fence_put(ctx->fence);
	kfree(ctx);
	current_work_ctx = NULL;
}

/* --- Poll implementation --- */

/**
 * my_dma_poll() - Custom poll handler for the driver device fd
 * @file: File pointer
 * @wait: Poll table for registering the wait queue
 *
 * Calling context: Process context via poll()/epoll() from userspace.
 *
 * Registers the process on poll_wq. When fence_signaled_cb() fires,
 * it sets poll_ready and wakes poll_wq, causing this function to
 * return EPOLLIN on the next check.
 *
 * Return: EPOLLIN if processing is complete, 0 otherwise.
 */
static __poll_t my_dma_poll(struct file *file, poll_table *wait)
{
	poll_wait(file, &poll_wq, wait);

	if (poll_ready)
		return EPOLLIN;

	return 0;
}

/* --- IOCTL handler --- */

/**
 * my_dma_ioctl() - Submit buffer for async processing with fence-based completion
 * @file: File pointer (unused)
 * @cmd: IOCTL command (must be MY_DMA_IOCTL_POLL_SUBMIT)
 * @arg: Pointer to userspace int holding the dma_buf file descriptor
 *
 * Calling context: Process context via ioctl() from userspace.
 *
 * Steps:
 *   1. dma_buf_get(fd) — import the buffer
 *   2. Allocate and initialize a dma_fence
 *   3. Register fence callback (fence_signaled_cb) to wake poll_wq
 *   4. Allocate work context, take extra refs on dmabuf and fence
 *   5. schedule_delayed_work() — processing happens ~1s later
 *   6. Drop ioctl's own references (work holds its own)
 *   7. Return immediately — userspace uses poll() on driver fd to wait
 *
 * Return: 0 on success, negative errno on failure.
 */
static long my_dma_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int user_fd;
	struct dma_buf *dmabuf;
	struct dma_fence *fence;
	struct poll_work_ctx *ctx;
	int ret;

	switch (cmd) {
	case MY_DMA_IOCTL_POLL_SUBMIT:
		if (copy_from_user(&user_fd, (int __user *)arg, sizeof(user_fd)))
			return -EFAULT;
		pr_info("dma_poll: Received DMA buf fd: %d\n", user_fd);

		/* Import dma_buf */
		dmabuf = dma_buf_get(user_fd);
		if (IS_ERR(dmabuf)) {
			pr_err("dma_poll: Failed to get dma_buf\n");
			return PTR_ERR(dmabuf);
		}
		pr_info("dma_poll: Imported dma_buf, size: %zu\n", dmabuf->size);

		/* Allocate and initialize fence */
		fence = kzalloc(sizeof(*fence), GFP_KERNEL);
		if (!fence) {
			dma_buf_put(dmabuf);
			return -ENOMEM;
		}
		dma_fence_init(fence, &poll_fence_ops, &poll_fence_lock,
			       poll_fence_context, ++poll_fence_seqno);
		pr_info("dma_poll: Created fence (context=%llu, seqno=%llu)\n",
			poll_fence_context, poll_fence_seqno);

		/* Allocate work context and take extra references for the work fn */
		ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
		if (!ctx) {
			ret = -ENOMEM;
			goto err_fence;
		}

		/*
		 * Reset poll state and register fence callback.
		 * When dma_fence_signal() is called (from the work fn),
		 * fence_signaled_cb() fires, sets poll_ready=true, and
		 * wakes poll_wq so that poll() on the driver fd returns.
		 */
		poll_ready = false;
		ret = dma_fence_add_callback(fence, &ctx->fcb, fence_signaled_cb);
		if (ret == -ENOENT) {
			/* Fence already signaled (shouldn't happen for a new fence) */
			poll_ready = true;
		} else if (ret) {
			pr_err("dma_poll: dma_fence_add_callback failed: %d\n", ret);
			kfree(ctx);
			goto err_fence;
		}
		pr_info("dma_poll: Fence callback registered for poll wakeup\n");

		get_dma_buf(dmabuf);    /* work fn will dma_buf_put() */
		dma_fence_get(fence);   /* work fn will dma_fence_put() */
		ctx->fence = fence;
		ctx->dmabuf = dmabuf;
		current_work_ctx = ctx;
		INIT_DELAYED_WORK(&ctx->dwork, poll_processing_work_fn);

		/* Schedule: signal fence after ~1 second */
		schedule_delayed_work(&ctx->dwork, msecs_to_jiffies(1000));
		pr_info("dma_poll: Scheduled processing, fence signals in ~1s\n");

		/* Drop ioctl's own references — work context holds its own */
		dma_fence_put(fence);
		dma_buf_put(dmabuf);
		break;

	default:
		return -EINVAL;
	}
	return 0;

err_fence:
	dma_fence_put(fence);
	dma_buf_put(dmabuf);
	return ret;
}

/* --- File operations --- */

static const struct file_operations my_dma_fops = {
	.owner          = THIS_MODULE,
	.unlocked_ioctl = my_dma_ioctl,
	.poll           = my_dma_poll,
};

static struct miscdevice my_dma_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = "dummy_dma_poll_device",
	.fops  = &my_dma_fops,
};

/* --- Platform device and module lifecycle --- */

/**
 * dummy_dma_poll_init() - Allocate fence context, register platform + misc device
 *
 * Steps:
 *   1. dma_fence_context_alloc(1) — unique fence context for this driver
 *   2. Register dummy platform device with DMA mask
 *   3. Register misc device for ioctl + poll interface
 *
 * Return: 0 on success, negative errno on failure.
 */
static int __init dummy_dma_poll_init(void)
{
	int ret;

	poll_fence_context = dma_fence_context_alloc(1);

	dummy_dma_pdev = platform_device_register_simple("dummy_dma_poll",
							 -1, NULL, 0);
	if (IS_ERR(dummy_dma_pdev)) {
		pr_err("dma_poll: Failed to register platform device\n");
		return PTR_ERR(dummy_dma_pdev);
	}
	pr_info("dma_poll: Platform device registered\n");

	ret = dma_set_mask_and_coherent(&dummy_dma_pdev->dev, DMA_BIT_MASK(32));
	if (ret) {
		pr_err("dma_poll: Failed to set DMA mask\n");
		platform_device_unregister(dummy_dma_pdev);
		return ret;
	}

	ret = misc_register(&my_dma_misc_device);
	if (ret) {
		pr_err("dma_poll: Failed to register misc device\n");
		platform_device_unregister(dummy_dma_pdev);
		return ret;
	}
	pr_info("dma_poll: Device registered as /dev/%s\n",
		my_dma_misc_device.name);
	return 0;
}

/**
 * dummy_dma_poll_exit() - Cancel pending work and deregister devices
 *
 * If delayed work is still pending, cancel_delayed_work_sync() cancels it
 * (returns true) and we clean up the resources that the work fn would have
 * released. If the work already ran, current_work_ctx is NULL and we skip.
 */
static void __exit dummy_dma_poll_exit(void)
{
	if (current_work_ctx) {
		if (cancel_delayed_work_sync(&current_work_ctx->dwork)) {
			/*
			 * Work was cancelled before it ran — the work fn
			 * never released these, so we must do it here.
			 */
			dma_fence_signal(current_work_ctx->fence);
			dma_buf_put(current_work_ctx->dmabuf);
			dma_fence_put(current_work_ctx->fence);
			kfree(current_work_ctx);
			current_work_ctx = NULL;
		}
		/* If cancel returned false, work already ran and cleaned up */
	}

	misc_deregister(&my_dma_misc_device);
	platform_device_unregister(dummy_dma_pdev);
	pr_info("dma_poll: Module unloaded\n");
}

module_init(dummy_dma_poll_init);
module_exit(dummy_dma_poll_exit);

/* --- Module metadata --- */

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("Driver demonstrating poll-based completion using dma_fence");
MODULE_LICENSE("GPL");
MODULE_IMPORT_NS("DMA_BUF");
