/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include "coremod.h" // Include the shared header

/**
 * usermod_init() - The module initialization function.
 *
 * This function demonstrates the use of symbols exported by another module.
 * It gets the initial value of the counter from coremod, sets a new value,
 * and then verifies the new value.
 *
 * Return: Always 0.
 */
static int __init usermod_init(void)
{
	int original_value;
	int new_value;

	pr_info("User module initializing.\n");

	/* Get the original value from the core module. */
	original_value = get_counter();
	pr_info("Original counter value from coremod: %d\n", original_value);

	/* Set a new value in the core module. */
	pr_info("Setting counter value to 200...\n");
	set_counter(200);

	/* Get the new value to confirm it was set. */
	new_value = get_counter();
	pr_info("New counter value from coremod: %d\n", new_value);

	return 0;
}

/**
 * usermod_exit() - The module exit function.
 *
 * Prints a cleanup message before the module unloads.
 */
static void __exit usermod_exit(void)
{
	pr_info("User module exiting.\n");
}

module_init(usermod_init);
module_exit(usermod_exit);

MODULE_AUTHOR("raghu@techveda.org");
MODULE_DESCRIPTION("A module that uses symbols from another module");
MODULE_LICENSE("Dual MIT/GPL");
