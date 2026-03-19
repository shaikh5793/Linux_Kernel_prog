/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>

static int __init basics_init(void)
{
	void *ptr;
	int *array;

	pr_info("[KMALLOC] kernel memory allocation (slab-based)\n");

	/* kmalloc() allocates uninitialized memory */
	ptr = kmalloc(1024, GFP_KERNEL);
	if (ptr) {
		pr_info("[KMALLOC] kmalloc(1024): %px\n", ptr);
		kfree(ptr);
	} else {
		pr_err("[KMALLOC] kmalloc(1024): FAILED\n");
	}

	/* kzalloc() allocates and zeros memory */
	ptr = kzalloc(512, GFP_KERNEL);
	if (ptr) {
		pr_info("[KMALLOC] kzalloc(512): %px (zeroed: %s)\n", ptr,
			*(char *)ptr == 0 ? "yes" : "no");
		kfree(ptr);
	} else {
		pr_err("[KMALLOC] kzalloc(512): FAILED\n");
	}

	/* kmalloc_array() allocates arrays with overflow protection */
	array = kmalloc_array(10, sizeof(int), GFP_KERNEL);
	if (array) {
		pr_info("[KMALLOC] kmalloc_array(10, %zu): %px\n", sizeof(int),
			array);
		kfree(array);
	} else {
		pr_err("[KMALLOC] kmalloc_array(): FAILED\n");
	}

	/* kcalloc() allocates and zeros arrays */
	array = kcalloc(5, sizeof(int), GFP_KERNEL);
	if (array) {
		pr_info("[KMALLOC] kcalloc(5, %zu): %px (zeroed: %s)\n",
			sizeof(int), array, array[0] == 0 ? "yes" : "no");
		kfree(array);
	} else {
		pr_err("[KMALLOC] kcalloc(): FAILED\n");
	}

	return 0;
}

static void __exit basics_exit(void)
{
	pr_info("[KMALLOC] Module unloaded\n");
}

module_init(basics_init);
module_exit(basics_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("kmalloc interfaces");
