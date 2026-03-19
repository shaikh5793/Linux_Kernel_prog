/*
Copyright (c) 2024 TECH VEDA(www.techveda.org)
Author: Raghu Bharadwaj

This software is dual-licensed under the MIT License and GPL v2.
See the accompanying LICENSE file for the full text.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kfifo.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/slab.h>

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("Lockless queue using kfifo");
MODULE_VERSION("1.0");

#define QUEUE_SIZE 64
#define MAX_PRODUCERS 2
#define MAX_CONSUMERS 2

struct data_item {
    int id;
    int value;
    char msg[32];
};

static DEFINE_KFIFO(demo_queue, struct data_item, QUEUE_SIZE);
static struct task_struct *producer_threads[MAX_PRODUCERS];
static struct task_struct *consumer_threads[MAX_CONSUMERS];

static int producer_fn(void *data)
{
    int producer_id = *(int *)data;
    struct data_item item;
    int i;
    
    pr_info("%s: Producer %d started\n", __func__, producer_id);
    
    for (i = 0; i < 20 && !kthread_should_stop(); i++) {
        item.id = (producer_id * 1000) + i;
        item.value = i * producer_id;
        snprintf(item.msg, sizeof(item.msg), "P%d-Item%d", producer_id, i);
        
        if (kfifo_put(&demo_queue, item)) {
            pr_info("%s: Producer %d enqueued item %d (value=%d)\n", 
                    __func__, producer_id, item.id, item.value);
        } else {
            pr_warn("%s: Producer %d - queue full, dropping item %d\n", 
                    __func__, producer_id, item.id);
        }
        
        msleep(200 + (producer_id * 50));
    }
    
    pr_info("%s: Producer %d exiting\n", __func__, producer_id);
    return 0;
}

static int consumer_fn(void *data)
{
    int consumer_id = *(int *)data;
    struct data_item item;
    int consumed_count = 0;
    
    pr_info("%s: Consumer %d started\n", __func__, consumer_id);
    
    while (!kthread_should_stop() && consumed_count < 25) {
        if (kfifo_get(&demo_queue, &item)) {
            consumed_count++;
            pr_info("%s: Consumer %d dequeued item %d (value=%d, msg='%s')\n", 
                    __func__, consumer_id, item.id, item.value, item.msg);
        } else {
            msleep(100);
        }
        
        msleep(150 + (consumer_id * 30));
    }
    
    pr_info("%s: Consumer %d exiting after processing %d items\n", 
            __func__, consumer_id, consumed_count);
    return 0;
}

static int __init lockless_queue_init(void)
{
    static int producer_ids[] = {1, 2};
    static int consumer_ids[] = {1, 2};
    int i;
    
    pr_info("%s: Lockless queue module loaded\n", __func__);
    pr_info("%s: Queue capacity: %u items\n", __func__, kfifo_size(&demo_queue));
    
    for (i = 0; i < MAX_PRODUCERS; i++) {
        producer_threads[i] = kthread_run(producer_fn, &producer_ids[i], 
                                         "kfifo_producer%d", i+1);
        if (IS_ERR(producer_threads[i])) {
            pr_err("%s: Failed to create producer %d\n", __func__, i+1);
            while (--i >= 0)
                kthread_stop(producer_threads[i]);
            return PTR_ERR(producer_threads[i]);
        }
    }
    
    for (i = 0; i < MAX_CONSUMERS; i++) {
        consumer_threads[i] = kthread_run(consumer_fn, &consumer_ids[i], 
                                         "kfifo_consumer%d", i+1);
        if (IS_ERR(consumer_threads[i])) {
            pr_err("%s: Failed to create consumer %d\n", __func__, i+1);
            while (--i >= 0)
                kthread_stop(consumer_threads[i]);
            for (i = 0; i < MAX_PRODUCERS; i++)
                kthread_stop(producer_threads[i]);
            return PTR_ERR(consumer_threads[i]);
        }
    }
    
    return 0;
}

static void __exit lockless_queue_exit(void)
{
    int i;
    
    for (i = 0; i < MAX_PRODUCERS; i++) {
        if (producer_threads[i])
            kthread_stop(producer_threads[i]);
    }
    
    for (i = 0; i < MAX_CONSUMERS; i++) {
        if (consumer_threads[i])
            kthread_stop(consumer_threads[i]);
    }
    
    pr_info("%s: Remaining items in queue: %u\n", __func__, kfifo_len(&demo_queue));
    kfifo_reset(&demo_queue);
    
    pr_info("%s: Lockless queue module unloaded\n", __func__);
}

module_init(lockless_queue_init);
module_exit(lockless_queue_exit);