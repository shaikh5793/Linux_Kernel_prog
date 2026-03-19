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
#include <linux/mutex.h>
#include <linux/slab.h>

static DEFINE_MUTEX(my_mutex);
static int shared_resource = 0;
static struct task_struct *thread1, *thread2;
static bool stop_threads = false;

static int worker_thread(void *data)
{
    while (!kthread_should_stop() && !stop_threads) {
        if (mutex_lock_interruptible(&my_mutex))
            break;
        shared_resource++;
        pr_info("%s acquired mutex, shared_resource=%d\n",
                (char *)data, shared_resource);
        msleep(100); /* Simulate work */
        mutex_unlock(&my_mutex);
        ssleep(1);   /* Sleep outside the lock */
    }
    return 0;
}

static int __init mtxtest_init(void)
{
    pr_info("Mutex Example: Initializing module.\n");

    thread1 = kthread_run(worker_thread, "Thread1", "mutex_thread1");
    thread2 = kthread_run(worker_thread, "Thread2", "mutex_thread2");

    return 0;
}

static void __exit mtxtest_exit(void)
{
    pr_info("Mutex Example: Stopping threads.\n");
    stop_threads = true;
    if (thread1)
        kthread_stop(thread1);
    if (thread2)
        kthread_stop(thread2);
    pr_info("Mutex Example: Exiting module.\n");
}

module_init(mtxtest_init);
module_exit(mtxtest_exit);

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("Kernel Mutex Interface");
MODULE_LICENSE("Dual MIT/GPL");
