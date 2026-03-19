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

/* The pr_debug macro is only active if the DEBUG macro is defined.*/
#define DEBUG

/**
 * config_init() - The module initialization function.
 *
 * This function shows how conditional compilation can be used to include
 * or exclude debug messages.
 *
 * Return: Always 0.
 */
static int __init config_init(void)
{
	pr_info("Config module loaded.\n");

	/*
	 * This pr_debug message will only be compiled into the module if the
	 * DEBUG macro is defined at compile time. This can be done by adding
	 * EXTRA_CFLAGS += -DDEBUG to the Makefile.
	 */
	pr_debug("This is a debug message. It will only appear in debug builds.\n");

	return 0;
}

/**
 * config_exit() - The module cleanup function.
 */
static void __exit config_exit(void)
{
	pr_info("Config module unloaded.\n");
}

module_init(config_init);
module_exit(config_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("raghu@techveda.org");
MODULE_DESCRIPTION("A module demonstrating compile-time configuration.");
MODULE_VERSION("0.1");

