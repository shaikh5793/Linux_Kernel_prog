/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/gpio/consumer.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/io.h>

static unsigned int irq;
static int irq_counter = 0;
static struct gpio_desc *gpio_button;

static irqreturn_t irq_thread_fn(int irq, void *dev_id)
{
    irq_counter++;
    pr_info("In the threaded ISR; counter = %d\n", irq_counter);
    return IRQ_HANDLED;
}

static int __init gpio_irq_init(void)
{
    struct device_node *node;
    int ret;

    pr_info("GPIO Interrupt Module Init\n");

    // Find the device tree node
    node = of_find_node_by_path("/extbutton");
    if (!node)
    {
        pr_err("Device tree node /extbutton not found\n");
        return -ENODEV;
    }

    // Get the GPIO descriptor
    gpio_button = gpiod_get_from_of_node(node, "gpios", 0, GPIOD_ASIS, "gpio_button");
    if (IS_ERR(gpio_button))
    {
        pr_err("gpiod_get_from_of_node failed\n");
        return PTR_ERR(gpio_button);
    }

    pr_info("GPIO descriptor obtained successfully\n");

    // Set GPIO direction to input
    ret = gpiod_direction_input(gpio_button);
    if (ret)
    {
        pr_err("gpiod_direction_input failed\n");
        return ret;
    }

    pr_info("GPIO direction set to input successfully\n");

    // Set debounce
    gpiod_set_debounce(gpio_button, 200);

    // Get IRQ number
    irq = gpiod_to_irq(gpio_button);
    if (irq < 0)
    {
        pr_err("gpiod_to_irq failed\n");
        return irq;
    }

    pr_info("IRQ number obtained successfully: %d\n", irq);

    // Request threaded IRQ
    ret = request_threaded_irq(irq, NULL, irq_thread_fn, IRQF_TRIGGER_RISING | IRQF_ONESHOT, "gpio_button_interrupt", NULL);
    if (ret)
    {
        pr_err("request_threaded_irq failed\n");
        return ret;
    }

    pr_info("Threaded IRQ requested successfully\n");

    return 0;
}

static void __exit gpio_irq_exit(void)
{
    pr_info("GPIO Interrupt Module Exit\n");
    free_irq(irq, NULL);
    gpiod_put(gpio_button);
    pr_info("GPIO Interrupt Module Unloaded Successfully\n");
}

module_init(gpio_irq_init);
module_exit(gpio_irq_exit);

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("GPIO Interrupts using Threaded Interrupt Handler with IRQF_ONESHOT on BeagleBone Black");
MODULE_LICENSE("Dual MIT/GPL");
