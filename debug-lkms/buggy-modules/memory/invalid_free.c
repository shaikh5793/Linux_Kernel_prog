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
static char global_buf[64]; /* Not from kmalloc */

static ssize_t trigger_write(struct file *file, const char __user *buffer,
                             size_t count, loff_t *pos)
{
    char cmd[16];

    if (count >= sizeof(cmd))
        return -EINVAL;

    if (copy_from_user(cmd, buffer, count))
        return -EFAULT;

    cmd[count] = '\0';

    if (strncmp(cmd, "stack", 5) == 0) {
        char stack_buf[32];

        pr_info("invalid_free: *** TRIGGERING BUG: kfree() on stack pointer ***\n");
        pr_info("invalid_free: Attempting kfree() on &stack_buf (%p)\n", stack_buf);
        /* BUG: Freeing stack memory with kfree() */
        kfree(stack_buf);

    } else if (strncmp(cmd, "global", 6) == 0) {
        pr_info("invalid_free: *** TRIGGERING BUG: kfree() on global/static pointer ***\n");
        pr_info("invalid_free: Attempting kfree() on global_buf (%p)\n", global_buf);
        /* BUG: Freeing non-heap static/global memory */
        kfree(global_buf);

    } else if (strncmp(cmd, "offset", 6) == 0) {
        char *ptr;

        pr_info("invalid_free: Allocating memory\n");
        ptr = kmalloc(64, GFP_KERNEL);
        if (!ptr)
            return -ENOMEM;

        pr_info("invalid_free: *** TRIGGERING BUG: kfree() on interior pointer ***\n");
        pr_info("invalid_free: Freeing ptr+8 (ptr=%p)\n", ptr);
        /* BUG: Free interior pointer (not start of allocation) */
        kfree(ptr + 8);

        /* Avoid freeing the correct pointer to not double-free unexpectedly */
        pr_info("invalid_free: Skipping kfree(ptr) to avoid double-free; potential leak left intentionally\n");
    }

    return count;
}

static const struct proc_ops trigger_ops = {
    .proc_write = trigger_write,
};

static int __init invalid_free_init(void)
{
    pr_info("invalid_free: Module loaded\n");
    pr_info("invalid_free: Commands on /proc/invalid_free_trigger: 'stack', 'global', 'offset'\n");

    proc_entry = proc_create("invalid_free_trigger", 0200, NULL, &trigger_ops);
    if (!proc_entry) {
        pr_err("invalid_free: Failed to create proc entry\n");
        return -ENOMEM;
    }

    return 0;
}

static void __exit invalid_free_exit(void)
{
    proc_remove(proc_entry);
    pr_info("invalid_free: Module unloaded\n");
}

module_init(invalid_free_init);
module_exit(invalid_free_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Debug Testing");
MODULE_DESCRIPTION("Invalid Free Scenarios (stack/global/interior) for SLUB/KASAN Testing");
MODULE_VERSION("1.0");
