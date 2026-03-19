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
#include <linux/rcupdate.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/delay.h>

struct rcu_item {
    int val;
    struct rcu_head rcu;
};

static struct proc_dir_entry *proc_entry;
static struct rcu_item __rcu *global_ptr;

static ssize_t trigger_write(struct file *file, const char __user *buffer,
                             size_t count, loff_t *ppos)
{
    char cmd[24];
    struct rcu_item *p, *old;

    if (count >= sizeof(cmd))
        return -EINVAL;
    if (copy_from_user(cmd, buffer, count))
        return -EFAULT;
    cmd[count] = '\0';

    if (strncmp(cmd, "read_no_lock", 12) == 0) {
        pr_info("rcu_misuse: *** TRIGGERING BUG: RCU deref without rcu_read_lock ***\n");
        /* BUG: RCU dereference without read-side lock */
        p = rcu_dereference(global_ptr);
        if (p)
            pr_info("rcu_misuse: read val=%d\n", p->val);
        else
            pr_info("rcu_misuse: ptr is NULL\n");
    } else if (strncmp(cmd, "free_no_sync", 12) == 0) {
        pr_info("rcu_misuse: *** TRIGGERING BUG: Free RCU pointer without grace period ***\n");
        old = rcu_dereference_protected(global_ptr, 1);
        rcu_assign_pointer(global_ptr, NULL);
        if (old) {
            /* BUG: Free immediately without synchronize_rcu()/kfree_rcu() */
            kfree(old);
        }
    } else if (strncmp(cmd, "sleep_in_rcu", 12) == 0) {
        pr_info("rcu_misuse: *** TRIGGERING BUG: Sleep in RCU BH read-side critical section ***\n");
        rcu_read_lock_bh();
        /* BUG: Sleeping inside rcu_read_lock_bh() section */
        msleep(10);
        rcu_read_unlock_bh();
    } else if (strncmp(cmd, "init", 4) == 0) {
        p = kmalloc(sizeof(*p), GFP_KERNEL);
        if (!p)
            return -ENOMEM;
        p->val = 42;
        rcu_assign_pointer(global_ptr, p);
        pr_info("rcu_misuse: Initialized global_ptr=%p val=%d\n", p, p->val);
    }

    return count;
}

static const struct proc_ops trigger_ops = {
    .proc_write = trigger_write,
};

static int __init rcu_misuse_init(void)
{
    pr_info("rcu_misuse: Module loaded\n");
    pr_info("rcu_misuse: /proc/rcu_misuse_trigger cmds: 'init', 'read_no_lock', 'free_no_sync', 'sleep_in_rcu'\n");
    proc_entry = proc_create("rcu_misuse_trigger", 0200, NULL, &trigger_ops);
    if (!proc_entry)
        return -ENOMEM;
    return 0;
}

static void __exit rcu_misuse_exit(void)
{
    struct rcu_item *p;
    p = rcu_dereference_protected(global_ptr, 1);
    if (p)
        kfree(p);
    proc_remove(proc_entry);
    pr_info("rcu_misuse: Module unloaded\n");
}

module_init(rcu_misuse_init);
module_exit(rcu_misuse_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Debug Testing");
MODULE_DESCRIPTION("RCU misuse scenarios for PROVE_RCU/lockdep");
MODULE_VERSION("1.0");
