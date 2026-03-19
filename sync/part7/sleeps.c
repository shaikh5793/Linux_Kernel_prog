/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/hrtimer.h>
#include <linux/wait.h>
#include <linux/slab.h>

static struct task_struct *test_thread;
static DECLARE_WAIT_QUEUE_HEAD(test_wq);
static bool test_event = false;

/* A simple thread function that demonstrates various sleeps */
static int test_thread_fn(void *data)
{
	unsigned long start, end;

	pr_info("Thread: Starting sleeps demonstration...\n");

	/* Jiffies-based measure */
	start = jiffies;
	msleep(100);
	end = jiffies;
	pr_info("Thread: msleep(100) took about %u ms\n",
		jiffies_to_msecs(end - start));

	/* usleep_range - finer granularity than msleep */
	pr_info("Thread: usleep_range(500,1000)...\n");
	usleep_range(500, 1000);
	pr_info("Thread: Woke after usleep_range.\n");

	/* schedule_timeout */
	pr_info
	    ("Thread: Using schedule_timeout(200ms) in interruptible state...\n");
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout(msecs_to_jiffies(200));
	pr_info("Thread: Woke from schedule_timeout.\n");

	/* wait_event_timeout to simulate waiting for an event or timeout */
	pr_info("Thread: Waiting for event or 300 ms timeout...\n");
	if (wait_event_timeout(test_wq, test_event, msecs_to_jiffies(300))) {
		pr_info("Thread: Event occurred!\n");
	} else {
		pr_info("Thread: Timeout occurred, no event.\n");
	}

	pr_info("Thread: Sleeps demonstration complete.\n");
	return 0;
}

static int __init sleep_demo_init(void)
{
	pr_info("Sleep Demo Module: init\n");
	test_thread = kthread_run(test_thread_fn, NULL, "sleep_demo_thread");
	if (IS_ERR(test_thread)) {
		pr_err("Failed to create test thread.\n");
		return PTR_ERR(test_thread);
	}
	return 0;
}

static void __exit sleep_demo_exit(void)
{
	/* In a real scenario, you might signal the thread to stop and wait for it */
	if (test_thread)
		kthread_stop(test_thread);
	pr_info("Sleep Demo Module: exit\n");
}

module_init(sleep_demo_init);
module_exit(sleep_demo_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("Demonstration of kernel sleeps and delays");
