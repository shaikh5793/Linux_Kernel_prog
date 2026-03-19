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

/* The counter variable, now static to enforce encapsulation. */
static int veda_counter;

/**
 * set_counter() - Sets the value of the internal counter.
 * @a: The new integer value for the counter.
 *
 * Return: Always 0.
 */
int set_counter(int a)
{
	veda_counter = a;
	return 0;
}

/**
 * get_counter() - Gets the current value of the internal counter.
 *
 * Return: The current integer value of the counter.
 */
int get_counter(void)
{
	return veda_counter;
}

/**
 * coremod_init() - The module initialization function.
 *
 * Initializes the counter to a default value of 100.
 *
 * Return: Always 0.
 */
static int __init coremod_init(void)
{
	pr_info("Core module initializing. Setting counter to 100.\n");
	veda_counter = 100;
	return 0;
}

/**
 * coremod_exit() - The module exit function.
 *
 * Prints the final value of the counter before unloading.
 */
static void __exit coremod_exit(void)
{
	pr_info("Core module exiting. Final value of veda_counter: %d\n",
		get_counter());
}

module_init(coremod_init);
module_exit(coremod_exit);

/* Export the symbols for other modules to use. */
EXPORT_SYMBOL_GPL(set_counter);
EXPORT_SYMBOL(get_counter);

MODULE_AUTHOR("raghu@techveda.org");
MODULE_DESCRIPTION("A core module that exports symbols");
MODULE_LICENSE("Dual MIT/GPL");
