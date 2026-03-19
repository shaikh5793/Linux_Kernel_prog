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

static int __init gfp_init(void)
{
	struct page *page;
	int order = 0;

	pr_info("[MASK] GFP allocation flags and masks\n");

	/* GFP_KERNEL: most common flag, can sleep and reclaim memory */
	page = alloc_pages(GFP_KERNEL, order);
	if (page) {
		pr_info("[MASK] GFP_KERNEL: SUCCESS (can sleep)\n");
		__free_pages(page, order);
	} else {
		pr_err("[MASK] GFP_KERNEL: FAILED\n");
	}

	/* GFP_ATOMIC: used in interrupt context, cannot sleep */
	page = alloc_pages(GFP_ATOMIC, order);
	if (page) {
		pr_info("[MASK] GFP_ATOMIC: SUCCESS (no sleep)\n");
		__free_pages(page, order);
	} else {
		pr_err("[MASK] GFP_ATOMIC: FAILED\n");
	}

	/* GFP_HIGHUSER: allocates from high memory zones for user pages */
	page = alloc_pages(GFP_HIGHUSER, order);
	if (page) {
		pr_info("[MASK] GFP_HIGHUSER: SUCCESS (high memory)\n");
		__free_pages(page, order);
	} else {
		pr_err("[MASK] GFP_HIGHUSER: FAILED\n");
	}

	/* Modifier flags are combined with base flags using bitwise OR */
	/* __GFP_ZERO: clears allocated memory to zero before returning */
	page = alloc_pages(GFP_KERNEL | __GFP_ZERO, order);
	if (page) {
		pr_info("[MASK] __GFP_ZERO: SUCCESS\n");
		void *kaddr = page_address(page);
		if (kaddr && *(char *)kaddr == 0) {
			pr_info("[MASK] Zero-initialization verified\n");
		}
		__free_pages(page, order);
	} else {
		pr_err("[MASK] __GFP_ZERO: FAILED\n");
	}

	/* __GFP_COMP: creates compound pages (used for huge pages) */
	page = alloc_pages(GFP_KERNEL | __GFP_COMP, 2);
	if (page) {
		pr_info("[MASK] __GFP_COMP: SUCCESS (order=2)\n");
		pr_info("[MASK] Compound page: %s (size: %lu bytes)\n",
			PageCompound(page) ? "Yes" : "No",
			(1UL << 2) * PAGE_SIZE);
		__free_pages(page, 2);
	} else {
		pr_err("[MASK] __GFP_COMP: FAILED\n");
	}

	/* __GFP_NOWARN: suppresses kernel warning messages on allocation failure */
	page = alloc_pages(GFP_KERNEL | __GFP_NOWARN, order);
	if (page) {
		pr_info("[MASK] __GFP_NOWARN: SUCCESS\n");
		__free_pages(page, order);
	} else {
		pr_info("[MASK] __GFP_NOWARN: FAILED (silent)\n");
	}

	/* __GFP_RETRY_MAYFAIL: retry allocation but allow failure */
	page = alloc_pages(GFP_KERNEL | __GFP_RETRY_MAYFAIL, order);
	if (page) {
		pr_info("[MASK] __GFP_RETRY_MAYFAIL: SUCCESS\n");
		__free_pages(page, order);
	} else {
		pr_err("[MASK] __GFP_RETRY_MAYFAIL: FAILED\n");
	}

	/* __GFP_NOFAIL: never fails, will retry indefinitely (use with caution) */
	page = alloc_pages(GFP_KERNEL | __GFP_NOFAIL, order);
	if (page) {
		pr_info("[MASK] __GFP_NOFAIL: SUCCESS (guaranteed)\n");
		__free_pages(page, order);
	} else {
		pr_err("[MASK] __GFP_NOFAIL: FAILED (should never happen)\n");
	}

	/* __GFP_NOMEMALLOC: don't use emergency memory reserves */
	page = alloc_pages(GFP_KERNEL | __GFP_NOMEMALLOC, order);
	if (page) {
		pr_info("[MASK] __GFP_NOMEMALLOC: SUCCESS\n");
		__free_pages(page, order);
	} else {
		pr_err("[MASK] __GFP_NOMEMALLOC: FAILED\n");
	}
	return 0;
}

static void __exit gfp_exit(void)
{
	pr_info("[MASK] Module unloaded\n");
}

module_init(gfp_init);
module_exit(gfp_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("Page GFP masks");
