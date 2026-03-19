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
#include "mymod.h"

/**
 * cmod_init() - The initialization function for the cmod module.
 *
 * This is the entry point of the module. It calls a function from another
 * source file (routine.c) to demonstrate how modules can be built from
 * multiple files.
 *
 * Return: Always 0.
 */
static int __init cmod_init(void)
{
	int ret;
	pr_info("cmod: Hello from the kernel!\n");
	ret = mod_routine();
	pr_info("cmod: mod_routine() returned %d\n", ret);
	return 0;
}

/**
 * cmod_exit() - The cleanup function for the cmod module.
 */
static void __exit cmod_exit(void)
{
	pr_info("cmod: Goodbye from the kernel!\n");
}

module_init(cmod_init);
module_exit(cmod_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("raghu@techveda.org");
MODULE_DESCRIPTION("A module built from multiple source files.");
