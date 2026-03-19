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

static int *int_array;
static long *long_array;
static size_t count = 1024 * 1024;

static int array_init(void)
{
	int_array = vmalloc_array(count, sizeof(int));
	if (!int_array) {
		pr_err("int array allocation failed\n");
		return -ENOMEM;
	}

	long_array = vmalloc_array(count, sizeof(long));
	if (!long_array) {
		pr_err("long array allocation failed\n");
		vfree(int_array);
		return -ENOMEM;
	}

	pr_info("vmalloc array allocation successful\n");
	return 0;
}

static void array_exit(void)
{
	vfree(int_array);
	vfree(long_array);
}

module_init(array_init);
module_exit(array_exit);

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("vmalloc array test");
MODULE_LICENSE("Dual MIT/GPL");
