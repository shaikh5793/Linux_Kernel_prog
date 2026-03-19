/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/delay.h>


DEFINE_RAW_SPINLOCK(my_lock);
static int my_global = 0;
static struct task_struct *thread1;
static struct task_struct *thread2;

static int thread_fn1(void *data) {
    while (!kthread_should_stop()) {
        raw_spin_lock(&my_lock);
        my_global++;
        raw_spin_unlock(&my_lock);
        msleep(1000);  // sleep for a while
    }
    return 0;
}

static int thread_fn2(void *data) {
    while (!kthread_should_stop()) {
        raw_spin_lock(&my_lock);
        pr_info("my_global: %d\n", my_global);
        raw_spin_unlock(&my_lock);
        msleep(1000);  // sleep for a while
    }
    return 0;
}

static int __init my_module_init(void) {
    pr_info("my_module loaded\n");
    thread1 = kthread_run(thread_fn1, NULL, "my_module_thread1");
    thread2 = kthread_run(thread_fn2, NULL, "my_module_thread2");
    return 0;
}

static void __exit my_module_exit(void) {
    pr_info("my_module unloaded\n");
    if (thread1) {
        kthread_stop(thread1);
    }
    if (thread2) {
        kthread_stop(thread2);
    }
}

module_init(my_module_init);
module_exit(my_module_exit);


MODULE_LICENSE("Dual MIT/GPL");
