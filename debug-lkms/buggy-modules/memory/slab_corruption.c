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
    
    if (strncmp(cmd, "corrupt", 7) == 0) {
        char *ptr;
        unsigned long *metadata;
        
        pr_info("slab_corruption: Allocating memory\n");
        ptr = kmalloc(128, GFP_KERNEL);
        if (!ptr)
            return -ENOMEM;
        
        pr_info("slab_corruption: *** TRIGGERING BUG: Slab metadata corruption ***\n");
        pr_info("slab_corruption: Writing beyond object to corrupt slab metadata\n");
        
        /* BUG: Write beyond object into slab metadata area */
        metadata = (unsigned long *)(ptr + 128);
        *metadata = 0x11223344;
        *(metadata + 1) = 0x55667788;
        
        pr_info("slab_corruption: Metadata corrupted, now freeing...\n");
        kfree(ptr);
        
        pr_info("slab_corruption: If you see this, corruption may not have been detected\n");
        
    } else if (strncmp(cmd, "redzones", 8) == 0) {
        char *ptr;
        
        pr_info("slab_corruption: Testing redzone protection\n");
        ptr = kmalloc(32, GFP_KERNEL);
        if (!ptr)
            return -ENOMEM;
        
        pr_info("slab_corruption: *** TRIGGERING BUG: Redzone violation ***\n");
        
        /* BUG: Write immediately past end and deeper into redzone */
        ptr[32] = 'R';  /* Right after allocation */
        ptr[48] = 'Z';  /* In the redzone */
        
        pr_info("slab_corruption: Redzone written, freeing...\n");
        kfree(ptr);
        
    } else if (strncmp(cmd, "poison", 6) == 0) {
        char *ptr;
        
        pr_info("slab_corruption: Testing poison patterns\n");
        ptr = kmalloc(64, GFP_KERNEL);
        if (!ptr)
            return -ENOMEM;
        
        memset(ptr, 0xAA, 64);
        pr_info("slab_corruption: Filled with pattern\n");
        
        kfree(ptr);
        
        pr_info("slab_corruption: *** TRIGGERING BUG: Writing to freed, poisoned memory ***\n");
        /* BUG: UAF write to poison-filled area to disturb poison */
        ptr[0] = 'C';
        ptr[32] = 'O';
        ptr[63] = 'R';
        
        pr_info("slab_corruption: Poison pattern corrupted\n");
    }
    
    return count;
}

static const struct proc_ops trigger_ops = {
    .proc_write = trigger_write,
};

static int __init slab_corruption_init(void)
{
    pr_info("slab_corruption: Module loaded\n");
    pr_info("slab_corruption: Write 'corrupt' to /proc/slab_trigger for metadata corruption\n");
    pr_info("slab_corruption: Write 'redzones' to /proc/slab_trigger for redzone violation\n");
    pr_info("slab_corruption: Write 'poison' to /proc/slab_trigger for poison pattern violation\n");
    
    proc_entry = proc_create("slab_trigger", 0200, NULL, &trigger_ops);
    if (!proc_entry) {
        pr_err("slab_corruption: Failed to create proc entry\n");
        return -ENOMEM;
    }
    
    return 0;
}

static void __exit slab_corruption_exit(void)
{
    proc_remove(proc_entry);
    pr_info("slab_corruption: Module unloaded\n");
}

module_init(slab_corruption_init);
module_exit(slab_corruption_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Debug Testing");
MODULE_DESCRIPTION("Slab Corruption Bug Module for SLUB Debug Testing");
MODULE_VERSION("1.0");
