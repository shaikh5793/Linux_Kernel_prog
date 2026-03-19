/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 *
 * kmalloc memory overhead
 * Demonstrates: Additional memory overhead beyond requested size
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>

static int __init overhead_init(void)
{
	void *ptr;
	size_t requested, actual;
	int i;

	pr_info("[KMALLOC] memory overhead analysis\n");

	/* Power-of-2 sizes typically align with slab cache boundaries */
	size_t sizes[] = { 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096 };

	for (i = 0; i < 10; i++) {
		requested = sizes[i];
		/* kmalloc() returns pointer to memory from appropriate slab cache */
		ptr = kmalloc(requested, GFP_KERNEL);
		if (ptr) {
			/* ksize() returns actual allocated size from slab cache
			 * Always >= requested size due to cache granularity */
			actual = ksize(ptr);
			pr_info
			    ("[KMALLOC] %4zu -> %4zu bytes (%2d%% overhead)\n",
			     requested, actual,
			     (int)((actual - requested) * 100 / requested));
			/* Free immediately to avoid memory pressure during analysis */
			kfree(ptr);
		} else {
			pr_err("[KMALLOC] Alloc %zu bytes failed\n", requested);
		}
	}

	/* Odd sizes demonstrate cache rounding behavior */
	size_t odd_sizes[] = { 9, 17, 33, 65, 129 };
	for (i = 0; i < 5; i++) {
		requested = odd_sizes[i];
		ptr = kmalloc(requested, GFP_KERNEL);
		if (ptr) {
			actual = ksize(ptr);
			pr_info("[KMALLOC] %3zu -> %3zu bytes (%2zu wasted)\n",
				requested, actual, actual - requested);
			kfree(ptr);
		} else {
			pr_err("[KMALLOC] Alloc %zu bytes failed\n", requested);
		}
	}

	return 0;
}

static void __exit overhead_exit(void)
{
	pr_info("[KMALLOC] Module unloaded\n");
}

module_init(overhead_init);
module_exit(overhead_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("kmalloc overhead");
