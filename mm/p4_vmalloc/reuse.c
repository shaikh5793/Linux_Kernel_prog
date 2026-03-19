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
#include <linux/delay.h>

static void *vaddr1, *vaddr2;
static size_t size = 4096;

static int reuse_init(void)
{
	vaddr1 = vmalloc(size);
	if (!vaddr1) {
		pr_err("first allocation failed\n");
		return -ENOMEM;
	}

	pr_info("First allocation at: %px\n", vaddr1);
	vfree(vaddr1);

	msleep(100);

	vaddr2 = vmalloc(size);
	if (!vaddr2) {
		pr_err("second allocation failed\n");
		return -ENOMEM;
	}

	pr_info("Second allocation at: %px\n", vaddr2);
	pr_info("Address reuse test: %s\n",
		(vaddr1 == vaddr2) ? "REUSED" : "NEW");

	return 0;
}

static void reuse_exit(void)
{
	vfree(vaddr2);
}

module_init(reuse_init);
module_exit(reuse_exit);

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("vmalloc address reuse test");
MODULE_LICENSE("Dual MIT/GPL");
