/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <linux/sched.h>

static struct workqueue_struct *percpu_wqs[NR_CPUS];
struct work_struct my_work;

void my_work_handler(struct work_struct *work) {
    pr_info("%s: Executed on CPU %d, in %s context, called by %s (pid %d)\n", 
            __func__, smp_processor_id(), 
            in_interrupt() ? "interrupt" : "process",
            current->comm, current->pid);
    schedule_work(work);  // Reschedule the work
}

static int __init my_module_init(void) {
    int cpu;

    for_each_possible_cpu(cpu) {
        percpu_wqs[cpu] = create_workqueue("percpu_workqueue");
        if (!percpu_wqs[cpu]) {
            pr_err("Failed to create per-CPU workqueue for CPU %d\n", cpu);
            return -ENOMEM;
        }
    }

    INIT_WORK(&my_work, my_work_handler);
    queue_work_on(smp_processor_id(), percpu_wqs[smp_processor_id()], &my_work);
    pr_info("Work queued on per-CPU workqueue\n");

    return 0;
}

static void __exit my_module_exit(void) {
    int cpu;

    for_each_possible_cpu(cpu) {
        if (percpu_wqs[cpu]) {
            flush_workqueue(percpu_wqs[cpu]);
            destroy_workqueue(percpu_wqs[cpu]);
        }
    }
    pr_info("Per-CPU workqueue destroyed\n");
}

module_init(my_module_init);
module_exit(my_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("Per-CPU Workqueue Example");

