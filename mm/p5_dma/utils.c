/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/device.h>

static void *kbuf;
static dma_addr_t handle;
static struct device *dev;
static int size = 4096;

static void mydev_release(struct device *dev)
{
	pr_info("[UTILS] releasing device\n");
}

static int __init utils_init(void)
{
	int ret;

	/* Device allocation with error checking */
	dev = kzalloc(sizeof(struct device), GFP_KERNEL);
	if (!dev) {
		pr_err("[UTILS] device allocation failed\n");
		return -ENOMEM;
	}

	dev_set_name(dev, "utils");
	dev->release = mydev_release;

	ret = device_register(dev);
	if (ret) {
		pr_err("[UTILS] device registration failed: %d\n", ret);
		kfree(dev);
		return ret;
	}

	/* DMA mask management with fallback */
	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(64));
	if (ret) {
		pr_info("[UTILS] 64-bit DMA not supported, trying 32-bit\n");
		ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(32));
		if (ret) {
			pr_err("[UTILS] 32-bit DMA not supported: %d\n", ret);
			goto err_unreg;
		}
		pr_info("[UTILS] 32-bit DMA mask set successfully\n");
	} else {
		pr_info("[UTILS] 64-bit DMA mask set successfully\n");
	}

	/* Buffer allocation with error checking */
	kbuf = kmalloc(size, GFP_KERNEL);
	if (!kbuf) {
		pr_err("[UTILS] buffer allocation failed\n");
		ret = -ENOMEM;
		goto err_unreg;
	}

	/* DMA mapping with error checking */
	handle = dma_map_single(dev, kbuf, size, DMA_TO_DEVICE);
	if (dma_mapping_error(dev, handle)) {
		pr_err("[UTILS] DMA mapping failed\n");
		ret = -ENOMEM;
		goto err_free_buf;
	}

	pr_info("[UTILS] DMA utilities (mask management, error handling)\n");
	pr_info("[UTILS] DMA utilities test successful\n");
	return 0;

 err_free_buf:
	kfree(kbuf);
 err_unreg:
	device_unregister(dev);
	return ret;
}

static void __exit utils_exit(void)
{
	dma_unmap_single(dev, handle, size, DMA_TO_DEVICE);
	kfree(kbuf);
	device_unregister(dev);
}

module_init(utils_init);
module_exit(utils_exit);

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("DMA utilities (masks, error handling)");
MODULE_LICENSE("Dual MIT/GPL");
