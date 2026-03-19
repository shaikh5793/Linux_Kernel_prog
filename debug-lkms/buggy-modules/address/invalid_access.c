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
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/highmem.h>

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
    
    if (strncmp(cmd, "kernel", 6) == 0) {
        unsigned long *bad_addr;
        
        pr_info("invalid_access: *** TRIGGERING BUG: Invalid kernel address ***\n");
        pr_info("invalid_access: Accessing invalid kernel virtual address\n");
        
        /* BUG: Read from invalid kernel virtual address */
        bad_addr = (unsigned long *)0xDEADBEEF;
        pr_info("invalid_access: Reading from 0x%p\n", bad_addr);
        pr_info("invalid_access: Value: 0x%lx\n", *bad_addr);
        
    } else if (strncmp(cmd, "user", 4) == 0) {
        unsigned long *user_addr;
        unsigned long value;
        
        pr_info("invalid_access: *** TRIGGERING BUG: Accessing userspace from kernel ***\n");
        pr_info("invalid_access: Direct userspace access without copy_from_user\n");
        
        /* BUG: Direct userspace dereference from kernel context */
        user_addr = (unsigned long *)0x1000;
        value = *user_addr;
        
        pr_info("invalid_access: User value: 0x%lx\n", value);
        
    } else if (strncmp(cmd, "unaligned", 9) == 0) {
        unsigned long *unaligned_ptr;
        char buffer_data[64];
        
        pr_info("invalid_access: *** TRIGGERING BUG: Unaligned access ***\n");
        pr_info("invalid_access: Attempting unaligned memory access\n");
        
        /* BUG: Create intentionally unaligned pointer */
        unaligned_ptr = (unsigned long *)(buffer_data + 1);
        
        pr_info("invalid_access: Writing to unaligned address %p\n", unaligned_ptr);
        /* BUG: Unaligned store may fault on some archs */
        *unaligned_ptr = 0x123456789ABCDEF0UL;
        
        pr_info("invalid_access: Reading from unaligned address\n");
        pr_info("invalid_access: Value: 0x%lx\n", *unaligned_ptr);
        
    } else if (strncmp(cmd, "high", 4) == 0) {
        void *bad_addr;
        
        pr_info("invalid_access: *** TRIGGERING BUG: Out of range kernel address ***\n");
        pr_info("invalid_access: Accessing very high kernel address\n");
        
        /* BUG: Access address likely unmapped / non-canonical */
        bad_addr = (void *)0xFFFFFFFFFFFFFFFFUL;
        pr_info("invalid_access: Accessing %p\n", bad_addr);
        pr_info("invalid_access: Value: %d\n", *(int *)bad_addr);
        
    } else if (strncmp(cmd, "freed", 5) == 0) {
        void *addr;
        
        pr_info("invalid_access: *** TRIGGERING BUG: Accessing freed page ***\n");
        
        /* Get a page and immediately free it */
        addr = (void *)__get_free_page(GFP_KERNEL);
        if (!addr) {
            pr_err("invalid_access: Failed to allocate page\n");
            return -ENOMEM;
        }
        
        pr_info("invalid_access: Got page at %p\n", addr);
        pr_info("invalid_access: Freeing page\n");
        free_page((unsigned long)addr);
        
        pr_info("invalid_access: Accessing freed page (may or may not crash)\n");
        /* BUG: Use memory after freeing whole page */
        memset(addr, 0xCC, PAGE_SIZE);
        
        pr_info("invalid_access: If you see this, freed page access succeeded\n");
        
    } else if (strncmp(cmd, "exec", 4) == 0) {
        char *data_page;
        void (*func)(void);
        
        pr_info("invalid_access: *** TRIGGERING BUG: Executing non-executable memory ***\n");
        
        /* Allocate a data page */
        data_page = kmalloc(64, GFP_KERNEL);
        if (!data_page)
            return -ENOMEM;
        
        /* Try to execute it as code */
        pr_info("invalid_access: Attempting to execute data as code\n");
        func = (void (*)(void))data_page;
        /* BUG: Jump to data memory (NX violation) */
        func();
        
        kfree(data_page);
    } else if (strncmp(cmd, "io", 2) == 0) {
        void __iomem *bad_io;
        
        pr_info("invalid_access: *** TRIGGERING BUG: Invalid I/O address ***\n");
        
        /* BUG: MMIO read from likely unmapped/invalid I/O address */
        bad_io = (void __iomem *)0xF0000000;
        pr_info("invalid_access: Reading from I/O address %p\n", bad_io);
        pr_info("invalid_access: Value: 0x%x\n", readl(bad_io));
    }
    
    return count;
}

static const struct proc_ops trigger_ops = {
    .proc_write = trigger_write,
};

static int __init invalid_access_init(void)
{
    pr_info("invalid_access: Module loaded\n");
    pr_info("invalid_access: WARNING: These tests will likely cause kernel oops!\n");
    pr_info("invalid_access: Write 'kernel' to /proc/invalid_access_trigger for invalid kernel addr\n");
    pr_info("invalid_access: Write 'user' to /proc/invalid_access_trigger for userspace access\n");
    pr_info("invalid_access: Write 'unaligned' to /proc/invalid_access_trigger for unaligned access\n");
    pr_info("invalid_access: Write 'high' to /proc/invalid_access_trigger for high address\n");
    pr_info("invalid_access: Write 'freed' to /proc/invalid_access_trigger for freed page access\n");
    pr_info("invalid_access: Write 'exec' to /proc/invalid_access_trigger for exec data\n");
    pr_info("invalid_access: Write 'io' to /proc/invalid_access_trigger for invalid I/O\n");
    
    proc_entry = proc_create("invalid_access_trigger", 0200, NULL, &trigger_ops);
    if (!proc_entry) {
        pr_err("invalid_access: Failed to create proc entry\n");
        return -ENOMEM;
    }
    
    return 0;
}

static void __exit invalid_access_exit(void)
{
    proc_remove(proc_entry);
    pr_info("invalid_access: Module unloaded\n");
}

module_init(invalid_access_init);
module_exit(invalid_access_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Debug Testing");
MODULE_DESCRIPTION("Invalid Memory Access Bug Module for MMU/Page Fault Testing");
MODULE_VERSION("1.0");
