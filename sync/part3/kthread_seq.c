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
#include <linux/seqlock.h>
#include <linux/kthread.h>
#include <linux/sched.h>

#define MODNAME "[SEQLOCK]: "


unsigned int counter;

DEFINE_SEQLOCK(my_seq_lock);

struct task_struct *read_thread1, *read_thread2, *read_thread3, *write_thread;

static int writer_function(void *data)
{
	while (!kthread_should_stop()) {
		write_seqlock(&my_seq_lock);
		counter++;
		write_sequnlock(&my_seq_lock);
		msleep(500);
	}
	return 0;
}

static int read_function(void *data)
{

	unsigned long seq;
	while (!kthread_should_stop()) {
		do {
			seq = read_seqbegin(&my_seq_lock);
			pr_info("%s:counter: %d\n", __func__, counter);
		} while (read_seqretry(&my_seq_lock, seq));
		msleep(500);
	}
	return 0;
}

static int __init my_mod_init(void)
{
	pr_info(MODNAME "Entering module.\n");
	counter = 0;
	read_thread1 = kthread_run(read_function, NULL, "read-thread1");
	read_thread2 = kthread_run(read_function, NULL, "read-thread2");
	read_thread3 = kthread_run(read_function, NULL, "read-thread3");
	write_thread = kthread_run(writer_function, NULL, "write-thread");
	return 0;
}

static void __exit my_mod_exit(void)
{
	kthread_stop(read_thread3);
	kthread_stop(read_thread2);
	kthread_stop(read_thread1);
	kthread_stop(write_thread);
	pr_info(MODNAME "Exiting module.\n");
}

module_init(my_mod_init);
module_exit(my_mod_exit);

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("seqlocks");
MODULE_LICENSE("Dual MIT/GPL");
