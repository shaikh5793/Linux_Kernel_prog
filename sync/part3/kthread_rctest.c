/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/random.h>
#include <linux/rcupdate.h> // for rcu_dereference, rcu_assign_pointer, etc.

struct rcu_data {
    int value;
    struct rcu_head rcu;
};

static struct rcu_data *rcu_ptr;
static struct task_struct *updater_task, *reader_task;
static bool stop_threads = false;


static int updater_thread(void *data)
{
    struct rcu_data *new_data, *old_data;

    while (!kthread_should_stop() && !stop_threads) {
        new_data = kmalloc(sizeof(*new_data), GFP_KERNEL);
        if (!new_data)
            continue;

        new_data->value = get_random_u32() & 0xff;

        // Atomically replace rcu_ptr and get the old pointer
        old_data = rcu_ptr;
        rcu_assign_pointer(rcu_ptr, new_data);

        pr_info("Updater: Wrote %d\n", new_data->value);

        // Wait for a grace period before freeing old data
        if (old_data) {
            synchronize_rcu();
            kfree(old_data);
        }

        msleep(1000);
    }
    return 0;
}

static int reader_thread(void *data)
{
    struct rcu_data *cur_data;

    while (!kthread_should_stop() && !stop_threads) {
        rcu_read_lock();
        cur_data = rcu_dereference(rcu_ptr);
        if (cur_data)
            pr_info("Reader: Read %d\n", cur_data->value);
        rcu_read_unlock();
        msleep(500);
    }
    return 0;
}

static int __init rcu_test_init(void)
{
    pr_info("RCU Example: Initializing.\n");
    updater_task = kthread_run(updater_thread, NULL, "rcu_updater");
    reader_task = kthread_run(reader_thread, NULL, "rcu_reader");
    return 0;
}

static void __exit rcu_test_exit(void)
{
    stop_threads = true;
    if (updater_task)
        kthread_stop(updater_task);
    if (reader_task)
        kthread_stop(reader_task);

    kfree(rcu_ptr);
    pr_info("RCU Example: Exiting.\n");
}

module_init(rcu_test_init);
module_exit(rcu_test_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("RCU Example Module");
