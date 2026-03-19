/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/atomic.h>
#include <linux/kthread.h>
#include <linux/delay.h>

static atomic_t counter = ATOMIC_INIT(0);
static struct task_struct *increaser_thread, *checker_thread;
static bool stop_threads = false;

static int increaser_fn(void *data)
{
    while (!kthread_should_stop() && !stop_threads) {
        atomic_inc(&counter);
        pr_info("Atomic test: Incremented, value=%d\n", atomic_read(&counter));
        msleep(100);
    }
    return 0;
}

static int checker_fn(void *data)
{
    while (!kthread_should_stop() && !stop_threads) {
        int val = atomic_read(&counter);
        if (val > 50) {
            /* Try to reset it atomically */
            int old = atomic_cmpxchg(&counter, val, 0);
            if (old == val)
                pr_info("Atomic test: Reset atomic_counter from %d to 0\n", old);
        }
        msleep(200);
    }
    return 0;
}

static int __init at_init(void)
{
    pr_info("Atomic test: Initializing module.\n");
    increaser_thread = kthread_run(increaser_fn, NULL, "atomic_increaser");
    checker_thread = kthread_run(checker_fn, NULL, "atomic_checker");
    return 0;
}

static void __exit at_exit(void)
{
    stop_threads = true;
    if (increaser_thread)
        kthread_stop(increaser_thread);
    if (checker_thread)
        kthread_stop(checker_thread);
    pr_info("Atomic test: Exiting module.\n");
}

module_init(at_init);
module_exit(at_exit);


MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Raghu Bharadwaj<raghu@techveda.org>");
MODULE_DESCRIPTION("Atomic Operations");
