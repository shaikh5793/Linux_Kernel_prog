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
 

static __init int log_init(void)
{
	pr_emerg("This is an emergency message");
	pr_alert("This is an alert message");
	pr_crit("This is a critical message");
	pr_err("This is an error message");
	pr_warn("This is a warning message");
	pr_notice("This is a notice message");
	pr_info("This is an info message");
	pr_debug("This is a debug message");

	return 0;
}

static __exit void log_exit(void)
{
	pr_emerg("loglevel 0\n");
	pr_alert("loglevel 1\n");
	pr_crit("loglevel 2\n");
	pr_err("loglevel 3\n");
	pr_warn("loglevel 4\n");
	pr_notice("loglevel 5\n");
	pr_info("loglevel 6\n");
	pr_debug("loglevel 7\n");
}

module_init(log_init);
module_exit(log_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("raghu@techveda.org");
MODULE_DESCRIPTION("printk log levels");
