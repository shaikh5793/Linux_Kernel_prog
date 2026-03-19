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
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

static struct proc_dir_entry *proc_entry;

static ssize_t trigger_write(struct file *file, const char __user *buffer,
                             size_t count, loff_t *pos)
{
    char cmd[16];

    if (count >= sizeof(cmd))
        return -EINVAL;

    if (copy_from_user(cmd, buffer, count))
        return -EFAULT;

    cmd[count] = '\0';

    if (strncmp(cmd, "overflow", 8) == 0) {
        char *buf;
        int i;
        volatile unsigned long sum = 0;

        pr_info("oob_read: Allocating 64 bytes\n");
        buf = kmalloc(64, GFP_KERNEL);
        if (!buf)
            return -ENOMEM;

        pr_info("oob_read: *** TRIGGERING BUG: Out-of-bounds read (overflow) ***\n");
        pr_info("oob_read: Reading 64 bytes beyond end\n");

        /* BUG: Read past end of allocation (heap OOB read) */
        for (i = 64; i < 128; i++)
            sum += ((unsigned char *)buf)[i];

        pr_info("oob_read: Sum=%lu (dummy)\n", sum);
        kfree(buf);

    } else if (strncmp(cmd, "underflow", 9) == 0) {
        char *buf;
        volatile unsigned long val;

        pr_info("oob_read: Allocating 64 bytes\n");
        buf = kmalloc(64, GFP_KERNEL);
        if (!buf)
            return -ENOMEM;

        pr_info("oob_read: *** TRIGGERING BUG: Out-of-bounds read (underflow) ***\n");
        pr_info("oob_read: Reading before buffer start\n");

        /* BUG: Read using negative indices (before allocation start) */
        val = ((unsigned char *)buf)[-1] + ((unsigned char *)buf)[-16];
        pr_info("oob_read: Read dummy val=%lu\n", val);
        kfree(buf);
    }

    return count;
}

static const struct proc_ops trigger_ops = {
    .proc_write = trigger_write,
};

static int __init oob_read_init(void)
{
    pr_info("oob_read: Module loaded\n");
    pr_info("oob_read: Write 'overflow' or 'underflow' to /proc/oob_read_trigger\n");

    proc_entry = proc_create("oob_read_trigger", 0200, NULL, &trigger_ops);
    if (!proc_entry) {
        pr_err("oob_read: Failed to create proc entry\n");
        return -ENOMEM;
    }

    return 0;
}

static void __exit oob_read_exit(void)
{
    proc_remove(proc_entry);
    pr_info("oob_read: Module unloaded\n");
}

module_init(oob_read_init);
module_exit(oob_read_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Debug Testing");
MODULE_DESCRIPTION("Out-of-Bounds Read (overflow/underflow) for KASAN Testing");
MODULE_VERSION("1.0");
