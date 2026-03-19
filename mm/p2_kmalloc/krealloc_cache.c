/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 *
 * krealloc Cache Changes
 * Demonstrates: Address changes when crossing cache boundaries
 *
 * Cache Boundary Transitions:
 * ===========================
 *
 *    kmalloc-8    kmalloc-16    kmalloc-32    kmalloc-64    kmalloc-128
 *        │            │            │            │            │
 *        ▼            ▼            ▼            ▼            ▼
 *   ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐
 *   │ 8 bytes │  │16 bytes │  │32 bytes │  │64 bytes │  │128 bytes│
 *   └─────────┘  └─────────┘  └─────────┘  └─────────┘  └─────────┘
 *        │            │            │            │            │
 *        └────────────┼────────────┼────────────┼────────────┘
 *                     │            │            │
 *              Address Changes     │            │
 *              when crossing  ─────┘            │
 *              boundaries                       │
 *                                    Same cache, same address
 *                                    (if space available)
 *
 * Expected Behavior:
 * - Small increases within same cache: Address stays same (in-place)
 * - Cache boundary crossings: New address (different cache)  
 * - Shrinking across boundaries: New address (smaller cache)
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>

static int __init krealloc_cache_init(void)
{
	void *ptr, *new_ptr;
	char *data;
	size_t sizes[] = { 8, 16, 32, 64, 128, 256, 512, 64, 32, 16, 8 };
	int i;

	pr_info("[KREALLOC] memory reallocation and caching\n");

	/* Initial allocation in kmalloc-8 cache */
	ptr = kmalloc(8, GFP_KERNEL);
	if (!ptr) {
		pr_err("[KREALLOC] Initial allocation failed\n");
		return -ENOMEM;
	}

	/* Fill with pattern to verify data preservation */
	data = (char *)ptr;
	strcpy(data, "TEST");
	pr_info("[KREALLOC] Initial: size=%d, addr=%px, data='%s'\n", 8, ptr,
		data);

	/* Test expansions and contractions across cache boundaries */
	for (i = 1; i < ARRAY_SIZE(sizes); i++) {
		new_ptr = krealloc(ptr, sizes[i], GFP_KERNEL);
		if (!new_ptr) {
			pr_err("[KREALLOC] krealloc(%zu) failed\n", sizes[i]);
			kfree(ptr);
			return -ENOMEM;
		}

		data = (char *)new_ptr;
		pr_info
		    ("[KREALLOC] Step %d: size=%zu, addr=%px, data='%s', %s\n",
		     i, sizes[i], new_ptr, data,
		     (new_ptr == ptr) ? "SAME_ADDR" : "NEW_ADDR");

		/* Update pointer for next iteration */
		ptr = new_ptr;

		/* Verify data is preserved across reallocations */
		if (strncmp(data, "TEST", 4) != 0) {
			pr_warn("[KREALLOC] Data corruption detected!\n");
		}
	}

	/* Final cleanup */
	kfree(ptr);

	/* Demonstrate cache size detection through krealloc behavior */
	pr_info("[KREALLOC] Cache behavior analysis:\n");

	ptr = kmalloc(16, GFP_KERNEL);
	if (ptr) {
		pr_info("[KREALLOC] 16->24: ");
		new_ptr = krealloc(ptr, 24, GFP_KERNEL);
		pr_cont("addr=%px %s (stays in kmalloc-32)\n", new_ptr,
			(new_ptr == ptr) ? "SAME" : "NEW");
		ptr = new_ptr;

		pr_info("[KREALLOC] 24->40: ");
		new_ptr = krealloc(ptr, 40, GFP_KERNEL);
		pr_cont("addr=%px %s (stays in kmalloc-64)\n", new_ptr,
			(new_ptr == ptr) ? "SAME" : "NEW");
		ptr = new_ptr;

		pr_info("[KREALLOC] 40->80: ");
		new_ptr = krealloc(ptr, 80, GFP_KERNEL);
		pr_cont("addr=%px %s (moves to kmalloc-128)\n", new_ptr,
			(new_ptr == ptr) ? "SAME" : "NEW");
		ptr = new_ptr;

		pr_info("[KREALLOC] 80->20: ");
		new_ptr = krealloc(ptr, 20, GFP_KERNEL);
		pr_cont("addr=%px %s (shrinks to kmalloc-32)\n", new_ptr,
			(new_ptr == ptr) ? "SAME" : "NEW");

		kfree(new_ptr);
	}

	/* Test edge cases */
	pr_info("[KREALLOC] Edge case testing:\n");

	/* krealloc with NULL pointer (should behave like kmalloc) */
	ptr = krealloc(NULL, 64, GFP_KERNEL);
	if (ptr) {
		pr_info
		    ("[KREALLOC] krealloc(NULL, 64): %px (equivalent to kmalloc)\n",
		     ptr);

		/* krealloc to size 0 (should behave like kfree) */
		new_ptr = krealloc(ptr, 0, GFP_KERNEL);
		pr_info
		    ("[KREALLOC] krealloc(ptr, 0): %px (equivalent to kfree)\n",
		     new_ptr);
		/* new_ptr should be NULL, no need to free */
	}

	return 0;
}

static void __exit krealloc_cache_exit(void)
{
	pr_info("[KREALLOC] Module unloaded\n");
}

module_init(krealloc_cache_init);
module_exit(krealloc_cache_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("krealloc cache boundary behavior");
