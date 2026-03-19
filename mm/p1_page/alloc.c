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

static int __init page_init(void)
{
	struct page *page;
	void *page_addr;

	pr_info("[ALLOC] page allocator (buddy system allocation)\n");

	/* alloc_page() allocates exactly one page (order 0) */
	page = alloc_page(GFP_KERNEL);
	if (page) {
		page_addr = page_address(page);
		pr_info("[ALLOC] Single page: %p -> %p\n", page, page_addr);
		memset(page_addr, 0xAA, PAGE_SIZE);
		__free_page(page);
	} else {
		pr_err("[ALLOC] Single page: FAILED\n");
	}

	/* alloc_pages(order): order N = 2^N pages, contiguous physical memory */
	page = alloc_pages(GFP_KERNEL, 1);
	if (page) {
		page_addr = page_address(page);
		pr_info("[ALLOC] Order-1: %p -> %p (2 pages)\n", page,
			page_addr);
		memset(page_addr, 0xBB, PAGE_SIZE);
		memset(page_addr + PAGE_SIZE, 0xCC, PAGE_SIZE);
		__free_pages(page, 1);
	} else {
		pr_err("[ALLOC] Order-1: FAILED\n");
	}

	/* Higher orders provide larger contiguous blocks */
	page = alloc_pages(GFP_KERNEL, 2);
	if (page) {
		page_addr = page_address(page);
		pr_info("[ALLOC] Order-2: %p -> %p (4 pages)\n", page,
			page_addr);
		for (int i = 0; i < 4; i++) {
			memset(page_addr + i * PAGE_SIZE, 0x10 + i, PAGE_SIZE);
		}

		__free_pages(page, 2);
	} else {
		pr_err("[ALLOC] Order-2: FAILED\n");
	}

	/* Order-3 = 8 pages = 32KB contiguous */
	page = alloc_pages(GFP_KERNEL, 3);
	if (page) {
		page_addr = page_address(page);
		pr_info("[ALLOC] Order-3: %p -> %p (8 pages)\n", page,
			page_addr);
		for (int i = 0; i < 8; i++) {
			memset(page_addr + i * PAGE_SIZE, 'A' + i, PAGE_SIZE);
		}

		__free_pages(page, 3);
	} else {
		pr_err("[ALLOC] Order-3: FAILED\n");
	}

	/* Individual alloc_page() calls may return non-contiguous pages */
	struct page *pages[3];
	for (int i = 0; i < 3; i++) {
		pages[i] = alloc_page(GFP_KERNEL);
		if (pages[i]) {
			page_addr = page_address(pages[i]);
			pr_info("[ALLOC] Page %d: %p -> %p\n", i, pages[i],
				page_addr);
			memset(page_addr, 0x20 + i, PAGE_SIZE);
		}
	}

	for (int i = 0; i < 3; i++) {
		if (pages[i]) {
			__free_page(pages[i]);
		}
	}

	return 0;
}

static void __exit page_exit(void)
{
	pr_info("[ALLOC] Module unloaded\n");
}

module_init(page_init);
module_exit(page_exit);

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("Page allocator interfaces");
MODULE_LICENSE("Dual MIT/GPL");
