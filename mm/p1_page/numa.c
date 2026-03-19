/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 *
 * NUMA-Aware Page Allocation demonstration
 *
 * This module demonstrates NUMA node-specific page allocation including
 * alloc_pages_node(), NUMA-aware GFP flags, locality optimization,
 * and cross-node allocation patterns.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/gfp.h>
#include <linux/highmem.h>
#include <linux/numa.h>

static int __init numa_init(void)
{
	struct page *page;
	int order = 0;
	int node;

	pr_info("[NUMA] NUMA-aware page allocation\n");

	pr_info("[NUMA] Online nodes: %d | Current node: %d\n",
		num_online_nodes(), numa_node_id());

	for_each_online_node(node) {
		pr_info("[NUMA] Node %d: available\n", node);
	}

	/*
	 * === NODE-SPECIFIC ALLOCATIONS ===
	 * Purpose: Show how to target allocations to specific NUMA nodes
	 * Key concept: alloc_pages_node() forces allocation on a particular node
	 */
	/* alloc_pages_node() targets specific NUMA nodes for locality optimization */
	for_each_online_node(node) {
		page = alloc_pages_node(node, GFP_KERNEL, order);
		if (page) {
			pr_info("[NUMA] Node %d: SUCCESS\n", node);
			__free_pages(page, order);
		} else {
			pr_err("[NUMA] Node %d: FAILED\n", node);
		}
	}

	/* NUMA-specific flags modify locality vs availability trade-offs */
	/* __GFP_THISNODE: strict node allocation, no fallback allowed */
	page =
	    alloc_pages_node(numa_node_id(), GFP_KERNEL | __GFP_THISNODE,
			     order);
	if (page) {
		pr_info("[NUMA] __GFP_THISNODE: SUCCESS (local only)\n");
		__free_pages(page, order);
	} else {
		pr_err("[NUMA] __GFP_THISNODE: FAILED\n");
	}

	/* __GFP_NORETRY: fail fast if node is busy, avoids NUMA performance penalties */
	page =
	    alloc_pages_node(numa_node_id(), GFP_KERNEL | __GFP_NORETRY, order);
	if (page) {
		pr_info("[NUMA] __GFP_NORETRY: SUCCESS (no retry)\n");
		__free_pages(page, order);
	} else {
		pr_err("[NUMA] __GFP_NORETRY: FAILED\n");
	}

	return 0;
}

static void __exit numa_exit(void)
{
	pr_info("[NUMA] Module unloaded\n");
}

module_init(numa_init);
module_exit(numa_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("NUMA pages");
