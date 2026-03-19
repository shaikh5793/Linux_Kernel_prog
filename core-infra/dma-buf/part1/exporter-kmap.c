/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#include <linux/dma-buf.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/iosys-map.h>


struct dma_buf *dmabuf_exported;
EXPORT_SYMBOL_GPL(dmabuf_exported);

/*
 * DMA-BUF Operations Implementation
 * 
 * These functions implement the dma_buf_ops interface. For this basic
 * example, we only support CPU access (vmap) and don't implement
 * actual DMA operations.
 */

/**
 * exporter_map_dma_buf() - Map buffer for DMA access
 * @attachment: DMA-BUF attachment
 * @dir: DMA data direction
 *
 * This would normally return a scatter-gather table for DMA access.
 * Since this is a CPU-only example, we return NULL to indicate no
 * DMA support.
 *
 * Return: NULL (no DMA support in this example)
 */
static struct sg_table *exporter_map_dma_buf(struct dma_buf_attachment *attachment,
					     enum dma_data_direction dir)
{
	pr_debug("DMA mapping requested but not supported in this example\n");
	return NULL;
}

/**
 * exporter_unmap_dma_buf() - Unmap DMA buffer
 * @attachment: DMA-BUF attachment
 * @table: Scatter-gather table to unmap
 * @dir: DMA data direction
 *
 * Cleanup function for DMA mapping. Since map_dma_buf returns NULL,
 * this is effectively a no-op.
 */
static void exporter_unmap_dma_buf(struct dma_buf_attachment *attachment,
				   struct sg_table *table,
				   enum dma_data_direction dir)
{
	/* No-op: no DMA mapping to clean up */
}

/**
 * exporter_release() - Release DMA-BUF resources
 *
 * Calling Context:
 *   This function is a callback in the `dma_buf_ops` structure. It is called
 *   by the DMA-BUF framework when the last reference to the `dma_buf` is
 *   dropped (i.e., its reference count becomes zero).
 *
 * Call Chain:
 *   dma_buf_put() -> ... -> exporter_release()
 *
 * Steps to be handled:
 *   1. Check if the buffer and its private data exist.
 *   2. Free the memory allocated for the buffer's private data (`dmabuf->priv`).
 *   3. Set the private data pointer to NULL to avoid dangling pointers.
 *
 * @dmabuf: DMA-BUF to release
 */static void exporter_release(struct dma_buf *dmabuf)
{
	if (dmabuf && dmabuf->priv) {
		pr_info("Releasing DMA-BUF private data (size: %zu bytes)\n", 
			dmabuf->size);
		kfree(dmabuf->priv);
		dmabuf->priv = NULL;
	}
}

/**
 * exporter_vmap() - Map buffer into kernel virtual address space
 *
 * Calling Context:
 *   This function is a callback in the `dma_buf_ops` structure. It is
 *   invoked by the DMA-BUF framework when an importer calls `dma_buf_vmap()`.
 *
 * Call Chain:
 *   importer -> dma_buf_vmap() -> exporter_vmap()
 *
 * Steps to be handled:
 *   1. Take the virtual address stored in `dmabuf->priv`.
 *   2. Use `iosys_map_set_vaddr()` to store this address in the `iosys_map`
 *      structure provided by the framework.
 *
 * @dmabuf: DMA-BUF to map
 * @map: iosys_map structure to fill with mapping information
 *
 * Return: 0 on success
 */static int exporter_vmap(struct dma_buf *dmabuf, struct iosys_map *map)
{
	pr_debug("Mapping buffer for CPU access (vaddr: %p)\n", dmabuf->priv);
	iosys_map_set_vaddr(map, dmabuf->priv);
	return 0;
}

/**
 * exporter_vunmap() - Unmap kernel virtual mapping
 * @dmabuf: DMA-BUF to unmap
 * @map: iosys_map structure with mapping information
 *
 * Cleanup function for virtual mapping. Since our buffer uses static
 * allocation (kzalloc), no cleanup is needed.
 */
static void exporter_vunmap(struct dma_buf *dmabuf, struct iosys_map *map)
{
	pr_debug("Unmapping buffer from CPU access\n");
	/* No cleanup needed for kzalloc'd buffer */
}

/**
 * exporter_mmap() - Map buffer to userspace (not implemented)
 * @dmabuf: DMA-BUF to map
 * @vma: Virtual memory area for mapping
 *
 * This function would allow userspace to map the buffer. Not implemented
 * in this basic example - see Part 04 for mmap functionality.
 *
 * Return: -ENODEV (not supported)
 */
