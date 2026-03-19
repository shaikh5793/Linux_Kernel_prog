/*
Copyright (c) 2024 TECH VEDA(www.techveda.org)
Author: Raghu Bharadwaj

This software is dual-licensed under the MIT License and GPL v2.
See the accompanying LICENSE file for the full text.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/smp.h>
#include <linux/atomic.h>

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("Memory barriers demonstration");
MODULE_VERSION("1.0");

static struct task_struct *producer_thread;
static struct task_struct *consumer_thread;
static int shared_data = 0;
static int shared_flag = 0;

static int producer_fn(void *data)
{
    int i;
    
    pr_info("%s: Producer thread started\n", __func__);
    
    for (i = 0; i < 10 && !kthread_should_stop(); i++) {
        shared_data = i + 100;
        
        smp_wmb();
        
        shared_flag = 1;
        
        pr_info("%s: Producer set data=%d, flag=%d\n", __func__, shared_data, shared_flag);
        msleep(1000);
        
        shared_flag = 0;
        msleep(500);
    }
    
    pr_info("%s: Producer thread exiting\n", __func__);
    return 0;
}

static int consumer_fn(void *data)
{
    int local_data, local_flag;
    
    pr_info("%s: Consumer thread started\n", __func__);
    
    while (!kthread_should_stop()) {
        local_flag = shared_flag;
        
        smp_rmb();
        
        if (local_flag) {
            local_data = shared_data;
            pr_info("%s: Consumer read data=%d when flag=%d\n", __func__, local_data, local_flag);
        }
        
        msleep(100);
    }
    
    pr_info("%s: Consumer thread exiting\n", __func__);
    return 0;
}

static int __init memory_barriers_init(void)
{
    pr_info("%s: Memory barriers module loaded\n", __func__);
    pr_info("%s: Demonstrating smp_wmb() and smp_rmb() usage\n", __func__);
    
    producer_thread = kthread_run(producer_fn, NULL, "mb_producer");
    if (IS_ERR(producer_thread)) {
        pr_err("%s: Failed to create producer thread\n", __func__);
        return PTR_ERR(producer_thread);
    }
    
    consumer_thread = kthread_run(consumer_fn, NULL, "mb_consumer");
    if (IS_ERR(consumer_thread)) {
        pr_err("%s: Failed to create consumer thread\n", __func__);
        kthread_stop(producer_thread);
        return PTR_ERR(consumer_thread);
    }
    
    return 0;
}

static void __exit memory_barriers_exit(void)
{
    if (producer_thread)
        kthread_stop(producer_thread);
    if (consumer_thread)
        kthread_stop(consumer_thread);
    
    pr_info("%s: Memory barriers module unloaded\n", __func__);
}

module_init(memory_barriers_init);
module_exit(memory_barriers_exit);