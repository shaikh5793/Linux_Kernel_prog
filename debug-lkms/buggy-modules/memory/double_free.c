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
    
    if (strncmp(cmd, "trigger", 7) == 0) {
        char *ptr;
        
        pr_info("double_free: Allocating memory\n");
        ptr = kmalloc(128, GFP_KERNEL);
        if (!ptr)
            return -ENOMEM;
        
        sprintf(ptr, "Test data");
        pr_info("double_free: Memory allocated at %p\n", ptr);
        
        pr_info("double_free: First kfree()\n");
        kfree(ptr);
        
        pr_info("double_free: *** TRIGGERING BUG: Double free ***\n");
        pr_info("double_free: Second kfree() on same pointer\n");
        /* BUG: Freeing the same allocation twice */
        kfree(ptr);
        
        pr_info("double_free: If you see this, SLUB debug didn't catch it\n");
        
    } else if (strncmp(cmd, "complex", 7) == 0) {
        char *ptr1, *ptr2;
        
        pr_info("double_free: Complex double-free scenario\n");
        
        ptr1 = kmalloc(64, GFP_KERNEL);
        if (!ptr1)
            return -ENOMEM;
        
        ptr2 = ptr1; /* Aliasing */
        
        pr_info("double_free: ptr1 = %p, ptr2 = %p\n", ptr1, ptr2);
        pr_info("double_free: Freeing via ptr1\n");
        kfree(ptr1);
        
        pr_info("double_free: *** TRIGGERING BUG: Freeing via ptr2 (alias) ***\n");
        /* BUG: Freeing an alias to an already-freed allocation */
        kfree(ptr2);
    }
    
    return count;
}

static const struct proc_ops trigger_ops = {
    .proc_write = trigger_write,
};

static int __init double_free_init(void)
{
    pr_info("double_free: Module loaded\n");
    pr_info("double_free: Write 'trigger' to /proc/double_free_trigger for simple double free\n");
    pr_info("double_free: Write 'complex' to /proc/double_free_trigger for complex scenario\n");
    
    proc_entry = proc_create("double_free_trigger", 0200, NULL, &trigger_ops);
    if (!proc_entry) {
        pr_err("double_free: Failed to create proc entry\n");
        return -ENOMEM;
    }
    
    return 0;
}

static void __exit double_free_exit(void)
{
    proc_remove(proc_entry);
    pr_info("double_free: Module unloaded\n");
}

module_init(double_free_init);
module_exit(double_free_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Debug Testing");
MODULE_DESCRIPTION("Double Free Bug Module for SLUB Debug Testing");
MODULE_VERSION("1.0");
