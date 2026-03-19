/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/slab.h>
#include <linux/io.h>

#define GPIO_PIN 45 // GPIO1_13 corresponds to GPIO number 45

static unsigned int irq;
static int irq_counter = 0;
static struct gpio_desc *gpio_button;

static irqreturn_t interrupt_handler(int irq, void *dev_id)
{
    irq_counter++;
    pr_info("In the ISR; counter = %d\n", irq_counter);
    return IRQ_HANDLED;
}

static int __init gpio_irq_init(void)
{
    int ret;

    pr_info("GPIO Interrupt Module Init\n");

    gpio_button = gpio_to_desc(GPIO_PIN);
    if (!gpio_button)
    {
        pr_err("gpio_to_desc failed\n");
        return -EINVAL;
    }

    ret = gpiod_direction_input(gpio_button);
    if (ret)
    {
        pr_err("gpiod_direction_input failed\n");
        return ret;
    }

    gpiod_set_debounce(gpio_button, 200);

    irq = gpiod_to_irq(gpio_button);
    if (irq < 0)
    {
        pr_err("gpiod_to_irq failed\n");
        return irq;
    }

    ret = request_irq(irq, interrupt_handler, IRQF_TRIGGER_RISING, "gpio_button_interrupt", NULL);
    if (ret)
    {
        pr_err("request_irq failed\n");
        return ret;
    }

    pr_info("GPIO Interrupt Module Loaded Successfully\n");
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
MODULE_DESCRIPTION("GPIO Interrupts using GPIO Descriptor API on BeagleBone Black");
MODULE_LICENSE("Dual MIT/GPL");
