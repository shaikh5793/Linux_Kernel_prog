/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 *
 * DMA-BUF Exporter with Fence Synchronization (Part 6)
 *
 * Builds on Parts 1-5: exports a DMA-BUF with DMA-capable sg_table,
 * attach/detach, begin/end_cpu_access, mmap, and FD sharing via IOCTL.
 *
 * NEW in Part 6: attaches a dma_fence to the buffer's reservation
 * object (dma_buf->resv).  The fence is signaled from a delayed_work
 * after 1 second, simulating hardware completion.  The importer must
 * wait for the fence before accessing the buffer.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/dma-buf.h>
#include <linux/dma-fence.h>
#include <linux/dma-resv.h>
#include <linux/iosys-map.h>
#include <linux/workqueue.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/mm.h>
#include <linux/pfn.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>

#define EXPORTER_FENCE_GET_DMABUF_FD _IOR('E', 3, int)

static struct dma_buf *dmabuf_fence_exported;
static struct dma_fence *export_fence;
static struct delayed_work signal_work;

static DEFINE_SPINLOCK(fence_lock);
static u64 fence_context;
static u64 fence_seqno;

/* --- dma_fence_ops --- */

static const char *fence_get_driver_name(struct dma_fence *fence)
{
	return "dmabuf-fence-exporter";
}

static const char *fence_get_timeline_name(struct dma_fence *fence)
{
	return "exporter-timeline";
}

static const struct dma_fence_ops export_fence_ops = {
	.get_driver_name = fence_get_driver_name,
	.get_timeline_name = fence_get_timeline_name,
};

/*
 * fence_signal_work_fn() - Workqueue function to signal the DMA fence.
 *
 * Calling Context:
 *   Executed as a delayed workqueue item, scheduled when userspace
 *   requests the fd via IOCTL (simulates hardware starting work when
 *   the buffer is shared).
 *
 * Call Chain:
 *   exporter_ioctl() -> schedule_delayed_work() -> ... -> fence_signal_work_fn()
 *
 * Steps:
 *   1. Call dma_fence_signal() to unblock any waiters.
 *   2. In real drivers this would be triggered by a hardware interrupt.
 */
static void fence_signal_work_fn(struct work_struct *work)
{
	pr_info("fence_exporter: Simulated transfer complete, signaling fence\n");
	if (export_fence) {
		dma_fence_signal(export_fence);
		pr_info("fence_exporter: Fence signaled (seqno=%llu)\n",
			export_fence->seqno);
	}
}

/* --- dma_buf_ops --- */

static int exporter_attach(struct dma_buf *dmabuf,
			   struct dma_buf_attachment *attachment)
{
	pr_info("fence_exporter: Device %s attached\n",
		dev_name(attachment->dev));
	return 0;
}

static void exporter_detach(struct dma_buf *dmabuf,
			    struct dma_buf_attachment *attachment)
{
	pr_info("fence_exporter: Device %s detached\n",
		dev_name(attachment->dev));
}

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

	pr_info("fence_exporter: Mapped buffer for DMA at %pad\n", &dma_addr);
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

static void exporter_release(struct dma_buf *dmabuf)
{
	kfree(dmabuf->priv);
	dmabuf_fence_exported = NULL;
	pr_info("fence_exporter: DMA-BUF released\n");
}

static int exporter_begin_cpu_access(struct dma_buf *dmabuf,
				     enum dma_data_direction dir)
{
	pr_info("fence_exporter: begin_cpu_access (direction: %d)\n", dir);
	return 0;
}

static int exporter_end_cpu_access(struct dma_buf *dmabuf,
				   enum dma_data_direction dir)
{
	pr_info("fence_exporter: end_cpu_access (direction: %d)\n", dir);
	return 0;
}

static int exporter_vmap(struct dma_buf *dmabuf, struct iosys_map *map)
{
	iosys_map_set_vaddr(map, dmabuf->priv);
	return 0;
}

static void exporter_vunmap(struct dma_buf *dmabuf, struct iosys_map *map)
{
	iosys_map_clear(map);
}

static int exporter_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{
	void *vaddr = dmabuf->priv;
	unsigned long pfn = __pa(vaddr) >> PAGE_SHIFT;

	return remap_pfn_range(vma, vma->vm_start, pfn, PAGE_SIZE,
			       vma->vm_page_prot);
}

static const struct dma_buf_ops exp_dmabuf_ops = {
	.attach = exporter_attach,
	.detach = exporter_detach,
	.map_dma_buf = exporter_map_dma_buf,
	.unmap_dma_buf = exporter_unmap_dma_buf,
	.release = exporter_release,
	.begin_cpu_access = exporter_begin_cpu_access,
	.end_cpu_access = exporter_end_cpu_access,
	.vmap = exporter_vmap,
	.vunmap = exporter_vunmap,
	.mmap = exporter_mmap,
};

