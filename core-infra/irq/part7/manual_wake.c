/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/jiffies.h>

#define SHARED_IRQ 1

static int irq = SHARED_IRQ;
static int my_dev_id = 1;
static int thread_counter = 0;
static int manual_wake_counter = 0;
static int hardware_wake_counter = 0;
static struct timer_list manual_wake_timer;

module_param(irq, int, S_IRUGO);
MODULE_PARM_DESC(irq, "IRQ number to use (default: 1)");

static irqreturn_t my_hard_handler(int irq, void *dev_id)
{
	/* This is the hard interrupt handler - runs in atomic context */
	hardware_wake_counter++;
	pr_info("Hard IRQ: Hardware interrupt #%d - waking thread\n", 
		hardware_wake_counter);
	
	/* Wake up the threaded handler */
	return IRQ_WAKE_THREAD;
}

static irqreturn_t my_threaded_handler(int irq, void *dev_id)
{
	thread_counter++;
	pr_info("Threaded handler: execution #%d (hw:%d + manual:%d)\n", 
		thread_counter, hardware_wake_counter, manual_wake_counter);
	
	/* Demonstrate that we can sleep here */
	msleep(50);
	
	pr_info("Threaded handler: processing completed\n");
	return IRQ_HANDLED;
}

static void manual_wake_timer_func(struct timer_list *t)
{
	manual_wake_counter++;
	pr_info("Timer: Manual wake #%d - calling irq_wake_thread()\n", 
		manual_wake_counter);
	
	/*
	 * irq_wake_thread() - Manually wake up the threaded interrupt handler
	 * This allows software to trigger the threaded handler without
	 * an actual hardware interrupt occurring
	 */
	irq_wake_thread(irq, &my_dev_id);
	
	/* Schedule next manual wake in 5 seconds */
	if (manual_wake_counter < 3) {
		pr_info("Scheduling next manual wake in 5 seconds...\n");
		mod_timer(&manual_wake_timer, jiffies + msecs_to_jiffies(5000));
	} else {
		pr_info("Manual wake demo completed (3 manual wakes done)\n");
	}
}

static int __init manual_wake_init(void)
{
	int ret;
	
	pr_info("=== Manual Wake Thread Demo ===\n");
	pr_info("Demonstrating irq_wake_thread() API\n");
	
	/* Register threaded interrupt with both hard and threaded handlers */
	ret = request_threaded_irq(irq, my_hard_handler, my_threaded_handler,
				   IRQF_SHARED, "manual_wake_demo", &my_dev_id);
	if (ret) {
		pr_err("Failed to request threaded IRQ %d: %d\n", irq, ret);
		return ret;
	}
	
	/* Setup timer for automatic manual wake demonstration */
	timer_setup(&manual_wake_timer, manual_wake_timer_func, 0);
	
	pr_info("Manual wake demo loaded\n");
	pr_info("Hardware interrupts on IRQ %d will trigger threaded handler\n", irq);
	pr_info("Timer will also trigger manual wakes every 5 seconds\n");
	pr_info("Starting first manual wake in 3 seconds...\n");
	
	/* Start first manual wake in 3 seconds */
	mod_timer(&manual_wake_timer, jiffies + msecs_to_jiffies(3000));
	
	return 0;
}

static void __exit manual_wake_exit(void)
{
	pr_info("Cleaning up manual wake demo\n");
	
	/* Delete timer */
	del_timer_sync(&manual_wake_timer);
	
	/* Synchronize with any running handlers */
	synchronize_irq(irq);
	
	/* Free the IRQ */
	free_irq(irq, &my_dev_id);
	
	pr_info("Final statistics:\n");
	pr_info("  Total threaded handler calls: %d\n", thread_counter);
	pr_info("  Hardware-triggered calls: %d\n", hardware_wake_counter);
	pr_info("  Manual wake calls: %d\n", manual_wake_counter);
	pr_info("Manual wake demo unloaded\n");
}

module_init(manual_wake_init);
module_exit(manual_wake_exit);

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("Manual Thread Wake Demo using irq_wake_thread()");
MODULE_LICENSE("Dual MIT/GPL");

/*
 * Key Concepts Demonstrated:
 *
 * 1. irq_wake_thread() API:
 *    - Manually triggers threaded interrupt handler
 *    - Useful for software-triggered processing
 *    - Allows complex interrupt sharing scenarios
 *    - Thread runs in same context as hardware-triggered
 *
 * 2. Dual triggering mechanism:
 *    - Hardware interrupts: hard handler returns IRQ_WAKE_THREAD
 *    - Software triggers: irq_wake_thread() called directly
 *    - Same threaded handler processes both types
 *    - Statistics track different trigger sources
 *
 * 3. Use cases for manual wake:
 *    - Software events that need same processing as hardware IRQ
 *    - Testing interrupt handlers without hardware stimulus
 *    - Complex state machines in device drivers
 *    - Error recovery and retry mechanisms
 *    - Timer-driven processing using IRQ infrastructure
 *
 * 4. Timer integration:
 *    - Demonstrates periodic manual wake triggering
 *    - Shows how software can simulate hardware events
 *    - Useful for testing and development
 *
 * Usage:
 * sudo insmod manual_wake.ko
 * 
 * # Watch kernel messages for automatic demo
 * dmesg -w
 * 
 * # Trigger hardware interrupts manually (if available)
 * # For IRQ 1: Type on keyboard
 * 
 * # The module will automatically demonstrate manual wakes
 * # via timer every 5 seconds (3 times total)
 * 
 * sudo rmmod manual_wake
 * 
 * Expected output:
 * - Timer triggers manual wakes automatically
 * - Hardware interrupts (keyboard) trigger normal flow
 * - Both use same threaded handler
 * - Statistics show breakdown of trigger sources
 */
