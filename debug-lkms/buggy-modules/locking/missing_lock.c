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
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/delay.h>

static struct proc_dir_entry *proc_entry;
static DEFINE_MUTEX(protect_counter);
static int shared_counter = 0;
static struct task_struct *race_thread1;
static struct task_struct *race_thread2;

static int race_thread_fn(void *data)
{
    int i;
    int thread_id = (long)data;
    
    pr_info("missing_lock: Thread %d starting\n", thread_id);
    
    for (i = 0; i < 1000; i++) {
        /* INTENTIONAL BUG: Accessing shared_counter without lock */
        shared_counter++;
        
        /* Add small delay to increase chance of race */
        if (i % 100 == 0)
            msleep(1);
    }
    
    pr_info("missing_lock: Thread %d finished\n", thread_id);
    return 0;
}

static int race_thread_locked_fn(void *data)
{
    int i;
    int thread_id = (long)data;
    
    pr_info("missing_lock: Thread %d (locked) starting\n", thread_id);
    
    for (i = 0; i < 1000; i++) {
        /* CORRECT: Using lock */
        mutex_lock(&protect_counter);
        shared_counter++;
        mutex_unlock(&protect_counter);
        
        if (i % 100 == 0)
            msleep(1);
    }
    
    pr_info("missing_lock: Thread %d (locked) finished\n", thread_id);
    return 0;
}

static ssize_t trigger_write(struct file *file, const char __user *buffer,
                             size_t count, loff_t *pos)
{
    char cmd[16];
    
    if (count >= sizeof(cmd))
        return -EINVAL;
    
    if (copy_from_user(cmd, buffer, count))
        return -EFAULT;
    
    cmd[count] = '\0';
    
    if (strncmp(cmd, "race", 4) == 0) {
        pr_info("missing_lock: *** TRIGGERING BUG: Race condition without lock ***\n");
        pr_info("missing_lock: Resetting counter to 0\n");
        shared_counter = 0;
        
        pr_info("missing_lock: Creating racing threads (no lock protection)\n");
        
        race_thread1 = kthread_run(race_thread_fn, (void *)1L, "race_t1");
        if (IS_ERR(race_thread1)) {
            pr_err("missing_lock: Failed to create thread1\n");
            return PTR_ERR(race_thread1);
        }
        
        race_thread2 = kthread_run(race_thread_fn, (void *)2L, "race_t2");
        if (IS_ERR(race_thread2)) {
            pr_err("missing_lock: Failed to create thread2\n");
            return PTR_ERR(race_thread2);
        }
        
        pr_info("missing_lock: Threads created. Expected final counter: 2000\n");
        pr_info("missing_lock: Check /proc/missing_lock_status after threads finish\n");
        
    } else if (strncmp(cmd, "correct", 7) == 0) {
        pr_info("missing_lock: Running CORRECT version with locks\n");
        pr_info("missing_lock: Resetting counter to 0\n");
        shared_counter = 0;
        
        pr_info("missing_lock: Creating threads with proper locking\n");
        
        race_thread1 = kthread_run(race_thread_locked_fn, (void *)1L, "race_lock_t1");
        if (IS_ERR(race_thread1)) {
            pr_err("missing_lock: Failed to create thread1\n");
            return PTR_ERR(race_thread1);
        }
        
        race_thread2 = kthread_run(race_thread_locked_fn, (void *)2L, "race_lock_t2");
        if (IS_ERR(race_thread2)) {
            pr_err("missing_lock: Failed to create thread2\n");
            return PTR_ERR(race_thread2);
        }
        
        pr_info("missing_lock: Threads created with locks. Expected final counter: 2000\n");
        
    } else if (strncmp(cmd, "reset", 5) == 0) {
        mutex_lock(&protect_counter);
        shared_counter = 0;
        mutex_unlock(&protect_counter);
        pr_info("missing_lock: Counter reset to 0\n");
    }
    
    return count;
}

static int status_show(struct seq_file *m, void *v)
{
    seq_printf(m, "=== Missing Lock Test Status ===\n");
    seq_printf(m, "Shared counter value: %d\n", shared_counter);
    seq_printf(m, "\nExpected value with 2 threads: 2000\n");
    seq_printf(m, "If counter != 2000 after race test, race condition occurred\n");
    
    return 0;
}

static int status_open(struct inode *inode, struct file *file)
{
    return single_open(file, status_show, NULL);
}

static const struct proc_ops trigger_ops = {
    .proc_write = trigger_write,
};

static const struct proc_ops status_ops = {
    .proc_open = status_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static int __init missing_lock_init(void)
{
    struct proc_dir_entry *status_entry;
    
    pr_info("missing_lock: Module loaded\n");
    pr_info("missing_lock: Write 'race' to /proc/missing_lock_trigger for race condition\n");
    pr_info("missing_lock: Write 'correct' to /proc/missing_lock_trigger for correct version\n");
    pr_info("missing_lock: Write 'reset' to /proc/missing_lock_trigger to reset counter\n");
    
    proc_entry = proc_create("missing_lock_trigger", 0200, NULL, &trigger_ops);
    if (!proc_entry) {
        pr_err("missing_lock: Failed to create proc entry\n");
        return -ENOMEM;
    }
    
    status_entry = proc_create("missing_lock_status", 0444, NULL, &status_ops);
    if (!status_entry) {
        proc_remove(proc_entry);
        pr_err("missing_lock: Failed to create status entry\n");
        return -ENOMEM;
    }
    
    return 0;
}

static void __exit missing_lock_exit(void)
{
    if (race_thread1 && !IS_ERR(race_thread1))
        kthread_stop(race_thread1);
    if (race_thread2 && !IS_ERR(race_thread2))
        kthread_stop(race_thread2);
    
    proc_remove(proc_entry);
    remove_proc_entry("missing_lock_status", NULL);
    pr_info("missing_lock: Module unloaded\n");
}

module_init(missing_lock_init);
module_exit(missing_lock_exit);



MODULE_LICENSE("GPL");
MODULE_AUTHOR("Debug Testing");
MODULE_DESCRIPTION("Missing Lock Bug Module for Race Condition Testing");
MODULE_VERSION("1.0");
