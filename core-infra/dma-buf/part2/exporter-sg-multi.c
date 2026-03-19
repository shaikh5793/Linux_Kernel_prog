/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 *
 * exporter-sg-multi.c - DMA-BUF Exporter with Scattered (Multi-Page) Buffer
 *
 * Demonstrates WHY sg_table exists: the buffer is spread across N
 * individually-allocated pages that are NOT physically contiguous.
 * Each page becomes a separate sg_table entry, so the importer must
 * iterate the full list with for_each_sgtable_dma_sg().
 *
 * Contrast with exporter-sg.c which uses a single kzalloc page and
 * always produces a 1-entry sg_table.
 */

#include <linux/dma-buf.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/iosys-map.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/vmalloc.h>

#define NUM_PAGES 4  /* 4 scattered pages = 16 KB total */

/* Private data attached to the dma_buf */
struct multi_buf {
	struct page *pages[NUM_PAGES];
	void       *vaddr;	/* vmap mapping for CPU access */
};

struct dma_buf *dmabuf_sg_multi_exported;
EXPORT_SYMBOL_GPL(dmabuf_sg_multi_exported);

static int multi_attach(struct dma_buf *dmabuf,
			struct dma_buf_attachment *attachment)
{
	pr_info("SG-Multi-Exporter: Device %s attached\n",
		dev_name(attachment->dev));
	return 0;
}

static void multi_detach(struct dma_buf *dmabuf,
			 struct dma_buf_attachment *attachment)
{
	pr_info("SG-Multi-Exporter: Device %s detached\n",
		dev_name(attachment->dev));
}

/*
 * multi_map_dma_buf() - Build a multi-entry sg_table from scattered pages.
 *
 * Each page was allocated independently with alloc_page(), so they sit
 * at arbitrary physical addresses.  We create one SG entry per page,
 * then call dma_map_sgtable() which maps every entry for the importer's
 * device.  The importer will see NUM_PAGES entries when it iterates
 * the table with for_each_sgtable_dma_sg().
 *
 * On systems with an IOMMU the post-map nents may be smaller than
 * orig_nents because the IOMMU can coalesce adjacent IOVA ranges.
 *
 * Call Chain:
 *   importer -> dma_buf_map_attachment() -> multi_map_dma_buf()
 *
 * Steps:
 *   1. Allocate an sg_table with NUM_PAGES entries.
 *   2. Fill each entry with the corresponding struct page.
 *   3. dma_map_sgtable() to create DMA mappings for all entries.
 *   4. Return the populated sg_table.
 */
static struct sg_table *multi_map_dma_buf(struct dma_buf_attachment *attachment,
					  enum dma_data_direction dir)
{
	struct multi_buf *mbuf = attachment->dmabuf->priv;
	struct sg_table *sgt;
	struct scatterlist *sg;
	int i, ret;

	sgt = kmalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt)
		return ERR_PTR(-ENOMEM);

	ret = sg_alloc_table(sgt, NUM_PAGES, GFP_KERNEL);
	if (ret) {
		kfree(sgt);
		return ERR_PTR(ret);
	}

	/* Fill each SG entry with one scattered page */
	for_each_sgtable_sg(sgt, sg, i)
		sg_set_page(sg, mbuf->pages[i], PAGE_SIZE, 0);

	/* Map all entries for the importer's device in one call */
	ret = dma_map_sgtable(attachment->dev, sgt, dir, 0);
	if (ret) {
		sg_free_table(sgt);
		kfree(sgt);
		return ERR_PTR(ret);
	}

	pr_info("SG-Multi-Exporter: Mapped %d pages (orig_nents=%u, nents=%u)\n",
		NUM_PAGES, sgt->orig_nents, sgt->nents);
	return sgt;
}

static void multi_unmap_dma_buf(struct dma_buf_attachment *attachment,
				struct sg_table *sgt,
				enum dma_data_direction dir)
{
	dma_unmap_sgtable(attachment->dev, sgt, dir, 0);
	sg_free_table(sgt);
	kfree(sgt);
	pr_info("SG-Multi-Exporter: DMA mapping released\n");
}

static void multi_release(struct dma_buf *dmabuf)
{
	struct multi_buf *mbuf = dmabuf->priv;
	int i;

	if (!mbuf)
		return;

	if (mbuf->vaddr)
		vunmap(mbuf->vaddr);

	for (i = 0; i < NUM_PAGES; i++) {
		if (mbuf->pages[i])
			__free_page(mbuf->pages[i]);
	}
	kfree(mbuf);
	pr_info("SG-Multi-Exporter: Released %d scattered pages\n", NUM_PAGES);
}

static int multi_vmap(struct dma_buf *dmabuf, struct iosys_map *map)
{
	struct multi_buf *mbuf = dmabuf->priv;

	if (!mbuf->vaddr)
		return -ENOMEM;

	iosys_map_set_vaddr(map, mbuf->vaddr);
	return 0;
}

