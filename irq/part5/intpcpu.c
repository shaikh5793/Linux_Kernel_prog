/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/smp.h>
#include <linux/stacktrace.h>

static struct hrtimer hr_timer;

enum hrtimer_restart timer_callback(struct hrtimer *timer)
{
    pr_info("Timer callback on CPU %d\n", smp_processor_id());

    // Print the stack trace
    dump_stack();

    // Re-arm the timer
    hrtimer_forward_now(timer, ms_to_ktime(100));

    return HRTIMER_RESTART;
}

static int __init my_module_init(void)
{
    ktime_t ktime;

    // Initialize the timer to fire on the current CPU
    ktime = ms_to_ktime(100);
    hrtimer_init(&hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL_PINNED);
    hr_timer.function = &timer_callback;

    pr_info("Starting timer on CPU %d\n", smp_processor_id());

    // Start the timer
    hrtimer_start(&hr_timer, ktime, HRTIMER_MODE_REL_PINNED);

    return 0;
}

static void __exit my_module_exit(void)
{
    int ret;

    // Cancel the timer if it is still active
    ret = hrtimer_cancel(&hr_timer);
    if (ret)
        pr_info("Timer was still in use...\n");

    pr_info("Timer module unloaded\n");
}

module_init(my_module_init);
module_exit(my_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("Per-CPU Timer Interrupt Handler Example with Stack Dump");

