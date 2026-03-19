/*
Copyright (c) 2024 TECH VEDA(www.techveda.org)
Author: Raghu Bharadwaj

This software is dual-licensed under the MIT License and GPL v2.
See the accompanying LICENSE file for the full text.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/rcupdate.h>
#include <linux/srcu.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/slab.h>

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("Comparing different RCU variants");
MODULE_VERSION("1.0");

struct demo_data {
    struct rcu_head rcu_head;
    int value;
    char variant[16];
};

static struct demo_data __rcu *classic_rcu_data = NULL;
static struct demo_data __rcu *srcu_data = NULL;
static struct task_struct *demo_thread;
DEFINE_SRCU(demo_srcu);

static void free_classic_rcu(struct rcu_head *head)
{
    struct demo_data *data = container_of(head, struct demo_data, rcu_head);
    pr_info("%s: Freeing classic RCU data: value=%d, variant='%s'\n", 
            __func__, data->value, data->variant);
    kfree(data);
}

static struct demo_data *create_data(int value, const char *variant)
{
    struct demo_data *data = kmalloc(sizeof(*data), GFP_KERNEL);
    if (!data)
        return NULL;
    
    data->value = value;
    strncpy(data->variant, variant, sizeof(data->variant) - 1);
    data->variant[sizeof(data->variant) - 1] = '\0';
    
    return data;
}

static void demonstrate_classic_rcu(int iteration)
{
    struct demo_data *new_data, *old_data;
    
    pr_info("%s: === Classic RCU demonstration (iteration %d) ===\n", __func__, iteration);
    
    new_data = create_data(iteration * 10, "classic_rcu");
    if (!new_data)
        return;
    
    pr_info("%s: Classic RCU - updating to value=%d\n", __func__, new_data->value);
    
    old_data = rcu_dereference_protected(classic_rcu_data, 1);
    rcu_assign_pointer(classic_rcu_data, new_data);
    
    if (old_data) {
        pr_info("%s: Classic RCU - calling synchronize_rcu()\n", __func__);
        synchronize_rcu();
        kfree(old_data);
    }
    
    rcu_read_lock();
    new_data = rcu_dereference(classic_rcu_data);
    if (new_data)
        pr_info("%s: Classic RCU - read value=%d, variant='%s'\n", 
                __func__, new_data->value, new_data->variant);
    rcu_read_unlock();
}

static void demonstrate_srcu(int iteration)
{
    struct demo_data *new_data, *old_data;
    int idx;
    
    pr_info("%s: === SRCU demonstration (iteration %d) ===\n", __func__, iteration);
    
    new_data = create_data(iteration * 20, "srcu");
    if (!new_data)
        return;
    
    pr_info("%s: SRCU - updating to value=%d\n", __func__, new_data->value);
    
    old_data = rcu_dereference_protected(srcu_data, 1);
    rcu_assign_pointer(srcu_data, new_data);
    
    if (old_data) {
        pr_info("%s: SRCU - calling synchronize_srcu()\n", __func__);
        synchronize_srcu(&demo_srcu);
        kfree(old_data);
    }
    
    idx = srcu_read_lock(&demo_srcu);
    new_data = rcu_dereference(srcu_data);
    if (new_data) {
        pr_info("%s: SRCU - read value=%d, variant='%s'\n", 
                __func__, new_data->value, new_data->variant);
        
        pr_info("%s: SRCU - sleeping in read-side critical section\n", __func__);
        msleep(100);
        
        pr_info("%s: SRCU - continuing after sleep with value=%d\n", 
                __func__, new_data->value);
    }
    srcu_read_unlock(&demo_srcu, idx);
}

static void demonstrate_rcu_bh(int iteration)
{
    struct demo_data *data;
    
    pr_info("%s: === RCU-bh demonstration (iteration %d) ===\n", __func__, iteration);
    
    rcu_read_lock_bh();
    data = rcu_dereference(classic_rcu_data);
    if (data) {
        pr_info("%s: RCU-bh - read value=%d with bottom halves disabled\n", 
                __func__, data->value);
    }
    rcu_read_unlock_bh();
}

static int demo_thread_fn(void *unused)
{
    int i;
    
    pr_info("%s: RCU variants demo thread started\n", __func__);
    
    for (i = 0; i < 6 && !kthread_should_stop(); i++) {
        pr_info("%s: ===== RCU Variants Comparison - Round %d =====\n", __func__, i + 1);
        
        demonstrate_classic_rcu(i);
        msleep(200);
        
        demonstrate_srcu(i);
        msleep(200);
        
        demonstrate_rcu_bh(i);
        
        pr_info("%s: Completed round %d, sleeping...\n", __func__, i + 1);
        msleep(1000);
    }
    
    pr_info("%s: RCU variants demo thread exiting\n", __func__);
    return 0;
}

static int __init rcu_variants_init(void)
{
    pr_info("%s: RCU variants comparison module loaded\n", __func__);
    pr_info("%s: Comparing Classic RCU, SRCU, and RCU-bh\n", __func__);
    
    classic_rcu_data = create_data(0, "initial");
    if (!classic_rcu_data) {
        pr_err("%s: Failed to create initial classic RCU data\n", __func__);
        return -ENOMEM;
    }
    
    srcu_data = create_data(0, "srcu_init");
    if (!srcu_data) {
        pr_err("%s: Failed to create initial SRCU data\n", __func__);
        kfree(classic_rcu_data);
        return -ENOMEM;
    }
    
    demo_thread = kthread_run(demo_thread_fn, NULL, "rcu_variants_demo");
    if (IS_ERR(demo_thread)) {
        pr_err("%s: Failed to create demo thread\n", __func__);
        kfree(classic_rcu_data);
        kfree(srcu_data);
        return PTR_ERR(demo_thread);
    }
    
    return 0;
}

static void __exit rcu_variants_exit(void)
{
    struct demo_data *data;
    
    if (demo_thread)
        kthread_stop(demo_thread);
    
    data = rcu_dereference_protected(classic_rcu_data, 1);
    if (data) {
        synchronize_rcu();
        kfree(data);
    }
    
    data = rcu_dereference_protected(srcu_data, 1);
    if (data) {
        synchronize_srcu(&demo_srcu);
        kfree(data);
    }
    
    cleanup_srcu_struct(&demo_srcu);
    
    pr_info("%s: RCU variants comparison module unloaded\n", __func__);
}

module_init(rcu_variants_init);
module_exit(rcu_variants_exit);