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
#include <linux/rwlock.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

static struct proc_dir_entry *proc_entry;
static DEFINE_RWLOCK(test_rwlock);

static ssize_t trigger_write(struct file *file, const char __user *buffer,
                             size_t count, loff_t *ppos)
{
    char cmd[16];

    if (count >= sizeof(cmd))
        return -EINVAL;
    if (copy_from_user(cmd, buffer, count))
        return -EFAULT;
    cmd[count] = '\0';

    if (strncmp(cmd, "upgrade", 7) == 0) {
        pr_info("rwlock_misuse: *** TRIGGERING BUG: Illegal read->write upgrade ***\n");
        read_lock(&test_rwlock);
        /* BUG: Attempting to upgrade read lock to write lock */
        write_lock(&test_rwlock);
        write_unlock(&test_rwlock);
        read_unlock(&test_rwlock);
    } else if (strncmp(cmd, "double_write", 12) == 0) {
        pr_info("rwlock_misuse: *** TRIGGERING BUG: Recursive writer deadlock ***\n");
        write_lock(&test_rwlock);
        /* BUG: Taking write_lock twice on same CPU */
        write_lock(&test_rwlock);
        write_unlock(&test_rwlock);
        write_unlock(&test_rwlock);
    }

    return count;
}

static const struct proc_ops trigger_ops = {
    .proc_write = trigger_write,
};

static int __init rwlock_misuse_init(void)
{
    pr_info("rwlock_misuse: Module loaded\n");
    pr_info("rwlock_misuse: Commands at /proc/rwlock_misuse_trigger: 'upgrade', 'double_write'\n");
    proc_entry = proc_create("rwlock_misuse_trigger", 0200, NULL, &trigger_ops);
    if (!proc_entry)
        return -ENOMEM;
    return 0;
}

static void __exit rwlock_misuse_exit(void)
{
    proc_remove(proc_entry);
    pr_info("rwlock_misuse: Module unloaded\n");
}

module_init(rwlock_misuse_init);
module_exit(rwlock_misuse_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Debug Testing");
MODULE_DESCRIPTION("RW lock misuse examples for lockdep testing");
MODULE_VERSION("1.0");