static void multi_vunmap(struct dma_buf *dmabuf, struct iosys_map *map)
{
	/* vmap lifetime is managed in alloc/release, not here */
}

static const struct dma_buf_ops multi_dmabuf_ops = {
	.attach        = multi_attach,
	.detach        = multi_detach,
	.map_dma_buf   = multi_map_dma_buf,
	.unmap_dma_buf = multi_unmap_dma_buf,
	.release       = multi_release,
	.vmap          = multi_vmap,
	.vunmap        = multi_vunmap,
};

/*
 * multi_alloc_scattered() - Allocate N non-contiguous pages and export.
 *
 * Each page is allocated separately with alloc_page(GFP_KERNEL) so
 * the kernel is free to place them at any physical address.  We then
 * vmap() them into a single contiguous kernel virtual range so that
 * CPU access (via dma_buf_vmap) can treat the buffer as one block.
 *
 * Steps:
 *   1. Allocate NUM_PAGES individual pages.
 *   2. vmap() them for contiguous CPU access.
 *   3. Write test data across all pages.
 *   4. Export as a dma_buf of size NUM_PAGES * PAGE_SIZE.
 */
static struct dma_buf *multi_alloc_scattered(void)
{
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	struct multi_buf *mbuf;
	struct dma_buf *dmabuf;
	int i;

	mbuf = kzalloc(sizeof(*mbuf), GFP_KERNEL);
	if (!mbuf)
		return ERR_PTR(-ENOMEM);

	/* Step 1: Allocate scattered pages */
	for (i = 0; i < NUM_PAGES; i++) {
		mbuf->pages[i] = alloc_page(GFP_KERNEL | __GFP_ZERO);
		if (!mbuf->pages[i]) {
			pr_err("SG-Multi-Exporter: Failed to allocate page %d\n", i);
			goto err_free_pages;
		}
		pr_info("SG-Multi-Exporter: Page %d at phys %pad\n",
			i, &(dma_addr_t){page_to_phys(mbuf->pages[i])});
	}

	/* Step 2: vmap for contiguous CPU access */
	mbuf->vaddr = vmap(mbuf->pages, NUM_PAGES, VM_MAP, PAGE_KERNEL);
	if (!mbuf->vaddr) {
		pr_err("SG-Multi-Exporter: vmap failed\n");
		goto err_free_pages;
	}

	/* Step 3: Write test data spanning all pages */
	snprintf(mbuf->vaddr, NUM_PAGES * PAGE_SIZE,
		 "scattered buffer: %d pages, %lu bytes total",
		 NUM_PAGES, (unsigned long)(NUM_PAGES * PAGE_SIZE));

	/* Step 4: Export */
	exp_info.ops  = &multi_dmabuf_ops;
	exp_info.size = NUM_PAGES * PAGE_SIZE;
	exp_info.flags = O_CLOEXEC;
	exp_info.priv = mbuf;

	dmabuf = dma_buf_export(&exp_info);
	if (IS_ERR(dmabuf)) {
		pr_err("SG-Multi-Exporter: dma_buf_export failed: %ld\n",
		       PTR_ERR(dmabuf));
		goto err_vunmap;
	}

	pr_info("SG-Multi-Exporter: Exported %d scattered pages (%zu bytes)\n",
		NUM_PAGES, dmabuf->size);
	return dmabuf;

err_vunmap:
	vunmap(mbuf->vaddr);
err_free_pages:
	for (i = 0; i < NUM_PAGES; i++) {
		if (mbuf->pages[i])
			__free_page(mbuf->pages[i]);
	}
	kfree(mbuf);
	return ERR_PTR(-ENOMEM);
}

static int __init multi_exporter_init(void)
{
	dmabuf_sg_multi_exported = multi_alloc_scattered();
	if (IS_ERR(dmabuf_sg_multi_exported)) {
		int ret = PTR_ERR(dmabuf_sg_multi_exported);
		pr_err("SG-Multi-Exporter: Init failed: %d\n", ret);
		return ret;
	}

	pr_info("SG-Multi-Exporter: Ready (symbol: dmabuf_sg_multi_exported)\n");
	return 0;
}

static void __exit multi_exporter_exit(void)
{
	if (!IS_ERR_OR_NULL(dmabuf_sg_multi_exported)) {
		dma_buf_put(dmabuf_sg_multi_exported);
		dmabuf_sg_multi_exported = NULL;
	}
	pr_info("SG-Multi-Exporter: Unloaded\n");
}

module_init(multi_exporter_init);
module_exit(multi_exporter_exit);

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("DMA-BUF Exporter with scattered (non-contiguous) multi-page buffer");
MODULE_LICENSE("GPL");
MODULE_IMPORT_NS("DMA_BUF");
