/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/delay.h>

static struct proc_dir_entry *proc_entry;
static DEFINE_MUTEX(lock_a);
static DEFINE_MUTEX(lock_b);
static struct task_struct *thread1;
static struct task_struct *thread2;

static int thread1_fn(void *data)
{
    pr_info("deadlock: Thread 1 starting\n");
    msleep(100);
    
    pr_info("deadlock: Thread 1 acquiring lock_a\n");
    mutex_lock(&lock_a);
    pr_info("deadlock: Thread 1 got lock_a\n");
    
    msleep(200); /* Give thread2 time to acquire lock_b */
    
    pr_info("deadlock: Thread 1 trying to acquire lock_b (potential deadlock)\n");
    mutex_lock(&lock_b);
    pr_info("deadlock: Thread 1 got lock_b\n");
    
    mutex_unlock(&lock_b);
    mutex_unlock(&lock_a);
    
    pr_info("deadlock: Thread 1 finished\n");
    return 0;
}

static int thread2_fn(void *data)
{
    pr_info("deadlock: Thread 2 starting\n");
    msleep(150);
    
    pr_info("deadlock: Thread 2 acquiring lock_b\n");
    mutex_lock(&lock_b);
    pr_info("deadlock: Thread 2 got lock_b\n");
    
    msleep(200); /* Give thread1 time to try lock_b */
    
    pr_info("deadlock: Thread 2 trying to acquire lock_a (potential deadlock)\n");
    mutex_lock(&lock_a);
    pr_info("deadlock: Thread 2 got lock_a\n");
    
    mutex_unlock(&lock_a);
    mutex_unlock(&lock_b);
    
    pr_info("deadlock: Thread 2 finished\n");
    return 0;
}

static ssize_t trigger_write(struct file *file, const char __user *buffer,
                             size_t count, loff_t *pos)
{
    char cmd[16];
    
    if (count >= sizeof(cmd))
        return -EINVAL;
    
    if (copy_from_user(cmd, buffer, count))
        return -EFAULT;
    
    cmd[count] = '\0';
    
    if (strncmp(cmd, "trigger", 7) == 0) {
        pr_info("deadlock: *** TRIGGERING BUG: Potential deadlock (ABBA) ***\n");
        pr_info("deadlock: Creating threads with inverse lock ordering\n");
        
        thread1 = kthread_run(thread1_fn, NULL, "deadlock_t1");
        if (IS_ERR(thread1)) {
            pr_err("deadlock: Failed to create thread1\n");
            return PTR_ERR(thread1);
        }
        
        thread2 = kthread_run(thread2_fn, NULL, "deadlock_t2");
        if (IS_ERR(thread2)) {
            pr_err("deadlock: Failed to create thread2\n");
            kthread_stop(thread1);
            return PTR_ERR(thread2);
        }
        
        pr_info("deadlock: Threads created. Check dmesg for lockdep warnings\n");
        
    } else if (strncmp(cmd, "self", 4) == 0) {
        pr_info("deadlock: *** TRIGGERING BUG: Self deadlock ***\n");
        pr_info("deadlock: Acquiring same lock twice\n");
        
        mutex_lock(&lock_a);
        pr_info("deadlock: First lock acquired\n");
        
        pr_info("deadlock: Trying to acquire same lock again (will hang)\n");
        mutex_lock(&lock_a); /* This will deadlock */
        
        mutex_unlock(&lock_a);
        mutex_unlock(&lock_a);
    }
    
    return count;
}

static const struct proc_ops trigger_ops = {
    .proc_write = trigger_write,
};

static int __init deadlock_init(void)
{
    pr_info("deadlock: Module loaded\n");
    pr_info("deadlock: Write 'trigger' to /proc/deadlock_trigger for ABBA deadlock\n");
    pr_info("deadlock: Write 'self' to /proc/deadlock_trigger for self deadlock (WARNING: will hang!)\n");
    
    proc_entry = proc_create("deadlock_trigger", 0200, NULL, &trigger_ops);
    if (!proc_entry) {
        pr_err("deadlock: Failed to create proc entry\n");
        return -ENOMEM;
    }
    
    return 0;
}

static void __exit deadlock_exit(void)
{
    /* Note: If threads are still running, we should wait for them */
    if (thread1 && !IS_ERR(thread1))
        kthread_stop(thread1);
    if (thread2 && !IS_ERR(thread2))
        kthread_stop(thread2);
    
    proc_remove(proc_entry);
    pr_info("deadlock: Module unloaded\n");
}

module_init(deadlock_init);
module_exit(deadlock_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Debug Testing");
MODULE_DESCRIPTION("Deadlock Bug Module for Lockdep Testing");
MODULE_VERSION("1.0");
