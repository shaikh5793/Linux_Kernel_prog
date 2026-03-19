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

static void *user_ptr, *node_ptr, *addr32_ptr;
static size_t size = 64 * 1024;

static int user_init(void)
{
	user_ptr = vmalloc_user(size);
	if (!user_ptr) {
		pr_err("vmalloc_user allocation failed\n");
		return -ENOMEM;
	}

	node_ptr = vmalloc_node(size, numa_node_id());
	if (!node_ptr) {
		pr_err("vmalloc_node allocation failed\n");
		vfree(user_ptr);
		return -ENOMEM;
	}

	addr32_ptr = vmalloc_32(size);
	if (!addr32_ptr) {
		pr_err("vmalloc_32 allocation failed\n");
		vfree(user_ptr);
		vfree(node_ptr);
		return -ENOMEM;
	}

	pr_info("vmalloc variants allocation successful\n");
	return 0;
}

static void user_exit(void)
{
	vfree(user_ptr);
	vfree(node_ptr);
	vfree(addr32_ptr);
}

module_init(user_init);
module_exit(user_exit);

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("vmalloc variants test");
MODULE_LICENSE("Dual MIT/GPL");
