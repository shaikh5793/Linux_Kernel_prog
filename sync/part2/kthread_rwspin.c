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
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/sched.h>

#define MODNAME "[SYNC_RWSPINLOCK]: "
static rwlock_t rw_spinlock;
static int shared_counter = 0;

static struct task_struct *reader1, *reader2, *writer;
static bool stop_threads = false;

static int reader_fn(void *data)
{
	char *name = (char *)data;
	while (!kthread_should_stop() && !stop_threads) {
		read_lock(&rw_spinlock);
		pr_info("RW Spinlock Reader (%s): Read shared_counter=%d\n",
			name, shared_counter);
		read_unlock(&rw_spinlock);
		msleep(150);
	}
	return 0;
}

static int writer_fn(void *data)
{
	while (!kthread_should_stop() && !stop_threads) {
		write_lock(&rw_spinlock);
		shared_counter++;
		pr_info
		    ("RW Spinlock Writer: Incremented shared_counter to %d\n",
		     shared_counter);
		write_unlock(&rw_spinlock);
		msleep(500);
	}
	return 0;
}

static int __init rwlck_init(void)
{
	pr_info("RW Spinlock Example: Initializing module.\n");
	rwlock_init(&rw_spinlock);

	reader1 = kthread_run(reader_fn, "reader1", "rwspin_reader1");
	reader2 = kthread_run(reader_fn, "reader2", "rwspin_reader2");
	writer = kthread_run(writer_fn, NULL, "rwspin_writer");
	return 0;
}

static void __exit rwlck_exit(void)
{
	stop_threads = true;
	if (reader1)
		kthread_stop(reader1);
	if (reader2)
		kthread_stop(reader2);
	if (writer)
		kthread_stop(writer);
	pr_info("RW Spinlock Example: Exiting module.\n");
}

module_init(rwlck_init);
module_exit(rwlck_exit);

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("rwlocks");
MODULE_LICENSE("Dual MIT/GPL");
