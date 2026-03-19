/*
Copyright (c) 2024 TECH VEDA(www.techveda.org)
Author: Raghu Bharadwaj

This software is dual-licensed under the MIT License and GPL v2.
See the accompanying LICENSE file for the full text.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/srcu.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/slab.h>

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("Sleepable RCU demonstration");
MODULE_VERSION("1.0");

struct shared_data {
    struct rcu_head rcu;
    int value;
    char name[32];
};

static struct shared_data __rcu *global_data = NULL;
static struct task_struct *reader_threads[3];
static struct task_struct *writer_thread;
DEFINE_SRCU(demo_srcu);

static void free_data_rcu(struct rcu_head *head)
{
    struct shared_data *data = container_of(head, struct shared_data, rcu);
    pr_info("%s: Freeing data with value=%d, name='%s'\n", __func__, data->value, data->name);
    kfree(data);
}

static struct shared_data *create_data(int value, const char *name)
{
    struct shared_data *data = kmalloc(sizeof(*data), GFP_KERNEL);
    if (!data)
        return NULL;
    
    data->value = value;
    strncpy(data->name, name, sizeof(data->name) - 1);
    data->name[sizeof(data->name) - 1] = '\0';
    
    return data;
}

static int reader_fn(void *data)
{
    int reader_id = *(int *)data;
    struct shared_data *local_data;
    int idx;
    int reads = 0;
    
    pr_info("%s: SRCU Reader %d started\n", __func__, reader_id);
    
    while (!kthread_should_stop() && reads < 15) {
        idx = srcu_read_lock(&demo_srcu);
        
        local_data = rcu_dereference(global_data);
        if (local_data) {
            pr_info("%s: Reader %d read value=%d, name='%s'\n", 
                    __func__, reader_id, local_data->value, local_data->name);
            
            msleep(200);
            
            pr_info("%s: Reader %d continuing with value=%d\n", 
                    __func__, reader_id, local_data->value);
        } else {
            pr_info("%s: Reader %d found no data\n", __func__, reader_id);
        }
        
        srcu_read_unlock(&demo_srcu, idx);
        reads++;
        
        msleep(300 + (reader_id * 100));
    }
    
    pr_info("%s: SRCU Reader %d exiting after %d reads\n", __func__, reader_id, reads);
    return 0;
}

static int writer_fn(void *data)
{
    struct shared_data *new_data, *old_data;
    int i;
    
    pr_info("%s: SRCU Writer started\n", __func__);
    
    for (i = 0; i < 8 && !kthread_should_stop(); i++) {
        new_data = create_data(i * 10, i % 2 ? "even_data" : "odd_data");
        if (!new_data) {
            pr_err("%s: Failed to allocate new data\n", __func__);
            continue;
        }
        
        pr_info("%s: Writer updating to value=%d, name='%s'\n", 
                __func__, new_data->value, new_data->name);
        
        old_data = rcu_dereference_protected(global_data, 1);
        rcu_assign_pointer(global_data, new_data);
        
        if (old_data) {
            synchronize_srcu(&demo_srcu);
            call_rcu(&old_data->rcu, free_data_rcu);
        }
        
        pr_info("%s: Writer completed update %d\n", __func__, i);
        msleep(1000);
    }
    
    pr_info("%s: SRCU Writer exiting\n", __func__);
    return 0;
}

static int __init srcu_example_init(void)
{
    static int reader_ids[] = {1, 2, 3};
    int i;
    
    pr_info("%s: SRCU example module loaded\n", __func__);
    
    global_data = create_data(0, "initial_data");
    if (!global_data) {
        pr_err("%s: Failed to create initial data\n", __func__);
        return -ENOMEM;
    }
    
    for (i = 0; i < 3; i++) {
        reader_threads[i] = kthread_run(reader_fn, &reader_ids[i], 
                                       "srcu_reader%d", i+1);
        if (IS_ERR(reader_threads[i])) {
            pr_err("%s: Failed to create reader %d\n", __func__, i+1);
            while (--i >= 0)
                kthread_stop(reader_threads[i]);
            kfree(global_data);
            return PTR_ERR(reader_threads[i]);
        }
    }
    
    writer_thread = kthread_run(writer_fn, NULL, "srcu_writer");
    if (IS_ERR(writer_thread)) {
        pr_err("%s: Failed to create writer thread\n", __func__);
        for (i = 0; i < 3; i++)
            kthread_stop(reader_threads[i]);
        kfree(global_data);
        return PTR_ERR(writer_thread);
    }
    
    return 0;
}

static void __exit srcu_example_exit(void)
{
    int i;
    struct shared_data *data;
    
    if (writer_thread)
        kthread_stop(writer_thread);
    
    for (i = 0; i < 3; i++) {
        if (reader_threads[i])
            kthread_stop(reader_threads[i]);
    }
    
    data = rcu_dereference_protected(global_data, 1);
    if (data) {
        synchronize_srcu(&demo_srcu);
        kfree(data);
    }
    
    cleanup_srcu_struct(&demo_srcu);
    
    pr_info("%s: SRCU example module unloaded\n", __func__);
}

module_init(srcu_example_init);
module_exit(srcu_example_exit);