static int exporter_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{
	pr_debug("Userspace mmap not supported in this example\n");
	return -ENODEV;
}

/*
 * DMA-BUF operations structure
 * 
 * This structure defines the callback functions that the DMA-BUF framework
 * will call for various operations on our buffer. We implement basic CPU
 * access (vmap/vunmap) but don't support DMA operations in this example.
 */
static const struct dma_buf_ops exp_dmabuf_ops = {
	.map_dma_buf = exporter_map_dma_buf,
	.unmap_dma_buf = exporter_unmap_dma_buf,
	.release = exporter_release,
	.vmap = exporter_vmap,
	.vunmap = exporter_vunmap,
	.mmap = exporter_mmap,
};

/**
 * exporter_alloc_page() - Allocate and export a DMA-BUF
 *
 * Calling Context:
 *   This function is called from the module's initialization routine
 *   (`exporter_init`) to create the `dma_buf` that will be shared.
 *
 * Call Chain:
 *   exporter_init() -> exporter_alloc_page()
 *
 * Steps to be handled:
 *   1. Allocate a page of kernel memory using `kzalloc`.
 *   2. Configure the `dma_buf_export_info` structure with the buffer's
 *      properties, including the `dma_buf_ops` and the allocated memory as
 *      private data.
 *   3. Export the buffer by calling `dma_buf_export()`.
 *   4. If successful, write initial test data into the buffer.
 *   5. Return the newly created `dma_buf` pointer.
 *
 * Return: DMA-BUF pointer on success, ERR_PTR on failure
 */static struct dma_buf *exporter_alloc_page(void)
{
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	struct dma_buf *dmabuf;
	void *vaddr;

	/* Allocate a page-sized buffer */
	vaddr = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (!vaddr) {
		pr_err("Failed to allocate %lu bytes\n", PAGE_SIZE);
		return ERR_PTR(-ENOMEM);
	}

	/* Setup export information */
	exp_info.ops = &exp_dmabuf_ops;
	exp_info.size = PAGE_SIZE;
	exp_info.flags = O_CLOEXEC;
	exp_info.priv = vaddr;

	/* Export the buffer as a DMA-BUF */
	dmabuf = dma_buf_export(&exp_info);
	if (IS_ERR(dmabuf)) {
		pr_err("Failed to export DMA-BUF: %ld\n", PTR_ERR(dmabuf));
		kfree(vaddr);
		return dmabuf;
	}

	/* Fill buffer with test data */
	strscpy(vaddr, "hello world!", PAGE_SIZE);
	pr_info("Created DMA-BUF with test data: '%s'\n", (char *)vaddr);
	
	return dmabuf;
}

static int __init exporter_init(void)
{
	pr_info("DMA-BUF Basic Exporter (Part 01) - Initializing\n");
	
	dmabuf_exported = exporter_alloc_page();
	if (IS_ERR(dmabuf_exported)) {
		int ret = PTR_ERR(dmabuf_exported);
		pr_err("Failed to create and export DMA-BUF: %d\n", ret);
		return ret;
	}

	pr_info("DMA-BUF Exporter initialized successfully\n");
	pr_info("Buffer size: %zu bytes, available as 'dmabuf_exported'\n", 
		dmabuf_exported->size);
	return 0;
}

/**
 * exporter_exit() - Module cleanup function
 *
 * Called when the module is unloaded. Drops our reference to the DMA-BUF,
 * which may trigger its release if no other modules are using it.
 *
 * Note: If CONFIG_DMA_SHARED_BUFFER_DEBUG is enabled, debugfs may hold
 * an additional reference that prevents cleanup.
 */
static void __exit exporter_exit(void)
{
	pr_info("DMA-BUF Basic Exporter (Part 01) - Cleaning up\n");
	
	if (!IS_ERR_OR_NULL(dmabuf_exported)) {
		pr_info("Dropping exporter's reference to DMA-BUF\n");
		dma_buf_put(dmabuf_exported);
		dmabuf_exported = NULL;
	}
	
	pr_info("DMA-BUF Exporter cleanup complete\n");
}

/* Module entry points */
module_init(exporter_init);
module_exit(exporter_exit);

/* Module metadata */
MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("DMA-BUF Basic Exporter");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
MODULE_IMPORT_NS("DMA_BUF");
MODULE_ALIAS("dmabuf-basic-exporter");
