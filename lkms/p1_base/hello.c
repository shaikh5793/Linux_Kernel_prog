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

/**
 * func() - A simple function to be called from the init function.
 */
static void func(void)
{
	pr_info("Inside func()\n");
}

/**
 * init_module() - The default module initialization function.
 *
 * This is the entry point for the module. The use of the name `init_module`
 * is the classic way of defining an init function.
 *
 * Return: 0 on success.
 */
int init_module(void)
{
	pr_info("Hello from the kernel!\n");
	func();
	return 0;
}

/**
 * cleanup_module() - The default module cleanup function.
 *
 * This is the exit point for the module. The use of the name `cleanup_module`
 * is the classic way of defining an exit function.
 */
void cleanup_module(void)
{
	pr_info("Goodbye from the kernel!\n");
}

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("raghu@techveda.org");
MODULE_DESCRIPTION("First kernel module");
MODULE_VERSION("0.1");
