/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 *
 * importer-sg.c - DMA-BUF Scatter-Gather Importer
 *
 * Demonstrates the DMA import path: attach → map_attachment → read
 * sg_dma_address from the scatter-gather table → unmap → detach.
 * Also performs CPU access via vmap to verify buffer content.
 */

#include <linux/dma-buf.h>
#include <linux/iosys-map.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>

MODULE_IMPORT_NS("DMA_BUF");

/* Import from scatter-gather capable exporter */
extern struct dma_buf *dmabuf_sg_exported;

static struct platform_device *importer_pdev;

/*
 * importer_test_dma() - Tests DMA mapping path on the imported buffer.
 *
 * Call Chain:
 *   importer_init() -> importer_test_dma()
 *
 * Steps:
 *   1. dma_buf_attach() — attach our device to the dma_buf
 *   2. dma_buf_map_attachment() — triggers exporter's map_dma_buf(),
 *      returns an sg_table with DMA addresses
 *   3. Read sg_dma_address() and sg_dma_len() from the sg_table
 *   4. dma_buf_unmap_attachment() — triggers exporter's unmap_dma_buf()
 *   5. dma_buf_detach() — detach our device
 */
static int importer_test_dma(struct dma_buf *dmabuf, struct device *dev)
{
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	struct scatterlist *sg;
	dma_addr_t dma_addr;
	unsigned int len;
	int i = 0;

	/* Step 1: Attach our device to the dma_buf */
	attach = dma_buf_attach(dmabuf, dev);
	if (IS_ERR(attach)) {
		pr_err("SG-Importer: dma_buf_attach failed: %ld\n",
		       PTR_ERR(attach));
		return PTR_ERR(attach);
	}
	pr_info("SG-Importer: attached to dma_buf (size=%zu)\n", dmabuf->size);

	/* Step 2: Map attachment — triggers exporter's map_dma_buf() */
	sgt = dma_buf_map_attachment(attach, DMA_FROM_DEVICE);
	if (IS_ERR(sgt)) {
		pr_err("SG-Importer: dma_buf_map_attachment failed: %ld\n",
		       PTR_ERR(sgt));
		dma_buf_detach(dmabuf, attach);
		return PTR_ERR(sgt);
	}

	/* Step 3: Read DMA addresses from the scatter-gather table */
	pr_info("SG-Importer: sg_table has %u entries (nents=%u)\n",
		sgt->orig_nents, sgt->nents);

	for_each_sgtable_dma_sg(sgt, sg, i) {
		dma_addr = sg_dma_address(sg);
		len = sg_dma_len(sg);
		pr_info("SG-Importer: SG entry %d: DMA addr=%pad, len=%u\n",
			i, &dma_addr, len);
	}

	/* Step 4: Unmap — triggers exporter's unmap_dma_buf() */
	dma_buf_unmap_attachment(attach, sgt, DMA_FROM_DEVICE);
	pr_info("SG-Importer: DMA mapping released\n");

	/* Step 5: Detach */
	dma_buf_detach(dmabuf, attach);
	pr_info("SG-Importer: detached from dma_buf\n");

	return 0;
}

/*
 * importer_test_cpu() - Tests CPU access via vmap (same as Part 1).
 */
static int importer_test_cpu(struct dma_buf *dmabuf)
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

	pr_info("SG-Importer: CPU access via vmap: '%s'\n",
		(char *)map.vaddr);

	dma_buf_vunmap(dmabuf, &map);
	return 0;
}

static int __init importer_init(void)
{
	struct device *dev;
	int ret;

	if (!dmabuf_sg_exported)
		return -ENODEV;

	/* Create a platform device to serve as our DMA device */
	importer_pdev = platform_device_register_simple("sg_importer",
							-1, NULL, 0);
	if (IS_ERR(importer_pdev))
		return PTR_ERR(importer_pdev);

	dev = &importer_pdev->dev;

	/* Required for dma_map_single in exporter's map_dma_buf */
	dma_set_mask_and_coherent(dev, DMA_BIT_MASK(64));

	/* Test 1: DMA mapping path (attach → map → read SG → unmap → detach) */
	ret = importer_test_dma(dmabuf_sg_exported, dev);
	if (ret) {
		pr_err("SG-Importer: DMA mapping test failed: %d\n", ret);
		platform_device_unregister(importer_pdev);
		return ret;
	}

	/* Test 2: CPU access via vmap (verifies buffer content) */
	ret = importer_test_cpu(dmabuf_sg_exported);
	if (ret) {
		pr_err("SG-Importer: CPU access test failed: %d\n", ret);
		platform_device_unregister(importer_pdev);
		return ret;
	}

	pr_info("SG-Importer: Both DMA and CPU access tests passed\n");
	return 0;
}

static void __exit importer_exit(void)
{
	platform_device_unregister(importer_pdev);
	pr_info("DMA-BUF SG-Importer exited\n");
}

module_init(importer_init);
module_exit(importer_exit);

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("DMA-BUF Scatter-Gather Importer");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
MODULE_ALIAS("dmabuf-sg-importer");
