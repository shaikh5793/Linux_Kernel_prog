/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#include <linux/dma-buf.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/iosys-map.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>

struct dma_buf *dmabuf_sg_exported;
EXPORT_SYMBOL_GPL(dmabuf_sg_exported);

static int exporter_attach(struct dma_buf *dmabuf, struct dma_buf_attachment *attachment)
{
	pr_info("SG-Exporter: Device %s attached to DMA-BUF\n", 
		dev_name(attachment->dev));
	return 0;
}

static void exporter_detach(struct dma_buf *dmabuf, struct dma_buf_attachment *attachment)
{
	pr_info("SG-Exporter: Device %s detached from DMA-BUF\n", 
		dev_name(attachment->dev));
}

/*
 * exporter_map_dma_buf() - Maps the buffer for DMA access by an importer.
 *
 * Calling Context:
 *   This function is a callback in the `dma_buf_ops` structure. It is
 *   invoked by the DMA-BUF framework when an importer calls
 *   `dma_buf_map_attachment()`.
 *
 * Call Chain:
 *   importer -> dma_buf_map_attachment() -> exporter_map_dma_buf()
 *
 * Steps to be handled:
 *   1. Allocate a scatter-gather table (`sg_table`) structure.
 *   2. Initialize the `sg_table` to hold a single entry.
 *   3. Create a DMA mapping for the buffer's memory (`vaddr`) for the
 *      importer's device, getting a `dma_addr_t`.
 *   4. Store the resulting DMA address and length in the `sg_table` entry.
 *   5. Return the populated `sg_table` to the DMA-BUF framework.
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
 *
 */
static struct sg_table *exporter_map_dma_buf(struct dma_buf_attachment *attachment,
					     enum dma_data_direction dir)
{
	void *vaddr = attachment->dmabuf->priv;
	struct sg_table *table;
	dma_addr_t dma_addr;
	int err;

	pr_debug("SG-Exporter: Mapping buffer for DMA (direction: %d)\n", dir);

	/* Step 1: Allocate scatter-gather table structure */
	table = kmalloc(sizeof(*table), GFP_KERNEL);
	if (!table)
		return ERR_PTR(-ENOMEM);

	/* Step 2: Initialize sg_table with one entry */
	err = sg_alloc_table(table, 1, GFP_KERNEL);
	if (err) {
		kfree(table);
		return ERR_PTR(err);
	}

	/* Step 3: Create DMA mapping for hardware access */
	dma_addr = dma_map_single(attachment->dev, vaddr, PAGE_SIZE, dir);
	if (dma_mapping_error(attachment->dev, dma_addr)) {
		sg_free_table(table);
		kfree(table);
		return ERR_PTR(-ENOMEM);
	}

	/* Step 4: Store DMA address in scatter-gather entry */
	sg_dma_address(table->sgl) = dma_addr;
	sg_dma_len(table->sgl) = PAGE_SIZE;

	pr_info("SG-Exporter: Buffer mapped for DMA at address %pad\n", &dma_addr);
	return table;
}

static void exporter_unmap_dma_buf(struct dma_buf_attachment *attachment,
				   struct sg_table *table,
				   enum dma_data_direction dir)
{
	dma_addr_t dma_addr = sg_dma_address(table->sgl);
	dma_unmap_single(attachment->dev, dma_addr, PAGE_SIZE, dir);
	sg_free_table(table);
	kfree(table);
}

static void exporter_release(struct dma_buf *dmabuf)
{
	if (dmabuf && dmabuf->priv) {
		kfree(dmabuf->priv);
		dmabuf->priv = NULL;
	}
}

static int exporter_vmap(struct dma_buf *dmabuf, struct iosys_map *map)
{
	iosys_map_set_vaddr(map, dmabuf->priv);
	return 0;
}

static void exporter_vunmap(struct dma_buf *dmabuf, struct iosys_map *map)
{
	/* No cleanup needed */
}

static int exporter_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{
	return -ENODEV; 
}

static const struct dma_buf_ops exp_dmabuf_ops = {
	.attach = exporter_attach,        
	.detach = exporter_detach,        
	.map_dma_buf = exporter_map_dma_buf,    
	.unmap_dma_buf = exporter_unmap_dma_buf, 
	.release = exporter_release,      
	.vmap = exporter_vmap,            
	.vunmap = exporter_vunmap,        
	.mmap = exporter_mmap,            
};

/*
 * exporter_alloc_page() - Allocates and exports a DMA-capable buffer.
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
 *      properties, including the custom `dma_buf_ops` and the allocated
 *      memory as private data.
 *   3. Export the buffevmap = exporter_vmap,r by calling `dma_buf_export()`.
 *   4. If successful, write initial test data into the buffer.
 *   5. Return the newly created `dma_buf` pointer.
 */
static struct dma_buf *exporter_alloc_page(void)
{
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	struct dma_buf *dmabuf;
	void *vaddr;

	/* Allocate buffer memory */
	vaddr = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (!vaddr) {
		pr_err("SG-Exporter: Failed to allocate %lu bytes\n", PAGE_SIZE);
		return ERR_PTR(-ENOMEM);
	}

	/* Configure export information */
	exp_info.ops = &exp_dmabuf_ops;
	exp_info.size = PAGE_SIZE;
	exp_info.flags = O_CLOEXEC;
	exp_info.priv = vaddr;

	/* Export the buffer with DMA capabilities */
	dmabuf = dma_buf_export(&exp_info);
	if (IS_ERR(dmabuf)) {
		pr_err("SG-Exporter: Failed to export DMA-BUF: %ld\n", PTR_ERR(dmabuf));
		kfree(vaddr);
		return dmabuf;
	}

	/* Initialize buffer with test data */
	strscpy(vaddr, "hello world!", PAGE_SIZE);
	pr_info("SG-Exporter: Created DMA-capable buffer with test data\n");
	
	return dmabuf;
}

static int __init exporter_init(void)

{
	pr_info("DMA-BUF Scatter-Gather Exporter (Track A, Part 02) - Initializing\n");
	
	dmabuf_sg_exported = exporter_alloc_page();
	if (IS_ERR(dmabuf_sg_exported)) {
		int ret = PTR_ERR(dmabuf_sg_exported);
		pr_err("SG-Exporter: Initialization failed: %d\n", ret);
		return ret;
	}

	pr_info("SG-Exporter: Initialized successfully\n");
	pr_info("SG-Exporter: Buffer available as 'dmabuf_sg_exported' with DMA support\n");
	return 0;
}

/**
 * exporter_exit() - Module cleanup function
 *
 * Drops the module's reference to the DMA-BUF, potentially triggering
 * cleanup if no other modules are using it.
 */
static void __exit exporter_exit(void)
{
	pr_info("DMA-BUF Scatter-Gather Exporter (Track A, Part 02) - Cleaning up\n");
	
	if (!IS_ERR_OR_NULL(dmabuf_sg_exported)) {
		pr_info("SG-Exporter: Dropping reference to DMA-BUF\n");
		dma_buf_put(dmabuf_sg_exported);
		dmabuf_sg_exported = NULL;
		
		pr_info("SG-Exporter: If unload fails, debugfs may hold reference (use rmmod -f)\n");
	}
	
	pr_info("SG-Exporter: Cleanup complete\n");
}

/* Module entry points */
module_init(exporter_init);
module_exit(exporter_exit);

/* Module metadata */
MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("DMA-BUF SG Exporter");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
MODULE_IMPORT_NS("DMA_BUF");
MODULE_ALIAS("dmabuf-sg-exporter");
