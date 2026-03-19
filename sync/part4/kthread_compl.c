/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
*/



#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/completion.h>

#define MODNAME "[SYNC_COMP]: "


unsigned int counter;

struct completion *comp;
struct task_struct *read_thread, *write_thread;

static int writer_function(void *data)
{
	while (counter != 1234)
		counter++;
	complete_all(comp); //wakeup
	return 0;
}

static int read_function(void *data)
{
	wait_for_completion(comp); //wait
	pr_info(MODNAME "counter: %d\n", counter);
	return 0;
}

static int __init my_mod_init(void)
{
	counter = 0;
	comp = kmalloc(sizeof(struct completion), GFP_KERNEL);
	if (!comp)
		return -1;
	init_completion(comp);
	read_thread = kthread_run(read_function, NULL, "read-thread");
	write_thread = kthread_run(writer_function, NULL, "write-thread");
	return 0;
}

static void __exit my_mod_exit(void)
{
	kfree(comp);
}

module_init(my_mod_init);
module_exit(my_mod_exit);

MODULE_AUTHOR("Raghu Bharadwaj<raghu@techveda.org>");
MODULE_DESCRIPTION("completions");
MODULE_LICENSE("Dual MIT/GPL");
