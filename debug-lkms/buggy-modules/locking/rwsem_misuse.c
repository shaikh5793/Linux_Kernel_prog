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
#include <linux/rwsem.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

static struct proc_dir_entry *proc_entry;
static DECLARE_RWSEM(test_sem);
static DEFINE_SPINLOCK(holder);

static ssize_t trigger_write(struct file *file, const char __user *buffer,
                             size_t count, loff_t *ppos)
{
    char cmd[24];
    unsigned long flags;

    if (count >= sizeof(cmd))
        return -EINVAL;
    if (copy_from_user(cmd, buffer, count))
        return -EFAULT;
    cmd[count] = '\0';

    if (strncmp(cmd, "down_in_spin", 12) == 0) {
        pr_info("rwsem_misuse: *** TRIGGERING BUG: Sleeping rwsem while holding spinlock ***\n");
        spin_lock_irqsave(&holder, flags);
        pr_info("rwsem_misuse: spinlock acquired\n");
        /* BUG: May sleep while holding spinlock (invalid context) */
        down_read(&test_sem);
        up_read(&test_sem);
        spin_unlock_irqrestore(&holder, flags);
        pr_info("rwsem_misuse: spinlock released\n");
    }

    return count;
}

static const struct proc_ops trigger_ops = {
    .proc_write = trigger_write,
};

static int __init rwsem_misuse_init(void)
{
    pr_info("rwsem_misuse: Module loaded\n");
    pr_info("rwsem_misuse: Write 'down_in_spin' to /proc/rwsem_misuse_trigger\n");
    proc_entry = proc_create("rwsem_misuse_trigger", 0200, NULL, &trigger_ops);
    if (!proc_entry)
        return -ENOMEM;
    return 0;
}

static void __exit rwsem_misuse_exit(void)
{
    proc_remove(proc_entry);
    pr_info("rwsem_misuse: Module unloaded\n");
}

module_init(rwsem_misuse_init);
module_exit(rwsem_misuse_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Debug Testing");
MODULE_DESCRIPTION("RW semaphore misuse to trigger atomic sleep");
MODULE_VERSION("1.0");
