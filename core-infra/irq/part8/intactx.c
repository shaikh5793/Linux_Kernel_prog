/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/stacktrace.h>

#define SHARED_IRQ 89

static int irq = SHARED_IRQ, my_dev_id = 1, irq_counter = 0;
module_param(irq, int, S_IRUGO);

static irqreturn_t my_interrupt(int irq, void *dev_id)
{
    irq_counter++;

    if (in_interrupt()) {
        pr_info("In the ISR: counter = %d, context = interrupt\n", irq_counter);
    } else {
        pr_info("In the ISR: counter = %d, context = thread\n", irq_counter);
    }

//    dump_stack();

    return IRQ_NONE; /* we return IRQ_NONE because we are just observing */
    /* return IRQ_HANDLED; */
}

static int __init my_init(void)
{
    int ret;

    ret = request_any_context_irq(irq, my_interrupt, IRQF_SHARED, "my_interrupt", &my_dev_id);
    if (ret < 0) {
        pr_err("Failed to request IRQ %d: %d\n", irq, ret);
        return -1;
    }

    if (ret == IRQC_IS_HARDIRQ) {
        pr_info("Successfully requested IRQ %d: Handler will run in hard IRQ context\n", irq);
    } else if (ret == IRQC_IS_NESTED) {
        pr_info("Successfully requested IRQ %d: Handler will run in nested context\n", irq);
    } else {
        pr_warn("Successfully requested IRQ %d: Unknown context\n", irq);
    }
    
    return 0;
}

static void __exit my_exit(void)
{
    free_irq(irq, &my_dev_id);
    pr_info("Freed IRQ %d\n", irq);
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("Interrupt Handler Example using request_any_context_irq with Context Verification");

