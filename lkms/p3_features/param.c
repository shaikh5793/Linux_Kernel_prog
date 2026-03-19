/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/stat.h> /* For S_IRUGO etc */
#include <linux/kernel.h> /* For pr_info etc */

/*
 * Module parameters.
 */

/* An array of integers for base I/O ports. */
static int base_port[5];
/* Stores the actual number of base_port values provided by the user. */
static unsigned int num_ports;
/* The IRQ line number. Defaulted to -1 to indicate it's not set. */
static int irq = -1;
/* The name of the device driver. */
static char *name = "default_device";
/* A boolean flag to toggle debug messages. */
static bool debug_mode;

/*
 * module_param_array(name, type, num, perm);
 * @name: the name of the array variable
 * @type: the type of the elements in the array
 * @num: a pointer to a variable that will store the number of elements
 * @perm: the permissions for the corresponding sysfs entry
 */
module_param_array(base_port, int, &num_ports, S_IRUGO);
MODULE_PARM_DESC(base_port, "Base I/O ports (up to 5, integer array)");

/*
 * module_param(name, type, perm);
 * @name: the name of the variable
 * @type: the type of the variable
 * @perm: the permissions for the corresponding sysfs entry
 */
module_param(irq, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(irq, "The IRQ line number (integer)");

module_param(name, charp, S_IRUGO);
MODULE_PARM_DESC(name, "The device driver name (string)");

module_param(debug_mode, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug_mode, "Enable or disable debug mode (bool)");

/**
 * param_init() - The module initialization function.
 *
 * This function is called when the module is loaded. It validates and prints
 * the parameters passed to the module.
 *
 * Return: 0 on success, or an error code on failure.
 */
static int __init param_init(void)
{
	int i;

	pr_info("Module loaded.\n");

	/* Validate the IRQ number. */
	if (irq < 0) {
		pr_warn("IRQ number %d is invalid. Please provide a valid IRQ.\n",
			irq);
		/* In a real driver, you might return -EINVAL here. */
	}

	/* Check if the user provided a device name. */
	if (strcmp(name, "default_device") == 0)
		pr_warn("Device name not specified, using default '%s'.\n",
			name);

	if (debug_mode)
		pr_info("Debug mode is enabled.\n");

	pr_info("IRQ: %d\n", irq);
	pr_info("Device Name: %s\n", name);
	pr_info("Received %u base ports.\n", num_ports);

	/* Loop only for the number of ports actually provided. */
	for (i = 0; i < num_ports; i++)
		pr_info("Base Port %d: 0x%x\n", i + 1, base_port[i]);

	return 0;
}

/**
 * param_exit() - The module exit function.
 *
 * This function is called when the module is unloaded. It prints a
 * clean-up message.
 */
static void __exit param_exit(void)
{
	pr_info("Unloading module for device '%s'.\n", name);
}

module_init(param_init);
module_exit(param_exit);

MODULE_AUTHOR("raghu@techveda.org");
MODULE_DESCRIPTION("Demonstrates module parameters");
MODULE_LICENSE("Dual MIT/GPL");
