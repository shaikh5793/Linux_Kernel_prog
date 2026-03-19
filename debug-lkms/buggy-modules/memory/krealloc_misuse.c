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

    if (strncmp(cmd, "use_old", 7) == 0) {
        char *p, *q;

        pr_info("krealloc_misuse: Allocating 32 bytes\n");
        p = kmalloc(32, GFP_KERNEL);
        if (!p)
            return -ENOMEM;

        sprintf(p, "hello");
        pr_info("krealloc_misuse: Before krealloc: p=%p, data=%s\n", p, p);

        q = krealloc(p, 4096, GFP_KERNEL);
        if (!q) {
            pr_err("krealloc_misuse: krealloc failed; freeing p and aborting\n");
            kfree(p);
            return -ENOMEM;
        }

        pr_info("krealloc_misuse: After krealloc: new ptr q=%p (old p=%p)\n", q, p);

        pr_info("krealloc_misuse: *** TRIGGERING BUG: Using old pointer after krealloc (potential UAF) ***\n");
        /* BUG: Use stale 'p' after krealloc may have moved data */
        pr_info("krealloc_misuse: Reading from old p: %s\n", p);
        /* BUG: Write via stale pointer (UAF/invalid) */
        sprintf(p, "stale pointer write");

        kfree(q);

    } else if (strncmp(cmd, "double_free", 11) == 0) {
        char *p, *q;

        pr_info("krealloc_misuse: Allocating 64 bytes\n");
        p = kmalloc(64, GFP_KERNEL);
        if (!p)
            return -ENOMEM;

        q = krealloc(p, 128, GFP_KERNEL);
        if (!q) {
            pr_err("krealloc_misuse: krealloc failed; freeing p and aborting\n");
            kfree(p);
            return -ENOMEM;
        }

        pr_info("krealloc_misuse: *** TRIGGERING BUG: Double free after krealloc ***\n");
        /* BUG: Free old pointer (may be invalid) then new -> double free */
        kfree(p);
        kfree(q);
    }

    return count;
}

static const struct proc_ops trigger_ops = {
    .proc_write = trigger_write,
};

static int __init krealloc_misuse_init(void)
{
    pr_info("krealloc_misuse: Module loaded\n");
    pr_info("krealloc_misuse: Write 'use_old' or 'double_free' to /proc/krealloc_trigger\n");

    proc_entry = proc_create("krealloc_trigger", 0200, NULL, &trigger_ops);
    if (!proc_entry) {
        pr_err("krealloc_misuse: Failed to create proc entry\n");
        return -ENOMEM;
    }

    return 0;
}

static void __exit krealloc_misuse_exit(void)
{
    proc_remove(proc_entry);
    pr_info("krealloc_misuse: Module unloaded\n");
}

module_init(krealloc_misuse_init);
module_exit(krealloc_misuse_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Debug Testing");
MODULE_DESCRIPTION("krealloc misuse scenarios: stale pointer use, double free");
MODULE_VERSION("1.0");
