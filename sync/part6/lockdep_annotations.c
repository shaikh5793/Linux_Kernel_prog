/*
Copyright (c) 2024 TECH VEDA(www.techveda.org)
Author: Raghu Bharadwaj

This software is dual-licensed under the MIT License and GPL v2.
See the accompanying LICENSE file for the full text.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/lockdep.h>

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("Lockdep validation example");

static struct mutex ma, mb;
static struct task_struct *kt;
static struct lock_class_key ka_key, kb_key;
static int res_a, res_b;

static void ordered_lock(void)
{
    pr_info("Proper ordering: A->B\n");
    
    mutex_lock(&ma);
    res_a++;
    pr_info("Got A, res_a=%d\n", res_a);
    
    mutex_lock(&mb);
    res_b++;
    pr_info("Got B, res_b=%d\n", res_b);
    
    msleep(50);
    
    mutex_unlock(&mb);
    mutex_unlock(&ma);
    pr_info("Released B->A\n");
}

static void manual_track(void)
{
    pr_info("Manual lockdep tracking\n");
    
    lock_acquire(&ma.dep_map, 0, 0, 0, 1, NULL, _THIS_IP_);
    mutex_lock(&ma);
    
    res_a += 5;
    pr_info("Tracked lock A, res_a=%d\n", res_a);
    
    mutex_unlock(&ma);
    lock_release(&ma.dep_map, _THIS_IP_);
}

static void might_use(void)
{
    pr_info("Potential lock usage\n");
    
    might_lock(&ma);
    might_lock(&mb);
    
    if (res_a < 10) {
        mutex_lock(&ma);
        res_a += 2;
        mutex_unlock(&ma);
        pr_info("Used A conditionally\n");
    }
}

static int kthread_fn(void *unused)
{
    int i;
    
    pr_info("Lockdep test thread started\n");
    
    for (i = 0; i < 3 && !kthread_should_stop(); i++) {
        pr_info("=== Test %d ===\n", i + 1);
        
        ordered_lock();
        msleep(100);
        
        manual_track();
        msleep(100);
        
        might_use();
        
        pr_info("State: A=%d, B=%d\n", res_a, res_b);
        msleep(300);
    }
    
    return 0;
}

static int __init lockdep_init(void)
{
    pr_info("Lockdep example loaded\n");
    
    mutex_init(&ma);
    mutex_init(&mb);
    
    lockdep_set_class(&ma, &ka_key);
    lockdep_set_class(&mb, &kb_key);
    
    kt = kthread_run(kthread_fn, NULL, "lockdep_test");
    if (IS_ERR(kt))
        return PTR_ERR(kt);
    
    return 0;
}

static void __exit lockdep_exit(void)
{
    if (kt)
        kthread_stop(kt);
    
    pr_info("Final: A=%d, B=%d\n", res_a, res_b);
    pr_info("Lockdep example unloaded\n");
}

module_init(lockdep_init);
module_exit(lockdep_exit);
