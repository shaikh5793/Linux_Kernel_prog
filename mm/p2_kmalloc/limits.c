/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 *
 * kmalloc size limits
 * Demonstrates: Finding maximum allocation size
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>

static int __init limits_init(void)
{
	void *ptr;
	size_t size = 1024;
	size_t max_size = 0;

	pr_info("[LIMITS] kmalloc size limits testing\n");

	while (size <= 8 * 1024 * 1024) {
		ptr = kmalloc(size, GFP_KERNEL);
		if (ptr) {
			pr_info("[LIMITS] Size %zu KB: OK\n", size / 1024);
			/* Free immediately to avoid memory pressure */
			kfree(ptr);
			max_size = size;
			size *= 2;
		} else {
			pr_info("[LIMITS] Size %zu KB: FAILED\n", size / 1024);
			break;
		}
	}

	pr_info("[LIMITS] Max size: %zu KB, PAGE_SIZE: %lu\n",
		max_size / 1024, PAGE_SIZE);

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
MODULE_DESCRIPTION("kmalloc limits");
