/*
Copyright (c) 2024 TECH VEDA(www.techveda.org)
Author: Raghu Bharadwaj

This software is dual-licensed under the MIT License and GPL v2.
See the accompanying LICENSE file for the full text.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/refcount.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/delay.h>

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("Modern refcount_t usage demonstration");
MODULE_VERSION("1.0");

struct resource_object {
    refcount_t refcount;
    int resource_id;
    char description[64];
    bool active;
};

static struct resource_object *shared_resource = NULL;
static struct task_struct *user_threads[4];
static DEFINE_SPINLOCK(resource_lock);

static void free_resource(struct resource_object *res)
{
    pr_info("%s: Freeing resource %d ('%s')\n", __func__, res->resource_id, res->description);
    kfree(res);
}

static struct resource_object *get_resource(void)
{
    struct resource_object *res;
    
    spin_lock(&resource_lock);
    res = shared_resource;
    if (res && refcount_inc_not_zero(&res->refcount)) {
        spin_unlock(&resource_lock);
        return res;
    }
    spin_unlock(&resource_lock);
    return NULL;
}

static void put_resource(struct resource_object *res)
{
    if (refcount_dec_and_test(&res->refcount)) {
        free_resource(res);
    }
}

static struct resource_object *create_resource(int id, const char *desc)
{
    struct resource_object *res = kmalloc(sizeof(*res), GFP_KERNEL);
    if (!res)
        return NULL;
    
    refcount_set(&res->refcount, 1);
    res->resource_id = id;
    strncpy(res->description, desc, sizeof(res->description) - 1);
    res->description[sizeof(res->description) - 1] = '\0';
    res->active = true;
    
    pr_info("%s: Created resource %d ('%s') with refcount=1\n", __func__, id, desc);
    return res;
}

static int user_thread_fn(void *data)
{
    int thread_id = *(int *)data;
    struct resource_object *res;
    int operations = 0;
    
    pr_info("%s: User thread %d started\n", __func__, thread_id);
    
    while (!kthread_should_stop() && operations < 8) {
        res = get_resource();
        if (res) {
            pr_info("%s: Thread %d acquired resource %d (refcount=%u)\n", 
                    __func__, thread_id, res->resource_id, 
                    refcount_read(&res->refcount));
            
            msleep(200 + (thread_id * 50));
            
            if (res->active) {
                res->resource_id += thread_id;
                pr_info("%s: Thread %d modified resource to %d\n", 
                        __func__, thread_id, res->resource_id);
            }
            
            put_resource(res);
            pr_info("%s: Thread %d released resource\n", __func__, thread_id);
            operations++;
        } else {
            pr_info("%s: Thread %d could not acquire resource\n", __func__, thread_id);
        }
        
        msleep(100);
    }
    
    pr_info("%s: User thread %d exiting after %d operations\n", __func__, thread_id, operations);
    return 0;
}

static int __init refcount_example_init(void)
{
    static int thread_ids[] = {1, 2, 3, 4};
    int i;
    
    pr_info("%s: Refcount example module loaded\n", __func__);
    
    shared_resource = create_resource(1000, "Shared System Resource");
    if (!shared_resource) {
        pr_err("%s: Failed to create resource\n", __func__);
        return -ENOMEM;
    }
    
    for (i = 0; i < 4; i++) {
        user_threads[i] = kthread_run(user_thread_fn, &thread_ids[i], "refcnt_user%d", i+1);
        if (IS_ERR(user_threads[i])) {
            pr_err("%s: Failed to create thread %d\n", __func__, i+1);
            while (--i >= 0)
                kthread_stop(user_threads[i]);
            put_resource(shared_resource);
            return PTR_ERR(user_threads[i]);
        }
    }
    
    return 0;
}

static void __exit refcount_example_exit(void)
{
    int i;
    
    for (i = 0; i < 4; i++) {
        if (user_threads[i])
            kthread_stop(user_threads[i]);
    }
    
    spin_lock(&resource_lock);
    if (shared_resource) {
        shared_resource->active = false;
        put_resource(shared_resource);
        shared_resource = NULL;
    }
    spin_unlock(&resource_lock);
    
    pr_info("%s: Refcount example module unloaded\n", __func__);
}

module_init(refcount_example_init);
module_exit(refcount_example_exit);