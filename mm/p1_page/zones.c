/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 *
 * Memory Zones demonstration
 *
 * This module demonstrates zone-specific allocations and characteristics
 * including ZONE_DMA, ZONE_NORMAL, ZONE_HIGHMEM allocations,
 * zone fallback behavior, and zone performance characteristics.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/gfp.h>
#include <linux/highmem.h>

static int __init zones_init(void)
{
	struct page *page;
	int order = 0;

	pr_info("[ZONES] memory zone allocation\n");

	pr_info("[ZONES] Total zones: %d | PAGE_SIZE: %lu bytes\n",
		MAX_NR_ZONES, PAGE_SIZE);
	pr_info("[ZONES] ZONE_DMA: %d | ZONE_NORMAL: %d\n", ZONE_DMA,
		ZONE_NORMAL);
#ifdef ZONE_HIGHMEM
	pr_info("[ZONES] ZONE_HIGHMEM: %d (32-bit system)\n", ZONE_HIGHMEM);
#else
	pr_info("[ZONES] ZONE_HIGHMEM: not available (64-bit system)\n");
#endif

	/* GFP_DMA targets ZONE_DMA (0-16MB, needed for legacy devices) */
	page = alloc_pages(GFP_DMA, order);
	if (page) {
		pr_info("[ZONES] ZONE_DMA: SUCCESS (0-16MB)\n");
		__free_pages(page, order);
	} else {
		pr_err("[ZONES] ZONE_DMA: FAILED\n");
	}

	/* GFP_KERNEL targets ZONE_NORMAL (kernel's preferred zone) */
	page = alloc_pages(GFP_KERNEL, order);
	if (page) {
		pr_info("[ZONES] ZONE_NORMAL: SUCCESS (kernel)\n");
		__free_pages(page, order);
	} else {
		pr_err("[ZONES] ZONE_NORMAL: FAILED\n");
	}

	/* GFP_HIGHUSER targets ZONE_HIGHMEM (user pages, can be swapped) */
	page = alloc_pages(GFP_HIGHUSER, order);
	if (page) {
		pr_info("[ZONES] ZONE_HIGHMEM: SUCCESS (user)\n");
		__free_pages(page, order);
	} else {
		pr_err("[ZONES] ZONE_HIGHMEM: FAILED\n");
	}

	/* Zone fallback: allocator can fall back to other zones if preferred zone is full */
	page = alloc_pages(GFP_KERNEL | __GFP_DMA | __GFP_NOWARN, order);
	if (page) {
		pr_info("[ZONES] DMA with fallback: SUCCESS\n");
		__free_pages(page, order);
	} else {
		pr_info("[ZONES] DMA with fallback: FAILED\n");
	}

	pr_info("[ZONES] Total RAM: %lu pages (%lu MB)\n",
		totalram_pages(),
		(totalram_pages() * PAGE_SIZE) / (1024 * 1024));
	pr_info("[ZONES] High memory: %lu pages (%lu MB)\n", totalhigh_pages(),
		(totalhigh_pages() * PAGE_SIZE) / (1024 * 1024));
	pr_info("[ZONES] Low memory: %lu pages (%lu MB)\n",
		totalram_pages() - totalhigh_pages(),
		((totalram_pages() -
		  totalhigh_pages()) * PAGE_SIZE) / (1024 * 1024));

	return 0;
}

static void __exit zones_exit(void)
{
	pr_info("[ZONES] Module unloaded\n");
}

module_init(zones_init);
module_exit(zones_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("Memory zones");
