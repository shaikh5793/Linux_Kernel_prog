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
static int size = 1024;

static void mydev_release(struct device *dev)
{
	pr_info("[SYNC] releasing device\n");
}

static int __init sync_init(void)
{
	static const u64 dmamask = DMA_BIT_MASK(32);
	int ret;

	dev = kzalloc(sizeof(struct device), GFP_KERNEL);
	dev_set_name(dev, "sync");
	dev->dma_mask = (u64 *) & dmamask;
	dev->coherent_dma_mask = DMA_BIT_MASK(32);
	dev->release = mydev_release;
	ret = device_register(dev);
	if (ret) {
		pr_err("[SYNC] device registration failed: %d\n", ret);
		kfree(dev);
		return ret;
	}

	kbuf = kmalloc(size, GFP_KERNEL);
	if (!kbuf) {
		pr_err("[SYNC] allocation failed\n");
		return -ENOMEM;
	}

	handle = dma_map_single(dev, kbuf, size, DMA_BIDIRECTIONAL);
	if (dma_mapping_error(dev, handle)) {
		pr_err("[SYNC] dma mapping failed\n");
		return -EINVAL;
	}

	dma_sync_single_for_cpu(dev, handle, size, DMA_BIDIRECTIONAL);
	dma_sync_single_for_device(dev, handle, size, DMA_BIDIRECTIONAL);

	pr_info("[SYNC] explicit cache synchronization\n");
	pr_info("[SYNC] DMA sync operations successful\n");
	return 0;
}

static void __exit sync_exit(void)
{
	dma_unmap_single(dev, handle, size, DMA_BIDIRECTIONAL);
	kfree(kbuf);
	device_unregister(dev);
}

module_init(sync_init);
module_exit(sync_exit);

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("DMA sync test");
MODULE_LICENSE("Dual MIT/GPL");
