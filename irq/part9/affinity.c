/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/smp.h>
#include <linux/cpumask.h>
#include <linux/irq.h>

#define MY_IRQ 1

static irqreturn_t my_irq_handler(int irq, void *dev_id) {
    pr_info("Interrupt handled on CPU %d\n", smp_processor_id());
    return IRQ_HANDLED;
}

static int __init my_init(void) {
    int ret;
    struct cpumask mask;

    // Request the IRQ
    ret = request_irq(MY_IRQ, my_irq_handler, IRQF_SHARED, "my_interrupt", (void *)(my_irq_handler));
    if (ret) {
        pr_err("Failed to request IRQ %d\n", MY_IRQ);
        return ret;
    }

    // Set the affinity to CPU core 1
    cpumask_clear(&mask);
    cpumask_set_cpu(1, &mask);
    ret = irq_set_affinity(MY_IRQ, &mask);
    if (ret) {
        pr_err("Failed to set IRQ affinity for IRQ %d\n", MY_IRQ);
        free_irq(MY_IRQ, (void *)(my_irq_handler));
        return ret;
    }

    pr_info("Module loaded and IRQ handler registered\n");
    return 0;
}

static void __exit my_exit(void) {
    // Free the IRQ
    free_irq(MY_IRQ, (void *)(my_irq_handler));
    printk(KERN_INFO "Module unloaded and IRQ handler unregistered\n");
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("A simple kernel module to set up an interrupt handler with enforced affinity to CPU core 0");
MODULE_VERSION("1.0");

