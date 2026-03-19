/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/uio_driver.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/of.h>
#include <linux/of_device.h>

static struct uio_info *info;
static struct device *dev;
static struct gpio_desc *gpio_button;
static int irq;
module_param(irq, int, S_IRUGO);

static void my_release(struct device *dev)
{
    pr_info("releasing my uio device\n");
}

static irqreturn_t my_handler(int irq, struct uio_info *dev_info)
{
    static int count = 0;
    pr_info("In UIO handler, count=%d\n", ++count);
    
    return IRQ_HANDLED;
}

static int __init my_init(void)
{
    struct device_node *node;
    int ret;

    pr_info("GPIO UIO Module Init\n");

    // Find the device tree node
    node = of_find_node_by_path("/extbutton");
    if (!node)
    {
        pr_err("Device tree node /extbutton not found\n");
        return -ENODEV;
    }

    // Get the GPIO descriptor
    gpio_button = gpiod_get_from_of_node(node, "gpios", 0, GPIOD_IN, "gpio_button");
    if (IS_ERR(gpio_button))
    {
        pr_err("gpiod_get_from_of_node failed\n");
        return PTR_ERR(gpio_button);
    }

    pr_info("GPIO descriptor obtained successfully\n");

    gpiod_set_debounce(gpio_button, 200);

    irq = gpiod_to_irq(gpio_button);
    if (irq < 0)
    {
        pr_err("gpiod_to_irq failed\n");
        ret = irq;
        gpiod_put(gpio_button);
        return ret;
    }

    pr_info("IRQ number obtained successfully: %d\n", irq);

    // Allocate and initialize the device
    dev = kzalloc(sizeof(struct device), GFP_KERNEL);
    if (!dev)
    {
        gpiod_put(gpio_button);
        return -ENOMEM;
    }
    dev_set_name(dev, "my_uio_device");
    dev->release = my_release;
    ret = device_register(dev);
    if (ret)
    {
        pr_err("device_register failed\n");
        kfree(dev);
        gpiod_put(gpio_button);
        return ret;
    }

    // Allocate and initialize the UIO info structure
    info = kzalloc(sizeof(struct uio_info), GFP_KERNEL);
    if (!info)
    {
        device_unregister(dev);
        kfree(dev);
        gpiod_put(gpio_button);
        return -ENOMEM;
    }

    info->name = "my_uio_device";
    info->version = "0.0.1";
    info->irq = irq;
    info->irq_flags = IRQF_TRIGGER_RISING;
    info->handler = my_handler;

    ret = uio_register_device(dev, info);
    if (ret)
    {
        pr_err("uio_register_device failed with error %d\n", ret);
        kfree(info);
        device_unregister(dev);
        kfree(dev);
        gpiod_put(gpio_button);
        return ret;
    }

    pr_info("Registered UIO handler for IRQ=%d\n", irq);
    return 0;
}

static void __exit my_exit(void)
{
    pr_info("GPIO UIO Module Exit\n");
    uio_unregister_device(info);
    device_unregister(dev);
    pr_info("Un-Registered UIO handler for IRQ=%d\n", irq);
    kfree(info);
    kfree(dev);
    gpiod_put(gpio_button);
}

module_init(my_init);
module_exit(my_exit);

MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("GPIO Interrupts using UIO Subsystem on BeagleBone Black");
MODULE_LICENSE("GPL");
