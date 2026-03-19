/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/uio_driver.h>

static struct uio_info *info;
static struct device *dev;
static int irq = 1;
module_param(irq, int, S_IRUGO);

static void my_release(struct device *dev)
{
	pr_info("releasing my uio device\n");
}

static irqreturn_t my_handler(int irq, struct uio_info *dev_info)
{
	static int count = 0;
	pr_info("In UIO handler, count=%d\n", ++count);
	/* must return IRQ_HANDLED for event to reach uspace */
	return IRQ_HANDLED;
}

static int __init my_init(void)
{
	int ret;
	dev = kzalloc(sizeof(struct device), GFP_KERNEL);
	dev_set_name(dev, "my_uio_device");
	dev->release = my_release;
	ret = device_register(dev);

	info = kzalloc(sizeof(struct uio_info), GFP_KERNEL);

	/* irq information */
	info->name = "my_uio_device";
	info->version = "0.0.1";
	info->irq = irq;
	info->irq_flags = IRQF_SHARED;
	info->handler = my_handler;

	if (uio_register_device(dev, info) < 0) {
		device_unregister(dev);
		kfree(dev);
		kfree(info);
		pr_info("Failing to register uio device\n");
		return -1;
	}
	pr_info("Registered UIO handler for IRQ=%d\n", irq);
	return 0;
}

static void __exit my_exit(void)
{
	uio_unregister_device(info);
	device_unregister(dev);
	pr_info("Un-Registered UIO handler for IRQ=%d\n", irq);
	kfree(info);
	kfree(dev);
}

module_init(my_init);
module_exit(my_exit);

MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("LKDCW: Interrupt routing using uio");
MODULE_LICENSE("GPL");
