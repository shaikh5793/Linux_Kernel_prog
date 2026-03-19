/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 *
 * DebugFS Interface Example - Counter Service
 * Purpose: Demonstrates user-kernel communication via /sys/kernel/debug filesystem
 * Usage: Read/write operations through /sys/kernel/debug/counter/value
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

static int counter = 0;
static struct dentry *debugfs_dir;
static struct dentry *debugfs_file;

/*
 * counter_show() - Display current counter value through debugfs
 * Called by: debugfs read operations when user reads from /sys/kernel/debug/counter/value
 * Purpose: Formats counter value for display in debugfs entry
 */
static int counter_show(struct seq_file *m, void *v)
{
    seq_printf(m, "%d\n", counter);
    return 0;
}

/*
 * counter_open() - Handle debugfs file open operations
 * Called by: VFS when /sys/kernel/debug/counter/value is opened
 * Purpose: Initializes sequential file operations for reading
 */
static int counter_open(struct inode *inode, struct file *file)
{
    return single_open(file, counter_show, NULL);
}

/*
 * counter_write() - Handle write operations to update counter via debugfs
 * Called by: VFS when user writes to /sys/kernel/debug/counter/value
 * Purpose: Parses user input to increment (1) or reset (0) counter
 */
static ssize_t counter_write(struct file *file, const char __user *buffer,
                            size_t count, loff_t *pos)
{
    char input[32];
    int value;

    if (count >= sizeof(input))
        return -EINVAL;

    if (copy_from_user(input, buffer, count))
        return -EFAULT;

    input[count] = '\0';

    if (sscanf(input, "%d", &value) != 1)
        return -EINVAL;

    if (value == 0)
        counter = 0;  /* reset */
    else if (value == 1)
        counter++;    /* increment */
    else
        return -EINVAL;

    return count;
}

static const struct file_operations counter_fops = {
    .open = counter_open,
    .read = seq_read,
    .write = counter_write,
    .llseek = seq_lseek,
    .release = single_release,
};

static int __init counter_init(void)
{
    debugfs_dir = debugfs_create_dir("counter", NULL);
    if (!debugfs_dir) {
        pr_err("Failed to create debugfs directory\n");
        return -ENOMEM;
    }

    debugfs_file = debugfs_create_file("value", 0666, debugfs_dir, NULL, &counter_fops);
    if (!debugfs_file) {
        pr_err("Failed to create debugfs file\n");
        debugfs_remove_recursive(debugfs_dir);
        return -ENOMEM;
    }

    pr_info("Counter DebugFS module loaded: /sys/kernel/debug/counter/value\n");
    return 0;
}

static void __exit counter_exit(void)
{
    debugfs_remove_recursive(debugfs_dir);
    pr_info("Counter DebugFS module unloaded\n");
}

module_init(counter_init);
module_exit(counter_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("User-Kernel Interface Example");
MODULE_DESCRIPTION("Counter service via DebugFS interface");
MODULE_VERSION("1.0");
