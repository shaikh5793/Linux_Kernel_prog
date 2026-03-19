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

static struct proc_dir_entry *proc_entry;

struct test_struct {
    int value;
    char name[32];
    void (*func)(void);
};

static ssize_t trigger_write(struct file *file, const char __user *buffer,
                             size_t count, loff_t *pos)
{
    char cmd[16];
    
    if (count >= sizeof(cmd))
        return -EINVAL;
    
    if (copy_from_user(cmd, buffer, count))
        return -EFAULT;
    
    cmd[count] = '\0';
    
    if (strncmp(cmd, "read", 4) == 0) {
        int *null_ptr = NULL;
        
        pr_info("null_deref: *** TRIGGERING BUG: Null pointer read ***\n");
        pr_info("null_deref: Attempting to read from NULL pointer\n");
        
        /* BUG: Dereference NULL pointer (read) */
        pr_info("null_deref: Value at NULL: %d\n", *null_ptr);
        
    } else if (strncmp(cmd, "write", 5) == 0) {
        int *null_ptr = NULL;
        
        pr_info("null_deref: *** TRIGGERING BUG: Null pointer write ***\n");
        pr_info("null_deref: Attempting to write to NULL pointer\n");
        
        /* BUG: Dereference NULL pointer (write) */
        *null_ptr = 0x12345678;
        
        pr_info("null_deref: If you see this, something is very wrong\n");
        
    } else if (strncmp(cmd, "struct", 6) == 0) {
        struct test_struct *ptr = NULL;
        
        pr_info("null_deref: *** TRIGGERING BUG: Null struct dereference ***\n");
        pr_info("null_deref: Accessing NULL struct member\n");
        
        /* BUG: Access member through NULL struct pointer */
        pr_info("null_deref: Struct value: %d\n", ptr->value);
        
    } else if (strncmp(cmd, "function", 8) == 0) {
        struct test_struct *ptr = NULL;
        
        pr_info("null_deref: *** TRIGGERING BUG: Null function pointer call ***\n");
        pr_info("null_deref: Calling NULL function pointer\n");
        
        /* BUG: Would oops if ptr->func used without checks */
        if (ptr && ptr->func)
            ptr->func();
        
        /* Actually trigger it without check */
        pr_info("null_deref: Now really calling NULL function\n");
        void (*null_func)(void) = NULL;
        /* BUG: Call through NULL function pointer */
        null_func();
        
    } else if (strncmp(cmd, "offset", 6) == 0) {
        struct test_struct *ptr = NULL;
        
        pr_info("null_deref: *** TRIGGERING BUG: Null pointer with offset ***\n");
        pr_info("null_deref: Accessing NULL pointer with offset\n");
        
        /* BUG: Access field via offset from NULL base */
        pr_info("null_deref: Name field: %s\n", ptr->name);
    }
    
    return count;
}

static const struct proc_ops trigger_ops = {
    .proc_write = trigger_write,
};

static int __init null_deref_init(void)
{
    pr_info("null_deref: Module loaded\n");
    pr_info("null_deref: WARNING: These tests will cause kernel oops!\n");
    pr_info("null_deref: Write 'read' to /proc/null_deref_trigger for null read\n");
    pr_info("null_deref: Write 'write' to /proc/null_deref_trigger for null write\n");
    pr_info("null_deref: Write 'struct' to /proc/null_deref_trigger for struct dereference\n");
    pr_info("null_deref: Write 'function' to /proc/null_deref_trigger for function call\n");
    pr_info("null_deref: Write 'offset' to /proc/null_deref_trigger for offset access\n");
    
    proc_entry = proc_create("null_deref_trigger", 0200, NULL, &trigger_ops);
    if (!proc_entry) {
        pr_err("null_deref: Failed to create proc entry\n");
        return -ENOMEM;
    }
    
    return 0;
}

static void __exit null_deref_exit(void)
{
    proc_remove(proc_entry);
    pr_info("null_deref: Module unloaded\n");
}

module_init(null_deref_init);
module_exit(null_deref_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Debug Testing");
MODULE_DESCRIPTION("Null Pointer Dereference Bug Module");
MODULE_VERSION("1.0");
