/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>

static void *small_ptr, *large_ptr;
static size_t small_size = 64 * 1024;
static size_t large_size = 4 * 1024 * 1024;

static int kvmalloc_init(void)
{
	small_ptr = kvmalloc(small_size, GFP_KERNEL);
	if (!small_ptr) {
		pr_err("kvmalloc small allocation failed\n");
		return -ENOMEM;
	}

	large_ptr = kvmalloc(large_size, GFP_KERNEL);
	if (!large_ptr) {
		pr_err("kvmalloc large allocation failed\n");
		kvfree(small_ptr);
		return -ENOMEM;
	}

	pr_info("kvmalloc allocation successful\n");
	return 0;
}

static void kvmalloc_exit(void)
{
	kvfree(small_ptr);
	kvfree(large_ptr);
}

module_init(kvmalloc_init);
module_exit(kvmalloc_exit);

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("kvmalloc test");
MODULE_LICENSE("Dual MIT/GPL");