/**
 * exporter_ioctl() - Deliver DMA-BUF fd and start simulated hardware work
 *
 * Calling Context:
 *   Called when userspace performs ioctl() on /dev/exporter-fence.
 *
 * Call Chain:
 *   userspace ioctl() -> vfs_ioctl() -> exporter_ioctl()
 *
 * Steps:
 *   1. Create a file descriptor for the dma_buf using dma_buf_fd().
 *   2. Copy the fd back to userspace.
 *   3. Schedule the delayed work that will signal the fence after 1 second,
 *      simulating hardware starting work when the buffer is shared.
 */
static long exporter_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int fd;

	if (cmd != EXPORTER_FENCE_GET_DMABUF_FD) {
		pr_err("fence_exporter: Invalid IOCTL command: %u\n", cmd);
		return -EINVAL;
	}

	if (!dmabuf_fence_exported) {
		pr_err("fence_exporter: DMA-BUF not exported yet\n");
		return -ENOENT;
	}

	fd = dma_buf_fd(dmabuf_fence_exported, O_CLOEXEC);
	if (fd < 0) {
		pr_err("fence_exporter: dma_buf_fd failed: %d\n", fd);
		return fd;
	}

	if (copy_to_user((int __user *)arg, &fd, sizeof(fd))) {
		pr_err("fence_exporter: Failed to copy fd to userspace\n");
		return -EFAULT;
	}

	/* Start simulated hardware work — fence signals in ~1 second */
	schedule_delayed_work(&signal_work, msecs_to_jiffies(1000));
	pr_info("fence_exporter: Delivered fd %d, hardware work started (fence in ~1s)\n", fd);

	return 0;
}

static const struct file_operations exporter_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = exporter_ioctl,
};

static struct miscdevice mdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "exporter-fence",
	.fops = &exporter_fops,
};

static int __init exporter_fence_init(void)
{
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	void *kbuf;
	int ret;

	/* Allocate and export buffer */
	kbuf = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;

	strscpy(kbuf, "hello from fence exporter!", PAGE_SIZE);

	exp_info.ops = &exp_dmabuf_ops;
	exp_info.size = PAGE_SIZE;
	exp_info.flags = O_CLOEXEC;
	exp_info.priv = kbuf;

	dmabuf_fence_exported = dma_buf_export(&exp_info);
	if (IS_ERR(dmabuf_fence_exported)) {
		ret = PTR_ERR(dmabuf_fence_exported);
		dmabuf_fence_exported = NULL;
		kfree(kbuf);
		pr_err("fence_exporter: dma_buf_export failed: %d\n", ret);
		return ret;
	}

	/* Create fence context and fence */
	fence_context = dma_fence_context_alloc(1);

	export_fence = kzalloc(sizeof(*export_fence), GFP_KERNEL);
	if (!export_fence) {
		ret = -ENOMEM;
		goto err_put_dmabuf;
	}

	dma_fence_init(export_fence, &export_fence_ops, &fence_lock,
		       fence_context, ++fence_seqno);
	pr_info("fence_exporter: Fence created (context=%llu, seqno=%llu)\n",
		fence_context, fence_seqno);

	/* Attach fence to dma_buf's reservation object */
	ret = dma_resv_lock(dmabuf_fence_exported->resv, NULL);
	if (ret) {
		pr_err("fence_exporter: dma_resv_lock failed: %d\n", ret);
		goto err_put_fence;
	}

	ret = dma_resv_reserve_fences(dmabuf_fence_exported->resv, 1);
	if (ret) {
		dma_resv_unlock(dmabuf_fence_exported->resv);
		pr_err("fence_exporter: dma_resv_reserve_fences failed: %d\n", ret);
		goto err_put_fence;
	}

	dma_resv_add_fence(dmabuf_fence_exported->resv, export_fence,
			   DMA_RESV_USAGE_WRITE);
	dma_resv_unlock(dmabuf_fence_exported->resv);
	pr_info("fence_exporter: Fence attached to dma_buf reservation object\n");

	/* Initialize delayed work (scheduled when IOCTL delivers fd) */
	INIT_DELAYED_WORK(&signal_work, fence_signal_work_fn);

	/* Register misc device */
	ret = misc_register(&mdev);
	if (ret) {
		pr_err("fence_exporter: misc_register failed: %d\n", ret);
		goto err_put_fence;
	}

	pr_info("fence_exporter: Initialized — /dev/exporter-fence ready\n");
	return 0;

err_put_fence:
	dma_fence_put(export_fence);
	export_fence = NULL;
err_put_dmabuf:
	dma_buf_put(dmabuf_fence_exported);
	dmabuf_fence_exported = NULL;
	return ret;
}

static void __exit exporter_fence_exit(void)
{
	misc_deregister(&mdev);
	cancel_delayed_work_sync(&signal_work);

	if (export_fence) {
		if (!dma_fence_is_signaled(export_fence))
			dma_fence_signal(export_fence);
		dma_fence_put(export_fence);
		export_fence = NULL;
	}

	if (dmabuf_fence_exported) {
		dma_buf_put(dmabuf_fence_exported);
		dmabuf_fence_exported = NULL;
	}

	pr_info("fence_exporter: Module unloaded\n");
}

module_init(exporter_fence_init);
module_exit(exporter_fence_exit);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("DMA-BUF Fence Exporter — full pipeline with dma_fence/dma_resv");
MODULE_IMPORT_NS("DMA_BUF");
