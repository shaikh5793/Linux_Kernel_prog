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
        
        pr_info("buffer_overflow: Allocating 64 bytes\n");
        buf = kmalloc(64, GFP_KERNEL);
        if (!buf)
            return -ENOMEM;
        
        pr_info("buffer_overflow: *** TRIGGERING BUG: Buffer overflow ***\n");
        pr_info("buffer_overflow: Writing 128 bytes to 64-byte buffer\n");
        
        /* BUG: Write past end of 64B allocation (heap OOB write) */
        for (i = 0; i < 128; i++) {
            buf[i] = 'A' + (i % 26);
        }
        
        pr_info("buffer_overflow: Overflow complete\n");
        kfree(buf);
        
    } else if (strncmp(cmd, "underflow", 9) == 0) {
        char *buf;
        int i;
        
        pr_info("buffer_overflow: Allocating 64 bytes\n");
        buf = kmalloc(64, GFP_KERNEL);
        if (!buf)
            return -ENOMEM;
        
        pr_info("buffer_overflow: *** TRIGGERING BUG: Buffer underflow ***\n");
        pr_info("buffer_overflow: Writing 64 bytes BEFORE buffer start\n");
        
        /* BUG: Write using large negative indices (before allocation start) */
        /* This creates a more detectable out-of-bounds access */
        for (i = -64; i < 0; i++) {
            buf[i] = 'U' + ((-i) % 26);
        }
        
        pr_info("buffer_overflow: Underflow complete\n");
        kfree(buf);
    }
    
    return count;
}

static const struct proc_ops trigger_ops = {
    .proc_write = trigger_write,
};

static int __init buffer_overflow_init(void)
{
    pr_info("buffer_overflow: Module loaded\n");
    pr_info("buffer_overflow: Write 'overflow' to /proc/overflow_trigger for buffer overflow\n");
    pr_info("buffer_overflow: Write 'underflow' to /proc/overflow_trigger for buffer underflow\n");
    
    proc_entry = proc_create("overflow_trigger", 0200, NULL, &trigger_ops);
    if (!proc_entry) {
        pr_err("buffer_overflow: Failed to create proc entry\n");
        return -ENOMEM;
    }
    
    return 0;
}

static void __exit buffer_overflow_exit(void)
{
    proc_remove(proc_entry);
    pr_info("buffer_overflow: Module unloaded\n");
}

module_init(buffer_overflow_init);
module_exit(buffer_overflow_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Debug Testing");
MODULE_DESCRIPTION("Buffer Overflow Bug Module for KASAN/SLUB Testing");
MODULE_VERSION("1.0");
