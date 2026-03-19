/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/delay.h>

static void t_fun(struct tasklet_struct *);


static DECLARE_TASKLET(t_name, t_fun);

static int __init my_init(void)
{
	pr_info(" scheduling my tasklet, jiffies= %ld \n", jiffies);
	tasklet_schedule(&t_name);
	return 0;
}

static void __exit my_exit(void)
{
	pr_info("\nHello: cleanup_module loaded at address 0x%p\n",
		cleanup_module);
}

static void t_fun(struct tasklet_struct *t_arg)
{
	dump_stack();
	pr_info("Entering t_fun, ,in context of %s with pid = %d\n",
		current->comm, current->pid);
	msleep(5);
}

module_init(my_init);
module_exit(my_exit);

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_LICENSE("Dual MIT/GPL");
