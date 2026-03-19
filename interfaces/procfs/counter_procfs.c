/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 *
 * ProcFS Interface Example - Counter Service
 * Purpose: Demonstrates user-kernel communication via /proc filesystem
 * Usage: Read/write operations through /proc/counter_service
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#define PROC_NAME "counter_service"

static int counter = 0;
static struct proc_dir_entry *proc_entry;

/*
 * counter_show() - Display current counter value to user
 * Called by: proc read operations when user reads from /proc/counter_service
 * Purpose: Formats counter value for display in proc filesystem
 */
static int counter_show(struct seq_file *m, void *v)
{
    seq_printf(m, "%d\n", counter);
    return 0;
}

/*
 * counter_open() - Handle proc file open operations
 * Called by: VFS when /proc/counter_service is opened
 * Purpose: Initializes sequential file operations for reading
 */
static int counter_open(struct inode *inode, struct file *file)
{
    return single_open(file, counter_show, NULL);
}

/*
 * counter_write() - Handle write operations to update counter
 * Called by: VFS when user writes to /proc/counter_service
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

static const struct proc_ops counter_proc_ops = {
    .proc_open = counter_open,
    .proc_read = seq_read,
    .proc_write = counter_write,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static int __init counter_init(void)
{
    proc_entry = proc_create(PROC_NAME, 0666, NULL, &counter_proc_ops);
    if (!proc_entry) {
        pr_err("Failed to create /proc/%s\n", PROC_NAME);
        return -ENOMEM;
    }

    pr_info("Counter ProcFS module loaded: /proc/%s\n", PROC_NAME);
    return 0;
}

static void __exit counter_exit(void)
{
    proc_remove(proc_entry);
    pr_info("Counter ProcFS module unloaded\n");
}

module_init(counter_init);
module_exit(counter_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("User-Kernel Interface Example");
MODULE_DESCRIPTION("Counter service via ProcFS interface");
MODULE_VERSION("1.0");
