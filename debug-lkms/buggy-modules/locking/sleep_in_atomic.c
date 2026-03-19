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
#include <linux/spinlock.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/delay.h>

static struct proc_dir_entry *proc_entry;
static DEFINE_SPINLOCK(sleep_lock);

static ssize_t trigger_write(struct file *file, const char __user *buffer,
                             size_t count, loff_t *ppos)
{
    char cmd[16];
    unsigned long flags;

    if (count >= sizeof(cmd))
        return -EINVAL;
    if (copy_from_user(cmd, buffer, count))
        return -EFAULT;
    cmd[count] = '\0';

    if (strncmp(cmd, "spin_sleep", 10) == 0) {
        pr_info("sleep_in_atomic: *** TRIGGERING BUG: Sleep while holding spinlock ***\n");
        spin_lock_irqsave(&sleep_lock, flags);
        pr_info("sleep_in_atomic: spinlock acquired\n");
        /* BUG: Sleeping while holding spinlock (atomic context) */
        msleep(10);
        spin_unlock_irqrestore(&sleep_lock, flags);
        pr_info("sleep_in_atomic: spinlock released\n");
    }

    return count;
}

static const struct proc_ops trigger_ops = {
    .proc_write = trigger_write,
};

static int __init sleep_in_atomic_init(void)
{
    pr_info("sleep_in_atomic: Module loaded\n");
    pr_info("sleep_in_atomic: Write 'spin_sleep' to /proc/sleep_in_atomic_trigger\n");

    proc_entry = proc_create("sleep_in_atomic_trigger", 0200, NULL, &trigger_ops);
    if (!proc_entry)
        return -ENOMEM;
    return 0;
}

static void __exit sleep_in_atomic_exit(void)
{
    proc_remove(proc_entry);
    pr_info("sleep_in_atomic: Module unloaded\n");
}

module_init(sleep_in_atomic_init);
module_exit(sleep_in_atomic_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Debug Testing");
MODULE_DESCRIPTION("Sleep-in-atomic test for lock debugging");
MODULE_VERSION("1.0");
