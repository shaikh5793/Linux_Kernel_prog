/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/of.h>
#include <linux/of_gpio.h>

static struct gpio_desc *gpio_button;
static int irq;
static atomic_t button_count = ATOMIC_INIT(0);

static irqreturn_t gpio_button_context_handler(int irq, void *dev_id)
{
	unsigned long flags;
	int count;
	int gpio_val;
	
	/* Increment button press counter */
	count = atomic_inc_return(&button_count);
	
	/* Read current GPIO value */
	gpio_val = gpiod_get_value(gpio_button);
	
	/* Display context information */
	pr_info("=== Button Press #%d (GPIO value: %d) ===\n", count, gpio_val);
	
	/* Check if we're in hardirq or thread context */
	if (in_irq()) {
		pr_info("  Running in: HARDIRQ context\n");
	} else if (in_softirq()) {
		pr_info("  Running in: SOFTIRQ context\n");
	} else {
		pr_info("  Running in: THREAD context\n");
	}
	
	local_irq_save(flags);
	pr_info("  Context: %s (PID: %d), CPU: %d\n",
		current->comm, current->pid, smp_processor_id());
	local_irq_restore(flags);
	
	return IRQ_HANDLED;
}

static int __init gpio_context_init(void)
{
	struct device_node *node;
	int ret;
	
	pr_info("=== GPIO request_any_context_irq() Demo ===\n");
	pr_info("BeagleBone Black GPIO interrupt with context detection\n");
	
	/* Find the device tree node for our button */
	node = of_find_node_by_path("/extbutton");
	if (!node) {
		pr_err("Device tree node /extbutton not found\n");
		pr_err("Make sure device tree is configured correctly\n");
		return -ENODEV;
	}
	
	/* Get the GPIO descriptor from device tree using the working API */
	gpio_button = gpiod_get_from_of_node(node, "gpios", 0, GPIOD_IN, "gpio_button");
	if (IS_ERR(gpio_button)) {
		pr_err("Failed to get GPIO descriptor: %ld\n", PTR_ERR(gpio_button));
		of_node_put(node);
		return PTR_ERR(gpio_button);
	}
	of_node_put(node);
	
	pr_info("GPIO descriptor obtained successfully\n");
	
	/* Configure debouncing for button stability */
	gpiod_set_debounce(gpio_button, 200);
	
	/* Convert GPIO to IRQ number */
	irq = gpiod_to_irq(gpio_button);
	if (irq < 0) {
		pr_err("Failed to convert GPIO to IRQ: %d\n", irq);
		gpiod_put(gpio_button);
		return irq;
	}
	
	pr_info("GPIO IRQ number: %d\n", irq);
	
	/*
	 * request_any_context_irq() allows kernel to choose context
	 * Return values:
	 * - IRQC_IS_HARDIRQ: Hard IRQ context
	 * - IRQC_IS_NESTED: Threaded/nested context
	 */
	ret = request_any_context_irq(irq, gpio_button_context_handler, 
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
				"gpio_context_irq", NULL);
	if (ret < 0) {
		pr_err("Failed to request IRQ %d: %d\n", irq, ret);
		gpiod_put(gpio_button);
		return ret;
	}
	
	/* Interpret context selection */
	if (ret == IRQC_IS_HARDIRQ) {
		pr_info("IRQ %d: Handler will run in HARD IRQ context\n", irq);
	} else if (ret == IRQC_IS_NESTED) {
		pr_info("IRQ %d: Handler will run in THREADED context\n", irq);
	}
	
	pr_info("GPIO interrupt handler registered successfully\n");
	pr_info("Press the button to see context detection\n");
	return 0;
}

static void __exit gpio_context_exit(void)
{
	pr_info("GPIO context detection module unloading\n");
	free_irq(irq, NULL);
	gpiod_put(gpio_button);
	pr_info("Final button press count: %d\n", atomic_read(&button_count));
	pr_info("GPIO context detection module unloaded\n");
}

module_init(gpio_context_init);
module_exit(gpio_context_exit);

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("GPIO-based request_any_context_irq() demonstration");
MODULE_LICENSE("Dual MIT/GPL");

