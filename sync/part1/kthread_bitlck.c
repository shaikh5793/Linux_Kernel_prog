/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/kthread.h>

static unsigned long bitlock_word = 0; /* The lock bit will be stored here */
#define BITLOCK_BIT 0

static struct task_struct *bitlock_thread1, *bitlock_thread2;
static bool stop_threads = false;
static int shared_data = 0;

static int bitlock_fn(void *name)
{
    while (!kthread_should_stop() && !stop_threads) {
        /* Acquire bit lock */
        bit_spin_lock(BITLOCK_BIT, &bitlock_word);
        shared_data++;
        pr_info("Bitlock (%s): Incremented shared_data to %d\n",
                (char *)name, shared_data);

        /* Simulate small critical section */
        udelay(50);

        /* Release bit lock */
        bit_spin_unlock(BITLOCK_BIT, &bitlock_word);
        msleep(100);
    }
    return 0;
}

static int __init bittest_init(void)
{
    pr_info("Bitlock Example: Initializing module.\n");
    bitlock_thread1 = kthread_run(bitlock_fn, "Thread1", "bitlock_thread1");
    bitlock_thread2 = kthread_run(bitlock_fn, "Thread2", "bitlock_thread2");
    return 0;
}

static void __exit bittest_exit(void)
{
    stop_threads = true;
    if (bitlock_thread1)
        kthread_stop(bitlock_thread1);
    if (bitlock_thread2)
        kthread_stop(bitlock_thread2);
    pr_info("Bitlock Example: Exiting module.\n");
}

module_init(bittest_init);
module_exit(bittest_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("Bitlock Example Module");
