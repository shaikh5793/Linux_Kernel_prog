/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/gfp.h>
#include <linux/highmem.h>

/* Helper function to format size as string */
static const char *format_size(unsigned long bytes)
{
	static char size_str[32];
	if (bytes >= (1024ULL * 1024 * 1024)) {
		snprintf(size_str, sizeof(size_str), "(%llu GB)",
			 bytes / (1024ULL * 1024 * 1024));
	} else if (bytes >= (1024 * 1024)) {
		snprintf(size_str, sizeof(size_str), "(%lu MB)",
			 bytes / (1024 * 1024));
	} else if (bytes >= 1024) {
		snprintf(size_str, sizeof(size_str), "(%lu KB)", bytes / 1024);
	} else {
		snprintf(size_str, sizeof(size_str), "(%lu bytes)", bytes);
	}
	return size_str;
}

static int __init cma_large_init(void)
{
	struct page *page;
	int order;
	unsigned long size_bytes;

	pr_info("[CMA_LARGE] large contiguous memory allocation (CMA)\n");

	/* CMA can provide larger contiguous blocks than buddy allocator */
#ifdef CONFIG_CMA
	pr_info("\n[CMA_LARGE] CMA is available (CONFIG_CMA=y)\n");
#else
	pr_info
	    ("\n[CMA_LARGE] WARNING: CMA is NOT available (CONFIG_CMA is not set)\n");
	pr_info
	    ("[CMA_LARGE] This demonstration shows what would happen with CMA enabled\n");
	pr_info
	    ("[CMA_LARGE] To enable CMA: rebuild kernel with CONFIG_CMA=y and add cma=size to boot params\n");
#endif

	pr_info("\n[CMA_LARGE] Testing large allocations (orders 10-16)...\n");

	/* Start testing from order 10 (4MB) - typically beyond buddy allocator limits */
	for (order = 10; order <= 16; order++) {
		size_bytes = (1 << order) * PAGE_SIZE;

		pr_info("[CMA_LARGE] Testing order %d (%d pages) %s...\n",
			order, 1 << order, format_size(size_bytes));

		/* Compare normal buddy allocator vs CMA-friendly flags */
		page = alloc_pages(GFP_KERNEL, order);
		if (page) {
			pr_info("[CMA_LARGE]   GFP_KERNEL: [OK]\n");
			__free_pages(page, order);
		} else {
			pr_info("[CMA_LARGE]   GFP_KERNEL: [FAIL]\n");
		}

		/* GFP_HIGHUSER_MOVABLE: pages can be moved/migrated, CMA-friendly */
		page = alloc_pages(GFP_HIGHUSER_MOVABLE, order);
		if (page) {
			pr_info
			    ("[CMA_LARGE]   GFP_HIGHUSER_MOVABLE (CMA-friendly): [OK] SUCCESS\n");
			__free_pages(page, order);
		} else {
			pr_info
			    ("[CMA_LARGE]   GFP_HIGHUSER_MOVABLE (CMA-friendly): [FAIL]\n");
		}

		/* __GFP_MOVABLE: explicitly marks pages as movable for CMA */
		page = alloc_pages(GFP_USER | __GFP_MOVABLE, order);
		if (page) {
			pr_info("[CMA_LARGE]   GFP_USER|__GFP_MOVABLE: [OK]\n");
			__free_pages(page, order);
		} else {
			pr_info
			    ("[CMA_LARGE]   GFP_USER|__GFP_MOVABLE: [FAIL]\n");
		}

		pr_info("\n");

		/* Stop if we reach very large sizes */
		if (order >= 14) {	/* 64MB */
			pr_info
			    ("[CMA_LARGE] Stopping at order %d to avoid system stress\n",
			     order);
			break;
		}
	}

	/*
	 * Test CMA's ability to handle multiple large allocations
	 */
	pr_info
	    ("\n[CMA_LARGE] Multiple large allocation test (order 12 = 16MB)...\n");

	struct page *large_pages[5] = { NULL };
	int allocated_count = 0;
	order = 12;		/* 16MB */
	size_bytes = (1 << order) * PAGE_SIZE;

	for (int i = 0; i < 5; i++) {
		large_pages[i] = alloc_pages(GFP_HIGHUSER_MOVABLE, order);
		if (large_pages[i]) {
			allocated_count++;
			pr_info("[CMA_LARGE]   Block %d: [OK] %s\n",
				i + 1, format_size(size_bytes));
		} else {
			pr_info("[CMA_LARGE]   Block %d: [FAIL]\n", i + 1);
			break;
		}
	}

	pr_info("[CMA_LARGE] Total allocated: %d blocks of %s = %s\n",
		allocated_count, format_size(size_bytes),
		format_size(allocated_count * size_bytes));

	/* Free all allocated blocks */
	for (int i = 0; i < allocated_count; i++) {
		if (large_pages[i]) {
			__free_pages(large_pages[i], order);
		}
	}

	pr_info("\n[CMA_LARGE] Summary:\n");
#ifdef CONFIG_CMA
	pr_info("[CMA_LARGE] CMA was available and tested\n");
#else
	pr_info("[CMA_LARGE] CMA was NOT available on this system\n");
	pr_info
	    ("[CMA_LARGE] With CMA enabled, larger allocations would likely succeed\n");
	pr_info("[CMA_LARGE] \n");
	pr_info("[CMA_LARGE] To enable CMA:\n");
	pr_info("[CMA_LARGE] 1. Rebuild kernel with CONFIG_CMA=y\n");
	pr_info
	    ("[CMA_LARGE] 2. Add boot parameter: cma=128M (or desired size)\n");
#endif

	return 0;
}

static void __exit cma_large_exit(void)
{
	pr_info("[CMA_LARGE] Module unloaded\n");
}

module_init(cma_large_init);
module_exit(cma_large_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("CMA large block allocation using alloc_pages");
