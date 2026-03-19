/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>

#define SHARED_IRQ 1
static int irq = SHARED_IRQ, my_dev_id = 1, irq_counter = 0;
module_param(irq, int, S_IRUGO);

/* This routine runs in hard interrupt context and is needed for 
   1. check and validate interrupt source
   2. wake up thread context interrupt handler.
*/
static irqreturn_t my_interrupt(int irq, void *dev_id)
{
	/* ensure interupt is valid and handover control to thread handler */
	dump_stack();
	return IRQ_WAKE_THREAD;
}

static irqreturn_t my_intthread(int irq, void *dev_id)
{
	irq_counter++;
	dump_stack();
	pr_info("In the ISR: counter = %d\n", irq_counter);
	return IRQ_NONE;	/* we return IRQ_NONE because we are just observing */
	/*return IRQ_HANDLED; */
}

static int __init my_init(void)
{
	if (request_threaded_irq
	    (irq, my_interrupt, my_intthread, IRQF_SHARED,
	     "my_interrupt", &my_dev_id))
		return -1;
	return 0;
}

static void __exit my_exit(void)
{
	/* verify if isr is running */
	synchronize_irq(irq);
	free_irq(irq, &my_dev_id);
}

module_init(my_init);
module_exit(my_exit);

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("Interrupt Handler Test");
MODULE_LICENSE("Dual MIT/GPL");
/* 
 * my_hard_inthandler()
 * {
 * 	1. disable device interrupts(through register interface)
 * 	2. return IRQ_WAKE_THREAD;
 * 
  	

 * my_threadhandler()
 * {
 *
 *  	code to handle hard interrupt
 *  	enable device interrupts(through register interface)
 *  	code to handler hard interrupt non-critical(soft interrupt critical)  work
 *  }
 */
