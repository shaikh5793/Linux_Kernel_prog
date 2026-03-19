/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/rtmutex.h>
#include <linux/kthread.h>
#include <linux/delay.h>

/* Real-Time Mutex */
static struct rt_mutex rt_lock;
static struct task_struct *rt_thr1, *rt_thr2;
static bool stop_threads = false;

static int rt_thread_func(void *data)
{
    char *name = (char *)data;
    while (!kthread_should_stop() && !stop_threads) {
        rt_mutex_lock(&rt_lock);
        pr_info("RT Mutex (%s): Acquired lock.\n", name);
        msleep(100);
        rt_mutex_unlock(&rt_lock);
        pr_info("RT Mutex (%s): Released lock.\n", name);
        ssleep(1);
    }
    return 0;
}

static int __init rtm_init(void)
{
    pr_info("RT Mutex Example: Initializing module.\n");
    rt_mutex_init(&rt_lock);

    rt_thr1 = kthread_run(rt_thread_func, "RT_Thread1", "rt_thread1");
    rt_thr2 = kthread_run(rt_thread_func, "RT_Thread2", "rt_thread2");

    return 0;
}

static void __exit rtm_exit(void)
{
    stop_threads = true;
    if (rt_thr1)
        kthread_stop(rt_thr1);
    if (rt_thr2)
        kthread_stop(rt_thr2);

    pr_info("RT Mutex Example: Exiting module.\n");
}

module_init(rtm_init);
module_exit(rtm_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("RT Mutex Example Module");
