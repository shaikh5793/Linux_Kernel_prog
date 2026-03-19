/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/sched.h>

#define SHARED_IRQ 1
static int irq = SHARED_IRQ, my_dev_id, irq_counter = 0;
module_param(irq, int, S_IRUGO);

static void t_fun(struct tasklet_struct *);

static DECLARE_TASKLET(t_name, t_fun);

static irqreturn_t my_interrupt(int irq, void *dev_id)
{
	irq_counter++;
	pr_info("In the ISR: counter = %d\n", irq_counter);
	tasklet_hi_schedule(&t_name);
	return IRQ_NONE;
}
static int __init my_init(void)
{
    pr_info(" scheduling my tasklet, jiffies= %ld \n", jiffies);
    request_irq(SHARED_IRQ, my_interrupt, IRQF_SHARED, "myinterrupt", &my_dev_id);
    return 0;
}

static void __exit my_exit(void)
{
    pr_info("\nHello: cleanup_module loaded at address 0x%p\n",
        cleanup_module);
    free_irq(SHARED_IRQ, &my_dev_id);
}

static void t_fun(struct tasklet_struct *t_arg)
{
    dump_stack();
    pr_info("Entering t_fun, ,in context of %s with pid = %d\n",
        current->comm, current->pid);
}
module_init(my_init);
module_exit(my_exit);

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_LICENSE("Dual MIT/GPL");
