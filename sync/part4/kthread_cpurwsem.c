/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/rwsem.h>
#include <linux/percpu.h>

#define MODNAME "[SYNC_RWSEM] "


DEFINE_PER_CPU(unsigned int, counter);
DEFINE_PER_CPU(struct rw_semaphore, counter_rwsem);

struct task_struct *read_thread, *read_thread2, *write_thread, *write_thread2;

static int writer_function(void *data)
{
    while (!kthread_should_stop()) {
        down_write(this_cpu_ptr(&counter_rwsem));
        this_cpu_inc(counter);

        downgrade_write(this_cpu_ptr(&counter_rwsem));
        pr_info(MODNAME "(writer) counter: %d\n", this_cpu_read(counter));
        up_read(this_cpu_ptr(&counter_rwsem));
        msleep(500);
    }
    return 0;
}

static int read_function(void *data)
{
    while (!kthread_should_stop()) {
        down_read(this_cpu_ptr(&counter_rwsem));
        pr_info(MODNAME "counter: %d\n", this_cpu_read(counter));
        up_read(this_cpu_ptr(&counter_rwsem));
        msleep(500);
    }
    return 0;
}

static int __init my_mod_init(void)
{
    int cpu;

    for_each_possible_cpu(cpu) {
        init_rwsem(per_cpu_ptr(&counter_rwsem, cpu));
    }

    read_thread = kthread_run(read_function, NULL, "read-thread");
    read_thread2 = kthread_run(read_function, NULL, "read-thread2");
    write_thread = kthread_run(writer_function, NULL, "write-thread");
    write_thread2 = kthread_run(writer_function, NULL, "write-thread");

    return 0;
}

static void __exit my_mod_exit(void)
{
    kthread_stop(read_thread);
    kthread_stop(write_thread);
    kthread_stop(read_thread2);
    kthread_stop(write_thread2);
}

module_init(my_mod_init);
module_exit(my_mod_exit);

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("per-CPU rwsemaphores");
MODULE_LICENSE("Dual MIT/GPL");
