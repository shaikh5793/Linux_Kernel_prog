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

static void *vmalloc_ptr, *kmalloc_ptr;
static size_t size = 64 * 1024;

static int utils_init(void)
{
	vmalloc_ptr = vmalloc(size);
	if (!vmalloc_ptr) {
		pr_err("vmalloc allocation failed\n");
		return -ENOMEM;
	}

	kmalloc_ptr = kmalloc(1024, GFP_KERNEL);
	if (!kmalloc_ptr) {
		pr_err("kmalloc allocation failed\n");
		vfree(vmalloc_ptr);
		return -ENOMEM;
	}

	pr_info("vmalloc utilities test successful\n");
	return 0;
}

static void utils_exit(void)
{
	vfree(vmalloc_ptr);
	kfree(kmalloc_ptr);
}

module_init(utils_init);
module_exit(utils_exit);

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("vmalloc utilities test");
MODULE_LICENSE("Dual MIT/GPL");
