/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 *
 * importer-sg-multi.c - DMA-BUF Importer for Scattered Multi-Page Buffer
 *
 * Imports the multi-page scattered buffer from exporter-sg-multi.c and
 * demonstrates:
 *   1. DMA path: attach → map → iterate ALL sg entries → unmap → detach
 *   2. CPU path: vmap → read content spanning multiple pages → vunmap
 *
 * The key difference from importer-sg.c (single page) is that
 * for_each_sgtable_dma_sg() now produces multiple entries, each with
 * its own DMA address — proving the scatter-gather table is not just
 * a wrapper but a real list of disjoint physical regions.
 */

#include <linux/dma-buf.h>
#include <linux/iosys-map.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>

MODULE_IMPORT_NS("DMA_BUF");

extern struct dma_buf *dmabuf_sg_multi_exported;

static struct platform_device *importer_pdev;

/*
 * multi_importer_test_dma() - Walk the multi-entry sg_table.
 *
 * Call Chain:
 *   multi_importer_init() -> multi_importer_test_dma()
 *
 * Steps:
 *   1. dma_buf_attach() with our platform device.
 *   2. dma_buf_map_attachment() — triggers exporter's multi_map_dma_buf()
 *      which returns an sg_table with NUM_PAGES entries.
 *   3. Iterate with for_each_sgtable_dma_sg() and log each entry's
 *      DMA address and length.
 *   4. Sum total mapped size to verify all pages are accounted for.
 *   5. Unmap and detach.
 */
static int multi_importer_test_dma(struct dma_buf *dmabuf, struct device *dev)
{
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	struct scatterlist *sg;
	unsigned int total_len = 0;
	int i = 0;

	attach = dma_buf_attach(dmabuf, dev);
	if (IS_ERR(attach)) {
		pr_err("SG-Multi-Importer: attach failed: %ld\n",
		       PTR_ERR(attach));
		return PTR_ERR(attach);
	}
	pr_info("SG-Multi-Importer: Attached (buf size=%zu)\n", dmabuf->size);

	sgt = dma_buf_map_attachment(attach, DMA_FROM_DEVICE);
	if (IS_ERR(sgt)) {
		pr_err("SG-Multi-Importer: map_attachment failed: %ld\n",
		       PTR_ERR(sgt));
		dma_buf_detach(dmabuf, attach);
		return PTR_ERR(sgt);
	}

	pr_info("SG-Multi-Importer: sg_table orig_nents=%u, nents=%u\n",
		sgt->orig_nents, sgt->nents);

	/* Walk every DMA-mapped scatter-gather entry */
	for_each_sgtable_dma_sg(sgt, sg, i) {
		dma_addr_t addr = sg_dma_address(sg);
		unsigned int len = sg_dma_len(sg);

		pr_info("SG-Multi-Importer:   entry %d: DMA addr=%pad, len=%u\n",
			i, &addr, len);
		total_len += len;
	}

	pr_info("SG-Multi-Importer: Total DMA-mapped size: %u bytes "
		"(expected %zu)\n", total_len, dmabuf->size);

	dma_buf_unmap_attachment(attach, sgt, DMA_FROM_DEVICE);
	dma_buf_detach(dmabuf, attach);
	pr_info("SG-Multi-Importer: DMA test done\n");
	return 0;
}

/*
 * multi_importer_test_cpu() - Read scattered buffer via vmap.
 *
 * The exporter's vmap callback returns a contiguous virtual mapping
 * (created with vmap()) that spans all scattered pages, so we can
 * read the buffer as a single contiguous block despite the pages
 * being physically disjoint.
 */
static int multi_importer_test_cpu(struct dma_buf *dmabuf)
{
	struct iosys_map map;
	int ret;

	ret = dma_buf_vmap(dmabuf, &map);
	if (ret)
		return ret;

	if (iosys_map_is_null(&map)) {
		dma_buf_vunmap(dmabuf, &map);
		return -ENOMEM;
	}

	pr_info("SG-Multi-Importer: CPU vmap content: '%s'\n",
		(char *)map.vaddr);

	dma_buf_vunmap(dmabuf, &map);
	return 0;
}

static int __init multi_importer_init(void)
{
	struct device *dev;
	int ret;

	if (!dmabuf_sg_multi_exported)
		return -ENODEV;

	importer_pdev = platform_device_register_simple("sg_multi_importer",
							-1, NULL, 0);
	if (IS_ERR(importer_pdev))
		return PTR_ERR(importer_pdev);

	dev = &importer_pdev->dev;
	dma_set_mask_and_coherent(dev, DMA_BIT_MASK(64));

	pr_info("SG-Multi-Importer: === DMA mapping test ===\n");
	ret = multi_importer_test_dma(dmabuf_sg_multi_exported, dev);
	if (ret) {
		pr_err("SG-Multi-Importer: DMA test failed: %d\n", ret);
		platform_device_unregister(importer_pdev);
		return ret;
	}

	pr_info("SG-Multi-Importer: === CPU access test ===\n");
	ret = multi_importer_test_cpu(dmabuf_sg_multi_exported);
	if (ret) {
		pr_err("SG-Multi-Importer: CPU test failed: %d\n", ret);
		platform_device_unregister(importer_pdev);
		return ret;
	}

	pr_info("SG-Multi-Importer: Both tests passed\n");
	return 0;
}

static void __exit multi_importer_exit(void)
{
	platform_device_unregister(importer_pdev);
	pr_info("SG-Multi-Importer: Unloaded\n");
}

module_init(multi_importer_init);
module_exit(multi_importer_exit);

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("DMA-BUF Importer for scattered multi-page buffer");
MODULE_LICENSE("GPL");
MODULE_IMPORT_NS("DMA_BUF");
