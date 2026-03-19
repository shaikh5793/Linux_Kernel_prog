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
#include <linux/scatterlist.h>

static struct device *dev;
static struct scatterlist sg[2];
static void *buf1, *buf2;

static void mydev_release(struct device *dev)
{
	pr_info("[SCATTER] releasing device\n");
}

static int __init scattergather_init(void)
{
	static const u64 dmamask = DMA_BIT_MASK(32);
	int ret;

	dev = kzalloc(sizeof(struct device), GFP_KERNEL);
	dev_set_name(dev, "scattergather");
	dev->dma_mask = (u64 *) & dmamask;
	dev->coherent_dma_mask = DMA_BIT_MASK(32);
	dev->release = mydev_release;
	ret = device_register(dev);
	if (ret) {
		pr_err("[SCATTER] device registration failed: %d\n", ret);
		kfree(dev);
		return ret;
	}

	buf1 = kzalloc(PAGE_SIZE, GFP_KERNEL);
	buf2 = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (!buf1 || !buf2) {
		pr_err("[SCATTER] buffer allocation failed\n");
		return -ENOMEM;
	}

	sg_init_table(sg, 2);
	sg_set_buf(&sg[0], buf1, PAGE_SIZE);
	sg_set_buf(&sg[1], buf2, PAGE_SIZE);

	ret = dma_map_sg(dev, sg, 2, DMA_TO_DEVICE);
	if (ret == 0) {
		pr_err("[SCATTER] scatter-gather mapping failed\n");
		return -EINVAL;
	}

	pr_info("[SCATTER] scatter-gather mapping (multiple buffers)\n");
	pr_info("[SCATTER] scatter-gather DMA mapping successful\n");
	return 0;
}

static void __exit scattergather_exit(void)
{
	dma_unmap_sg(dev, sg, 2, DMA_TO_DEVICE);
	kfree(buf1);
	kfree(buf2);
	device_unregister(dev);
}

module_init(scattergather_init);
module_exit(scattergather_exit);

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("DMA scatter-gather test");
MODULE_LICENSE("Dual MIT/GPL");
