/*
Copyright (c) 2024 TECH VEDA(www.techveda.org)
Author: Raghu Bharadwaj

This software is dual-licensed under the MIT License and GPL v2.
See the accompanying LICENSE file for the full text.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kref.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/delay.h>

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("Kernel reference counting with kref");
MODULE_VERSION("1.0");

struct my_object {
    struct kref refcount;
    int data;
    char name[32];
};

static struct my_object *global_obj = NULL;
static struct task_struct *thread1, *thread2, *thread3;

static void my_object_release(struct kref *kref)
{
    struct my_object *obj = container_of(kref, struct my_object, refcount);
    pr_info("%s: Releasing object '%s' with data=%d\n", __func__, obj->name, obj->data);
    kfree(obj);
}

static struct my_object *create_object(const char *name, int data)
{
    struct my_object *obj = kmalloc(sizeof(*obj), GFP_KERNEL);
    if (!obj)
        return NULL;
    
    kref_init(&obj->refcount);
    obj->data = data;
    strncpy(obj->name, name, sizeof(obj->name) - 1);
    obj->name[sizeof(obj->name) - 1] = '\0';
    
    pr_info("%s: Created object '%s' with refcount=1\n", __func__, obj->name);
    return obj;
}

static int worker_thread(void *thread_id)
{
    int id = *(int *)thread_id;
    struct my_object *obj;
    int i;
    
    pr_info("%s: Worker thread %d started\n", __func__, id);
    
    for (i = 0; i < 5 && !kthread_should_stop(); i++) {
        if (global_obj) {
            kref_get(&global_obj->refcount);
            obj = global_obj;
            pr_info("%s: Thread %d got reference, using object '%s'\n", __func__, id, obj->name);
            
            msleep(100);
            
            obj->data += id;
            pr_info("%s: Thread %d modified data to %d\n", __func__, id, obj->data);
            
            kref_put(&obj->refcount, my_object_release);
            pr_info("%s: Thread %d released reference\n", __func__, id);
        }
        
        msleep(200);
    }
    
    pr_info("%s: Worker thread %d exiting\n", __func__, id);
    return 0;
}

static int __init kref_example_init(void)
{
    static int thread_ids[] = {1, 2, 3};
    
    pr_info("%s: Kref example module loaded\n", __func__);
    
    global_obj = create_object("shared_object", 100);
    if (!global_obj) {
        pr_err("%s: Failed to create object\n", __func__);
        return -ENOMEM;
    }
    
    thread1 = kthread_run(worker_thread, &thread_ids[0], "kref_worker1");
    if (IS_ERR(thread1)) {
        pr_err("%s: Failed to create thread1\n", __func__);
        kref_put(&global_obj->refcount, my_object_release);
        return PTR_ERR(thread1);
    }
    
    thread2 = kthread_run(worker_thread, &thread_ids[1], "kref_worker2");
    if (IS_ERR(thread2)) {
        pr_err("%s: Failed to create thread2\n", __func__);
        kthread_stop(thread1);
        kref_put(&global_obj->refcount, my_object_release);
        return PTR_ERR(thread2);
    }
    
    thread3 = kthread_run(worker_thread, &thread_ids[2], "kref_worker3");
    if (IS_ERR(thread3)) {
        pr_err("%s: Failed to create thread3\n", __func__);
        kthread_stop(thread1);
        kthread_stop(thread2);
        kref_put(&global_obj->refcount, my_object_release);
        return PTR_ERR(thread3);
    }
    
    return 0;
}

static void __exit kref_example_exit(void)
{
    if (thread1)
        kthread_stop(thread1);
    if (thread2)
        kthread_stop(thread2);
    if (thread3)
        kthread_stop(thread3);
    
    if (global_obj) {
        kref_put(&global_obj->refcount, my_object_release);
        global_obj = NULL;
    }
    
    pr_info("%s: Kref example module unloaded\n", __func__);
}

module_init(kref_example_init);
module_exit(kref_example_exit);