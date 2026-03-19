/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>

static void *kbuf;
static dma_addr_t handle;
static size_t size = 4096;
static struct device *dev;

static void mydev_release(struct device *dev)
{
	pr_info("[COHERENT] Releasing device\n");
}

static int __init dma_init(void)
{
	static const u64 dmamask = DMA_BIT_MASK(32);
	void *pa;
	int ret;

	dev = kzalloc(sizeof(struct device), GFP_KERNEL);
	dev_set_name(dev, "dmatest");
	dev->dma_mask = (u64 *) & dmamask;
	dev->coherent_dma_mask = DMA_BIT_MASK(32);
	dev->release = mydev_release;
	ret = device_register(dev);
	if (ret) {
		pr_err("[COHERENT] device registration failed: %d\n", ret);
		kfree(dev);
		return ret;
	}

	/* dma_alloc_coherent method */
	kbuf = dma_alloc_coherent(dev, size, &handle, GFP_KERNEL | GFP_DMA32);
	if ((kbuf == NULL) || (handle == 0)) {
		pr_err("[COHERENT] coherent allocation failed\n");
		return -ENOMEM;
	}

	pr_info("[COHERENT] coherent allocation (cache-coherent)\n");

	pr_info("[COHERENT] kbuf=%12p, handle=%12p, size = %d\n", kbuf,
		(unsigned long *)handle, (int)size);

	pa = (void *)__pa(kbuf);
	pr_info("[COHERENT] physical address @ %12p\n", pa);

	strcpy((char *)kbuf, "DMA coherent mapped buffer");
	pr_info("[COHERENT] DATA: %s\n", (char *)kbuf);

	return 0;
}

static void __exit dma_exit(void)
{
	dma_free_coherent(dev, size, kbuf, handle);
	device_unregister(dev);
}

module_init(dma_init);
module_exit(dma_exit);

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("DMA coherent test");
MODULE_LICENSE("Dual MIT/GPL");
