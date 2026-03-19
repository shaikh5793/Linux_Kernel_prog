/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ww_mutex.h>
#include <linux/kthread.h>
#include <linux/delay.h>


/* Define our own ww_class */
static DEFINE_WW_CLASS(my_ww_class);

static struct ww_mutex obj1_lock;
static struct ww_mutex obj2_lock;

static struct task_struct *thr_a, *thr_b;
static bool stop_threads = false;

static int ww_thread_fn(void *data)
{
    struct ww_acquire_ctx ctx;
    struct ww_mutex *first = &obj1_lock;
    struct ww_mutex *second = &obj2_lock;
    char *name = (char *)data;
    int ret;

    ww_acquire_init(&ctx, &my_ww_class);
    while (!kthread_should_stop() && !stop_threads) {
        /* Try to lock obj1 */
        ret = ww_mutex_lock_interruptible(first, &ctx);
        if (ret == 0) {
            /* Now try to lock obj2 */
            ret = ww_mutex_lock_interruptible(second, &ctx);
            if (ret == -EDEADLK) {
                /* Deadlock avoidance: unlock first and retry all */
                ww_mutex_unlock(first);
                ret = ww_mutex_lock_slow_interruptible(second, &ctx);
                if (!ret) {
                    /* Successfully got second, now try first again */
                    ret = ww_mutex_lock_slow_interruptible(first, &ctx);
                    if (ret) {
                        /* Failed to reacquire first, just unlock second */
                        ww_mutex_unlock(second);
                    }
                }
            }

            if (!ret) {
                pr_info("%s: Acquired both locks.\n", name);
                msleep(100);
                ww_mutex_unlock(second);
                ww_mutex_unlock(first);
            } else {
                pr_info("%s: Failed to acquire both locks.\n", name);
            }
        }
        msleep(200);
    }

    ww_acquire_fini(&ctx);
    return 0;
}

static int __init wwtest_init(void)
{
    pr_info("WW Mutex Example: Initializing module.\n");

    ww_mutex_init(&obj1_lock, &my_ww_class);
    ww_mutex_init(&obj2_lock, &my_ww_class);

    thr_a = kthread_run(ww_thread_fn, "ThreadA", "ww_threadA");
    thr_b = kthread_run(ww_thread_fn, "ThreadB", "ww_threadB");

    return 0;
}

static void __exit wwtest_exit(void)
{
    stop_threads = true;
    if (thr_a)
        kthread_stop(thr_a);
    if (thr_b)
        kthread_stop(thr_b);

    ww_mutex_destroy(&obj1_lock);
    ww_mutex_destroy(&obj2_lock);

    pr_info("WW Mutex Example: Exiting module.\n");
}

module_init(wwtest_init);
module_exit(wwtest_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("WW Mutex Example Module");
