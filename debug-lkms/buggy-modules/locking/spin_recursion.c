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

static struct proc_dir_entry *proc_entry;
static DEFINE_SPINLOCK(rec_lock);

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

    if (strncmp(cmd, "self", 4) == 0) {
        pr_info("spin_recursion: *** TRIGGERING BUG: Recursive spin_lock deadlock ***\n");
        spin_lock_irqsave(&rec_lock, flags);
        pr_info("spin_recursion: first lock acquired\n");
        /* BUG: Taking the same spinlock twice (non-recursive) */
        spin_lock_irqsave(&rec_lock, flags);
        pr_info("spin_recursion: second lock acquired (unexpected)\n");
        spin_unlock_irqrestore(&rec_lock, flags);
        spin_unlock_irqrestore(&rec_lock, flags);
    }

    return count;
}

static const struct proc_ops trigger_ops = {
    .proc_write = trigger_write,
};

static int __init spin_recursion_init(void)
{
    pr_info("spin_recursion: Module loaded\n");
    pr_info("spin_recursion: Write 'self' to /proc/spin_recursion_trigger\n");
    proc_entry = proc_create("spin_recursion_trigger", 0200, NULL, &trigger_ops);
    if (!proc_entry)
        return -ENOMEM;
    return 0;
}

static void __exit spin_recursion_exit(void)
{
    proc_remove(proc_entry);
    pr_info("spin_recursion: Module unloaded\n");
}

module_init(spin_recursion_init);
module_exit(spin_recursion_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Debug Testing");
MODULE_DESCRIPTION("Recursive spinlock misuse for lockdep testing");
MODULE_VERSION("1.0");
