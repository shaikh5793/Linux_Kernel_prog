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

static void *basic_ptr, *zero_ptr;
static size_t size = 1024 * 1024;

static int alloc_init(void)
{
	basic_ptr = vmalloc(size);
	if (!basic_ptr) {
		pr_err("vmalloc allocation failed\n");
		return -ENOMEM;
	}

	zero_ptr = vzalloc(size);
	if (!zero_ptr) {
		pr_err("vzalloc allocation failed\n");
		vfree(basic_ptr);
		return -ENOMEM;
	}

	pr_info("vmalloc allocation successful\n");
	return 0;
}

static void alloc_exit(void)
{
	vfree(basic_ptr);
	vfree(zero_ptr);
}

module_init(alloc_init);
module_exit(alloc_exit);

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("Basic vmalloc test");
MODULE_LICENSE("Dual MIT/GPL");
