/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <linux/hrtimer.h>
#include <linux/sched.h>

static struct workqueue_struct *hr_wq;
struct delayed_work my_delayed_work;

void my_work_handler(struct work_struct *work) {
    pr_info("%s: Executed on CPU %d, in %s context, called by %s (pid %d)\n", 
            __func__, smp_processor_id(), 
            in_interrupt() ? "interrupt" : "process",
            current->comm, current->pid);
    schedule_delayed_work((struct delayed_work *)work, msecs_to_jiffies(100));  // Reschedule the work
}

static int __init my_module_init(void) {
    hr_wq = create_workqueue("hr_workqueue");
    if (!hr_wq) {
        pr_err("Failed to create high-resolution timer workqueue\n");
        return -ENOMEM;
    }

    INIT_DELAYED_WORK(&my_delayed_work, my_work_handler);
    queue_delayed_work(hr_wq, &my_delayed_work, msecs_to_jiffies(100));
    pr_info("Delayed work queued with high-resolution timer\n");

    return 0;
}

static void __exit my_module_exit(void) {
    cancel_delayed_work_sync(&my_delayed_work);
    flush_workqueue(hr_wq);
    destroy_workqueue(hr_wq);
    pr_info("High-resolution timer workqueue destroyed\n");
}

module_init(my_module_init);
module_exit(my_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("High-Resolution Timer Workqueue Example");

