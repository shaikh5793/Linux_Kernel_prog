/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 *
 * Page Frame Number (PFN) Management
 * Demonstrates PFN <-> page conversions and address translation
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/gfp.h>
#include <linux/highmem.h>

static int __init pfn_init(void)
{
	struct page *page;
	unsigned long pfn;
	int order = 0;

	pr_info("[PFN] page frame number operations\n");

	/* PFN = Page Frame Number: converts between page descriptors and physical addresses */
	page = alloc_pages(GFP_KERNEL, order);
	if (page) {
		pfn = page_to_pfn(page);
		pr_info("[PFN] Page at PFN: %lu\n", pfn);
		pr_info("[PFN] Conversions: PFN->page=%p | page->virt=%p\n",
			pfn_to_page(pfn), page_address(page));
		__free_pages(page, order);
	} else {
		pr_err("[PFN] Page allocation failed\n");
	}

	/* Multi-page allocations provide contiguous PFNs */
	page = alloc_pages(GFP_KERNEL, 2);
	if (page) {
		pfn = page_to_pfn(page);
		pr_info("[PFN] Multi-page start PFN: %lu\n", pfn);
		pr_info("[PFN] Range: %lu-%lu (4 pages, %lu bytes)\n",
			pfn, pfn + 3, 4 * PAGE_SIZE);
		__free_pages(page, 2);
	}

	pr_info("[PFN] Total PFNs: %lu | Page size: %lu bytes\n",
		totalram_pages(), PAGE_SIZE);
	pr_info("[PFN] Physical memory: %lu MB\n",
		(totalram_pages() * PAGE_SIZE) / (1024 * 1024));

	/* Complete conversion cycle: page <-> PFN <-> virtual address */

	page = alloc_pages(GFP_KERNEL, order);
	if (page) {
		pfn = page_to_pfn(page);

		pr_info("[PFN] Conversions: page=%p <-> PFN=%lu <-> virt=%p\n",
			page, pfn, page_address(page));
		pr_info("[PFN] Verification: %s\n",
			(page == pfn_to_page(pfn)
			 && page ==
			 virt_to_page(page_address(page))) ? "SUCCESS" :
			"FAILED");

		__free_pages(page, order);
	}

	/* PFN arithmetic: useful for memory range operations */

	page = alloc_pages(GFP_KERNEL, order);
	if (page) {
		pfn = page_to_pfn(page);

		pr_info("[PFN] Base PFN: %lu\n", pfn);
		pr_info("[PFN] Arithmetic: +1=%lu | +10=%lu\n", pfn + 1,
			pfn + 10);
		pr_info("[PFN] Distance: %lu pages = %lu bytes\n",
			(pfn + 10) - pfn, ((pfn + 10) - pfn) * PAGE_SIZE);
		pr_info("[PFN] Physical addresses: 0x%lx -> 0x%lx\n",
			pfn << PAGE_SHIFT, (pfn + 10) << PAGE_SHIFT);

		__free_pages(page, order);
	}

	/* pfn_valid() checks if PFNs are backed by actual RAM */

	pr_info("[PFN] Valid range: 0-%lu (%lu total pages)\n",
		totalram_pages() - 1, totalram_pages());
	pr_info("[PFN] Boundary tests: PFN 0=%s | Last=%s | Beyond=%s\n",
		pfn_valid(0) ? "OK" : "FAIL",
		pfn_valid(totalram_pages() - 1) ? "OK" : "FAIL",
		pfn_valid(totalram_pages())? "OK" : "FAIL");
	pr_info("[PFN] Sample tests: PFN 100=%s | PFN 999999999=%s\n",
		pfn_valid(100) ? "OK" : "FAIL",
		pfn_valid(999999999) ? "OK" : "FAIL");

	return 0;
}

static void __exit pfn_exit(void)
{
	pr_info("[PFN] Module unloaded\n");
}

module_init(pfn_init);
module_exit(pfn_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("PFN management");
