/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#include <linux/dma-buf.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/iosys-map.h>

MODULE_IMPORT_NS("DMA_BUF");

/*
 * External DMA-BUF reference
 *
 * This references the DMA-BUF exported by the exporter module.
 * The exporter must be loaded before this importer module.
 */
extern struct dma_buf *dmabuf_exported;

/**
 * importer_test() - Test importing the DMA-BUF
 *
 * Calling Context:
 *   This function is called from the module's initialization routine
 *   (`importer_init`) after ensuring the exported `dma_buf` is available.
 *
 * Call Chain:
 *   importer_init() -> importer_test()
 *
 * Steps to be handled:
 *   1. Map the `dma_buf` into the kernel's virtual address space using
 *      `dma_buf_vmap()`. This triggers the exporter's `vmap` operation.
 *   2. Check if the mapping was successful.
 *   3. Read and print the content from the mapped memory to verify access.
 *   4. Unmap the buffer using `dma_buf_vunmap()` to release the virtual mapping.
 *
 * @dmabuf: DMA-BUF to import and test
 *
 * Return: 0 on success, negative error code on failure
 */static int importer_test(struct dma_buf *dmabuf)
{
	struct iosys_map map;
	int ret;

	if (!dmabuf) {
		pr_err("dmabuf_exported is null\n");
		return -EINVAL;
	}

	/* Obtain a virtual mapping of the buffer using vmap */
	ret = dma_buf_vmap(dmabuf, &map);
	if (ret) {
		pr_err("Failed to vmap dmabuf: %d\n", ret);
		return ret;
	}

	/* Validate the mapping and read buffer content */
	if (iosys_map_is_null(&map)) {
		pr_err("vmap returned a null mapping\n");
		dma_buf_vunmap(dmabuf, &map);
		return -ENOMEM;
	}

	/* Output the buffer content */
	pr_info("Read from dmabuf vmap: %s\n", (char *)map.vaddr);

	/* Clean up the mapping */
	dma_buf_vunmap(dmabuf, &map);

	return 0;
}

static int __init importer_init(void)
{
	int ret;

	/* Ensure the exported dmabuf is available */
	if (!dmabuf_exported) {
		pr_err("dmabuf_exported is not initialized\n");
		return -ENODEV;
	}

	/* Perform the import and buffer read test */
	ret = importer_test(dmabuf_exported);
	if (ret) {
		pr_err("Importer test failed: %d\n", ret);
		return ret;
	}

	pr_info("DMA-BUF Importer initialized successfully\n");
	return 0;
}

/**
 * importer_exit() - Module cleanup function
 *
 * Called when the module is unloaded. We take no special cleanup action
 * beyond logging the unload event, as our interactions are transient.
 */
static void __exit importer_exit(void)
{
	pr_info("DMA-BUF Importer exited\n");
}

/* Module entry points */
module_init(importer_init);
module_exit(importer_exit);

/* Module metadata */
MODULE_AUTHOR("Raghu Bharadwaj raghu@techveda.org");
MODULE_DESCRIPTION("DMA-BUF Importer - Part 01 of DMA-BUF Learning Series");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
MODULE_ALIAS("dmabuf-basic-importer");
