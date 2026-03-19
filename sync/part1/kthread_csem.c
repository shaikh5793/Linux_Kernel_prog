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
#include <linux/semaphore.h>
#include <linux/slab.h>

/* We allow up to 3 concurrent accesses to the shared resource */
static struct semaphore sem;
static struct task_struct *threads[5];
static bool stop_threads = false;
static int shared_var = 0;

static int sem_worker_thread(void *data)
{
    while (!kthread_should_stop() && !stop_threads) {
        if (down_interruptible(&sem))
            break;
        shared_var++;
        pr_info("%s acquired semaphore, shared_var=%d\n",
                (char *)data, shared_var);
        msleep(100); /* Simulated work */
        up(&sem);
        ssleep(1);
    }
    return 0;
}

static int __init sem_init(void)
{
    int i;

    pr_info("Semaphore Example: Initializing module.\n");
    sema_init(&sem,1); /* Initialize counting semaphore with count=3 */

    for (i = 0; i < 5; i++) {
        char *name;
        name = kasprintf(GFP_KERNEL, "sem_thread%d", i);
        threads[i] = kthread_run(sem_worker_thread, name, name);
    }

    return 0;
}

static void __exit sem_exit(void)
{
    int i;
    stop_threads = true;
    for (i = 0; i < 5; i++) {
        if (threads[i])
            kthread_stop(threads[i]);
    }
    pr_info("Semaphore Example: Exiting module.\n");
}

module_init(sem_init);
module_exit(sem_exit);

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("Kernel semaphore Interface");
MODULE_LICENSE("Dual MIT/GPL");
