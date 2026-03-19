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

static int __init limits_init(void)
{
	struct page *page;
	int order;
	int max_order = 0;
	int failure_order = -1;
	unsigned long size_bytes;

	pr_info("[LIMITS] page allocation limits testing\n");

	/* Higher orders become exponentially harder to allocate */

	for (order = 0; order <= 20; order++) {
		page = alloc_pages(GFP_KERNEL, order);
		if (page) {
			size_bytes = (1 << order) * PAGE_SIZE;
			if (order <= 3 || order % 2 == 0) {
				pr_info("[LIMITS] Order %2d: %4d pages %s\n",
					order, 1 << order,
					format_size(size_bytes));
			}
			__free_pages(page, order);
			max_order = order;
		} else {
			size_bytes = (1 << order) * PAGE_SIZE;
			pr_info("[LIMITS] Order %2d: FAILED %4d pages %s\n",
				order, 1 << order, format_size(size_bytes));
			failure_order = order;

			/* Different GFP flags may succeed where GFP_KERNEL failed */

			page = alloc_pages(GFP_ATOMIC, order);
			if (page) {
				pr_info
				    ("[LIMITS] Order %2d GFP_ATOMIC: SUCCESS\n",
				     order);
				__free_pages(page, order);
			} else {
				pr_info
				    ("[LIMITS] Order %2d GFP_ATOMIC: FAILED\n",
				     order);
			}

			page = alloc_pages(GFP_NOWAIT, order);
			if (page) {
				pr_info
				    ("[LIMITS] Order %2d GFP_NOWAIT: SUCCESS\n",
				     order);
				__free_pages(page, order);
			} else {
				pr_info
				    ("[LIMITS] Order %2d GFP_NOWAIT: FAILED\n",
				     order);
			}

			page = alloc_pages(GFP_USER, order);
			if (page) {
				pr_info
				    ("[LIMITS] Order %2d GFP_USER: SUCCESS\n",
				     order);
				__free_pages(page, order);
			} else {
				pr_info("[LIMITS] Order %2d GFP_USER: FAILED\n",
					order);
			}

			page = alloc_pages(GFP_HIGHUSER, order);
			if (page) {
				pr_info
				    ("[LIMITS] Order %2d GFP_HIGHUSER: SUCCESS\n",
				     order);
				__free_pages(page, order);
			} else {
				pr_info
				    ("[LIMITS] Order %2d GFP_HIGHUSER: FAILED\n",
				     order);
			}

			break;
		}
	}
	size_bytes = (1 << max_order) * PAGE_SIZE;
	pr_info("[LIMITS] Maximum order: %d (%d pages) %s\n",
		max_order, 1 << max_order, format_size(size_bytes));

	if (failure_order != -1) {
		size_bytes = (1 << failure_order) * PAGE_SIZE;
		pr_info("[LIMITS] First failure: order %d (%d pages) %s\n",
			failure_order, 1 << failure_order,
			format_size(size_bytes));
	}

	/* Memory fragmentation can cause sporadic successes beyond normal limits */
	for (order = max_order + 1; order <= max_order + 3; order++) {
		page = alloc_pages(GFP_KERNEL, order);
		if (page) {
			size_bytes = (1 << order) * PAGE_SIZE;
			pr_info("[LIMITS] Order %d: UNEXPECTED SUCCESS %s\n",
				order, format_size(size_bytes));
			__free_pages(page, order);
		} else {
			pr_info("[LIMITS] Order %d: FAILED\n", order);
		}
	}

	pr_info("[LIMITS] Total: %lu pages (%lu MB)\n",
		totalram_pages(),
		(totalram_pages() * PAGE_SIZE) / (1024 * 1024));
	pr_info("[LIMITS] High: %lu pages | Low: %lu pages\n",
		totalhigh_pages(), totalram_pages() - totalhigh_pages());

	return 0;
}

static void __exit limits_exit(void)
{
	pr_info("[LIMITS] Module unloaded\n");
}

module_init(limits_init);
module_exit(limits_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("Page limits");
