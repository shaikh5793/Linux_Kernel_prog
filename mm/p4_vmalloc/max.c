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

static void *test_ptr;
static size_t sizes[] =
    { 1 * 1024 * 1024, 16 * 1024 * 1024, 64 * 1024 * 1024, 256 * 1024 * 1024 };
static int max_size_index = -1;

static int max_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(sizes); i++) {
		test_ptr = vmalloc(sizes[i]);
		if (test_ptr) {
			max_size_index = i;
			pr_info("Allocated %zu MB successfully\n",
				sizes[i] / (1024 * 1024));
			vfree(test_ptr);
		} else {
			pr_info("Failed to allocate %zu MB\n",
				sizes[i] / (1024 * 1024));
			break;
		}
	}

	if (max_size_index >= 0) {
		pr_info("vmalloc limit testing successful\n");
		return 0;
	}

	pr_err("all allocations failed\n");
	return -ENOMEM;
}

static void max_exit(void)
{
	/* All allocations were freed in init */
}

module_init(max_init);
module_exit(max_exit);

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("vmalloc limits test");
MODULE_LICENSE("Dual MIT/GPL");
