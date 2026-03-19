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
#include <linux/spinlock.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

static struct proc_dir_entry *proc_entry;
static DEFINE_MUTEX(test_mutex);
static DEFINE_SPINLOCK(test_spinlock);

static ssize_t trigger_write(struct file *file, const char __user *buffer,
                             size_t count, loff_t *pos)
{
    char cmd[16];
    
    if (count >= sizeof(cmd))
        return -EINVAL;
    
    if (copy_from_user(cmd, buffer, count))
        return -EFAULT;
    
    cmd[count] = '\0';
    
    if (strncmp(cmd, "mutex", 5) == 0) {
        pr_info("double_unlock: *** TRIGGERING BUG: Double mutex unlock ***\n");
        
        pr_info("double_unlock: Acquiring mutex\n");
        mutex_lock(&test_mutex);
        pr_info("double_unlock: Mutex acquired\n");
        
        pr_info("double_unlock: First unlock\n");
        mutex_unlock(&test_mutex);
        
        pr_info("double_unlock: Second unlock (BUG!)\n");
        mutex_unlock(&test_mutex);
        
        pr_info("double_unlock: If you see this, lockdep didn't catch it\n");
        
    } else if (strncmp(cmd, "spinlock", 8) == 0) {
        unsigned long flags;
        
        pr_info("double_unlock: *** TRIGGERING BUG: Double spinlock unlock ***\n");
        
        pr_info("double_unlock: Acquiring spinlock\n");
        spin_lock_irqsave(&test_spinlock, flags);
        pr_info("double_unlock: Spinlock acquired\n");
        
        pr_info("double_unlock: First unlock\n");
        spin_unlock_irqrestore(&test_spinlock, flags);
        
        pr_info("double_unlock: Second unlock (BUG!)\n");
        spin_unlock_irqrestore(&test_spinlock, flags);
        
        pr_info("double_unlock: If you see this, lockdep didn't catch it\n");
        
    } else if (strncmp(cmd, "unbalanced", 10) == 0) {
        pr_info("double_unlock: *** TRIGGERING BUG: Unlock without lock ***\n");
        
        pr_info("double_unlock: Unlocking mutex without locking first\n");
        mutex_unlock(&test_mutex);
        
        pr_info("double_unlock: If you see this, lockdep didn't catch it\n");
    }
    
    return count;
}

static const struct proc_ops trigger_ops = {
    .proc_write = trigger_write,
};

static int __init double_unlock_init(void)
{
    pr_info("double_unlock: Module loaded\n");
    pr_info("double_unlock: Write 'mutex' to /proc/double_unlock_trigger for double mutex unlock\n");
    pr_info("double_unlock: Write 'spinlock' to /proc/double_unlock_trigger for double spinlock unlock\n");
    pr_info("double_unlock: Write 'unbalanced' to /proc/double_unlock_trigger for unlock without lock\n");
    
    proc_entry = proc_create("double_unlock_trigger", 0200, NULL, &trigger_ops);
    if (!proc_entry) {
        pr_err("double_unlock: Failed to create proc entry\n");
        return -ENOMEM;
    }
    
    return 0;
}

static void __exit double_unlock_exit(void)
{
    proc_remove(proc_entry);
    pr_info("double_unlock: Module unloaded\n");
}

module_init(double_unlock_init);
module_exit(double_unlock_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Debug Testing");
MODULE_DESCRIPTION("Double Unlock Bug Module for Lockdep Testing");
MODULE_VERSION("1.0");
