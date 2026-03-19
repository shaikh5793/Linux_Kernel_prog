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
static char *leaked_ptr = NULL;

static ssize_t trigger_write(struct file *file, const char __user *buffer,
                             size_t count, loff_t *pos)
{
    char cmd[16];
    
    if (count >= sizeof(cmd))
        return -EINVAL;
    
    if (copy_from_user(cmd, buffer, count))
        return -EFAULT;
    
    cmd[count] = '\0';
    
    if (strncmp(cmd, "trigger", 7) == 0) {
        char *ptr;
        
        pr_info("use_after_free: Allocating memory\n");
        ptr = kmalloc(128, GFP_KERNEL);
        if (!ptr)
            return -ENOMEM;
        
        sprintf(ptr, "Test data in allocated memory");
        pr_info("use_after_free: Data written: %s\n", ptr);
        
        pr_info("use_after_free: Freeing memory\n");
        kfree(ptr);
        
        pr_info("use_after_free: *** TRIGGERING BUG: Use-after-free ***\n");
        /* BUG: Read from freed pointer (UAF read) */
        pr_info("use_after_free: Accessing freed memory: %s\n", ptr);
        
        pr_info("use_after_free: Writing to freed memory\n");
        /* BUG: Write to freed pointer (UAF write) */
        sprintf(ptr, "This is a use-after-free bug!");
        
    } else if (strncmp(cmd, "leak", 4) == 0) {
        pr_info("use_after_free: Allocating and leaking memory\n");
        leaked_ptr = kmalloc(256, GFP_KERNEL);
        if (leaked_ptr) {
            sprintf(leaked_ptr, "Leaked memory");
            pr_info("use_after_free: Memory leaked at %p\n", leaked_ptr);
            /* BUG: Lose reference (set to NULL) without freeing -> leak */
            leaked_ptr = NULL;
        }
    }
    
    return count;
}

static const struct proc_ops trigger_ops = {
    .proc_write = trigger_write,
};

static int __init use_after_free_init(void)
{
    pr_info("use_after_free: Module loaded\n");
    pr_info("use_after_free: Write 'trigger' to /proc/uaf_trigger to trigger use-after-free\n");
    pr_info("use_after_free: Write 'leak' to /proc/uaf_trigger to trigger memory leak\n");
    
    proc_entry = proc_create("uaf_trigger", 0200, NULL, &trigger_ops);
    if (!proc_entry) {
        pr_err("use_after_free: Failed to create proc entry\n");
        return -ENOMEM;
    }
    
    return 0;
}

static void __exit use_after_free_exit(void)
{
    proc_remove(proc_entry);
    pr_info("use_after_free: Module unloaded\n");
}

module_init(use_after_free_init);
module_exit(use_after_free_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Debug Testing");
MODULE_DESCRIPTION("Use-After-Free Bug Module for KASAN/KFENCE Testing");
MODULE_VERSION("1.0");
