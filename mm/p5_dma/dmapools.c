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
#include <linux/dmapool.h>
#include <linux/dma-mapping.h>
#include <linux/device.h>

static void *kbuf;
static dma_addr_t handle;
static struct device *dev;
static struct dma_pool *pool;

static void mydev_release(struct device *dev)
{
	pr_info("[POOLS] Releasing device\n");
}

static int __init dmapools_init(void)
{
	static const u64 dmamask = DMA_BIT_MASK(32);
	int ret;

	dev = kzalloc(sizeof(struct device), GFP_KERNEL);
	dev_set_name(dev, "dmapools");
	dev->dma_mask = (u64 *) & dmamask;
	dev->coherent_dma_mask = DMA_BIT_MASK(32);
	dev->release = mydev_release;
	ret = device_register(dev);
	if (ret) {
		pr_err("[POOLS] device registration failed: %d\n", ret);
		kfree(dev);
		return ret;
	}

	pool = dma_pool_create("test_pool", dev, 512, 64, 0);
	if (!pool) {
		pr_err("[POOLS] pool creation failed\n");
		return -ENOMEM;
	}

	kbuf = dma_pool_alloc(pool, GFP_KERNEL, &handle);
	if (!kbuf) {
		pr_err("[POOLS] pool allocation failed\n");
		return -ENOMEM;
	}

	pr_info("[POOLS] pool allocation (efficient small buffers)\n");
	pr_info("[POOLS] DMA pool allocation successful\n");
	return 0;
}

static void __exit dmapools_exit(void)
{
	dma_pool_free(pool, kbuf, handle);
	dma_pool_destroy(pool);
	device_unregister(dev);
}

module_init(dmapools_init);
module_exit(dmapools_exit);

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("DMA pools test");
MODULE_LICENSE("Dual MIT/GPL");
