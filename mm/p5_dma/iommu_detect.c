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
#include <linux/iommu.h>

static void *bounce_buf, *iommu_buf;
static dma_addr_t bounce_handle, iommu_handle;
static struct device *dev;
static int size = 4096;

static void mydev_release(struct device *dev)
{
	pr_info("[IOMMU_DETECT] releasing device\n");
}

static int __init iommu_detect_init(void)
{
	static const u64 bounce_mask = DMA_BIT_MASK(24);
	static const u64 iommu_mask = DMA_BIT_MASK(32);
	phys_addr_t bounce_phys, iommu_phys;
	struct iommu_domain *domain;
	int ret;

	dev = kzalloc(sizeof(struct device), GFP_KERNEL);
	dev_set_name(dev, "iommu_detect");
	dev->release = mydev_release;
	ret = device_register(dev);
	if (ret) {
		pr_err("[IOMMU_DETECT] device registration failed: %d\n", ret);
		kfree(dev);
		return ret;
	}

	/* 1. Bounce buffer test with 24-bit mask */
	dev->dma_mask = (u64 *) & bounce_mask;
	dev->coherent_dma_mask = DMA_BIT_MASK(24);

	bounce_buf = kmalloc(size, GFP_KERNEL);
	if (bounce_buf) {
		bounce_handle =
		    dma_map_single(dev, bounce_buf, size, DMA_TO_DEVICE);
		if (!dma_mapping_error(dev, bounce_handle)) {
			bounce_phys = virt_to_phys(bounce_buf);
			pr_info("[IOMMU_DETECT] bounce test: DMA=%pad Phys=%pa %s\n",
				&bounce_handle, &bounce_phys,
				(bounce_handle !=
				 bounce_phys) ? "BOUNCE" : "DIRECT");
		}
	}

	/* 2. IOMMU detection test with 32-bit mask */
	dev->dma_mask = (u64 *) & iommu_mask;
	dev->coherent_dma_mask = DMA_BIT_MASK(32);

	domain = iommu_get_domain_for_dev(dev);
	if (domain)
		pr_info("[IOMMU_DETECT] IOMMU domain found\n");
	else
		pr_info("[IOMMU_DETECT] no IOMMU domain\n");

	iommu_buf = dma_alloc_coherent(dev, size, &iommu_handle, GFP_KERNEL);
	if (iommu_buf) {
		iommu_phys = virt_to_phys(iommu_buf);
		pr_info("[IOMMU_DETECT] IOMMU test: DMA=%pad Phys=%pa %s\n",
			&iommu_handle, &iommu_phys,
			(iommu_handle != iommu_phys) ? "IOMMU" : "DIRECT");
	}

	if (!bounce_buf && !iommu_buf) {
		pr_err("[IOMMU_DETECT] hardware detection tests failed\n");
		return -ENOMEM;
	}

	pr_info("[IOMMU_DETECT] hardware-level DMA detection (bounce buffers, IOMMU)\n");
	pr_info("[IOMMU_DETECT] hardware detection tests successful\n");
	return 0;
}

static void __exit iommu_detect_exit(void)
{
	if (bounce_buf) {
		dma_unmap_single(dev, bounce_handle, size, DMA_TO_DEVICE);
		kfree(bounce_buf);
	}
	if (iommu_buf)
		dma_free_coherent(dev, size, iommu_buf, iommu_handle);
	device_unregister(dev);
}

module_init(iommu_detect_init);
module_exit(iommu_detect_exit);

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("Hardware DMA detection (bounce buffers, IOMMU)");
MODULE_LICENSE("Dual MIT/GPL");
