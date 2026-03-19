/*
Copyright (c) 2024 TECH VEDA(www.techveda.org)
Author: Raghu Bharadwaj

This software is dual-licensed under the MIT License and GPL v2.
See the accompanying LICENSE file for the full text.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/rculist.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/slab.h>

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("Lock-free list operations using RCU");
MODULE_VERSION("1.0");

struct list_node {
    struct list_head list;
    struct rcu_head rcu;
    int data;
    int thread_id;
};

static LIST_HEAD(demo_list);
static struct task_struct *worker_threads[3];

static void free_node_rcu(struct rcu_head *head)
{
    struct list_node *node = container_of(head, struct list_node, rcu);
    pr_info("%s: RCU freeing node with data=%d (from thread %d)\n", 
            __func__, node->data, node->thread_id);
    kfree(node);
}

static struct list_node *create_node(int data, int thread_id)
{
    struct list_node *node = kmalloc(sizeof(*node), GFP_KERNEL);
    if (!node)
        return NULL;
    
    INIT_LIST_HEAD(&node->list);
    node->data = data;
    node->thread_id = thread_id;
    
    return node;
}

static void print_list(const char *context)
{
    struct list_node *node;
    int count = 0;
    
    pr_info("%s: List contents (%s):\n", __func__, context);
    
    rcu_read_lock();
    list_for_each_entry_rcu(node, &demo_list, list) {
        pr_info("  Node %d: data=%d (thread %d)\n", count++, node->data, node->thread_id);
    }
    rcu_read_unlock();
    
    if (count == 0)
        pr_info("  (empty list)\n");
}

static int worker_thread_fn(void *data)
{
    int thread_id = *(int *)data;
    struct list_node *node, *temp;
    int i;
    
    pr_info("%s: Worker thread %d started\n", __func__, thread_id);
    
    for (i = 0; i < 8 && !kthread_should_stop(); i++) {
        node = create_node((thread_id * 100) + i, thread_id);
        if (node) {
            pr_info("%s: Thread %d adding node with data=%d\n", 
                    __func__, thread_id, node->data);
            list_add_rcu(&node->list, &demo_list);
        }
        
        msleep(200);
        
        if (i % 3 == 0 && i > 0) {
            rcu_read_lock();
            list_for_each_entry_rcu(temp, &demo_list, list) {
                if (temp->thread_id == thread_id && temp->data % 2 == 0) {
                    pr_info("%s: Thread %d removing node with data=%d\n", 
                            __func__, thread_id, temp->data);
                    list_del_rcu(&temp->list);
                    call_rcu(&temp->rcu, free_node_rcu);
                    break;
                }
            }
            rcu_read_unlock();
        }
        
        msleep(100 + (thread_id * 50));
    }
    
    if (thread_id == 1) {
        msleep(500);
        print_list("from thread 1");
    }
    
    pr_info("%s: Worker thread %d exiting\n", __func__, thread_id);
    return 0;
}

static int __init lockfree_list_init(void)
{
    static int thread_ids[] = {1, 2, 3};
    int i;
    
    pr_info("%s: Lock-free list module loaded\n", __func__);
    
    for (i = 0; i < 3; i++) {
        worker_threads[i] = kthread_run(worker_thread_fn, &thread_ids[i], 
                                       "lockfree_worker%d", i+1);
        if (IS_ERR(worker_threads[i])) {
            pr_err("%s: Failed to create worker thread %d\n", __func__, i+1);
            while (--i >= 0)
                kthread_stop(worker_threads[i]);
            return PTR_ERR(worker_threads[i]);
        }
    }
    
    return 0;
}

static void __exit lockfree_list_exit(void)
{
    struct list_node *node, *temp;
    int i;
    
    for (i = 0; i < 3; i++) {
        if (worker_threads[i])
            kthread_stop(worker_threads[i]);
    }
    
    print_list("before cleanup");
    
    list_for_each_entry_safe(node, temp, &demo_list, list) {
        list_del_rcu(&node->list);
        synchronize_rcu();
        kfree(node);
    }
    
    pr_info("%s: Lock-free list module unloaded\n", __func__);
}

module_init(lockfree_list_init);
module_exit(lockfree_list_exit);