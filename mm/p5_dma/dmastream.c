/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>

int direction = DMA_TO_DEVICE;

static void *kbuf;
static dma_addr_t handle;
static struct device *dev;
static int size = 512;

static void mydev_release(struct device *dev)
{
	pr_info("[STREAM] Releasing device\n");
}

static int __init dma_init(void)
{
	static const u64 dmamask = DMA_BIT_MASK(32);
	void *pa;
	int ret;

	dev = kzalloc(sizeof(struct device), GFP_KERNEL);
	dev_set_name(dev, "dmastream");
	dev->dma_mask = (u64 *) & dmamask;
	dev->coherent_dma_mask = DMA_BIT_MASK(32);
	dev->release = mydev_release;
	ret = device_register(dev);
	if (ret) {
		pr_err("[STREAM] device registration failed: %d\n", ret);
		kfree(dev);
		return ret;
	}

	kbuf = kmalloc(size, GFP_KERNEL);
	if (kbuf == NULL) {
		pr_err("[STREAM] allocation failed\n");
		return -ENOMEM;
	}

	handle = dma_map_single(dev, kbuf, size, direction);
	if (dma_mapping_error(dev, handle)) {
		pr_err("[STREAM] dma mapping failed\n");
		return -EINVAL;
	}

	pr_info("[STREAM] streaming mapping (non-coherent)\n");

	pr_info("[STREAM] Kernel mapped @ %12p, DMA mapped @ %12p, size = %d\n", kbuf,
		(unsigned long *)handle, (int)size);

	pa = (void *)__pa(kbuf);
	pr_info("[STREAM] physical address @ %12p\n", pa);

	strcpy((char *)kbuf, "mapped buffer");
	pr_info("[STREAM] DATA: %s\n", (char *)kbuf);

	return 0;
}

static void __exit dma_exit(void)
{
	dma_unmap_single(dev, handle, size, direction);
	kfree(kbuf);
	device_unregister(dev);
}

module_init(dma_init);
module_exit(dma_exit);

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("DMA interface test");
MODULE_LICENSE("Dual MIT/GPL");

/* Rx
 * step 1: allocate buffer
 * step 2: dma_map_single(DMA_FROM_DEVICE) // mapping
 * step 3: Program DMA transfer
 * step 4: wait for DMA Transfer to complete
 * step 5: dma_unmap_single(DMA_FROM_DEVICE);//sync point (invalidation mem->cache)
 * step 6: Read data arrived into dma buffer.
 
 * Tx
 * step 1: allocate buffer
 * step 2: populate data into buffer
 * step 3: dma_map_single(DMA_TO_DEVICE) //flush from cache to memory
 * step 4: program DMA transfer
 * step 5: wait for transfer completion
 * step 6: dma_unmap_single(DMA_TO_DEVICE) //unmap dma_addr_t
 */
