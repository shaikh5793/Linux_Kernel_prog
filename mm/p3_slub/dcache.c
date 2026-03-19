/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 * 
 * Structured memory allocation patterns
 * Initialization function usage
 * Memory setup and management
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>

/* Simple device object structure */
struct device_info {
	u32 magic;		/* Magic number for validation */
	u32 device_id;		/* Device identifier */
	u32 status;		/* Device status flags */
	char name[16];		/* Device name */
};

#define DEVICE_MAGIC 0xDECE1234

static struct kmem_cache *device_cache = NULL;

/* Constructor function - initializes all objects when allocated */
static void device_constructor(void *obj)
{
	struct device_info *device = (struct device_info *)obj;

	device->magic = DEVICE_MAGIC;
	device->device_id = 0;
	device->status = 0;
	memset(device->name, 0, sizeof(device->name));
}

static void print_device_info(struct device_info *device, const char *label)
{
	pr_info("[DCACHE] %s: ID=%u, Status=0x%x, Name='%s'\n",
		label, device->device_id, device->status, device->name);
}

static int __init dcache_init(void)
{
	struct device_info *device1, *device2, *device3;

	pr_info("[DCACHE] Object cache test\n");

	/* Create cache with constructor to initialize objects automatically */
	device_cache = kmem_cache_create("device_info_cache", sizeof(struct device_info), 0,	/* alignment */
					 SLAB_POISON,	/* flags */
					 device_constructor);	/* constructor */

	if (!device_cache) {
		pr_err("[DCACHE] Failed to create device cache\n");
		return -ENOMEM;
	}

	pr_info("[DCACHE] Cache size: %zu bytes\n", sizeof(struct device_info));

	/* Allocate first device object */
	device1 = kmem_cache_alloc(device_cache, GFP_KERNEL);
	if (!device1) {
		pr_err("[DCACHE] First allocation failed\n");
		goto cleanup;
	}

	pr_info("[DCACHE] Device 1: %p\n", device1);
	if (device1->magic == DEVICE_MAGIC) {
		pr_info("[DCACHE] Init OK\n");
	}

	/* Simulate using the device object */
	device1->device_id = 101;
	device1->status = 0xACE5;
	strcpy(device1->name, "eth0");

	print_device_info(device1, "Device 1");

	/* Allocate second device */
	device2 = kmem_cache_alloc(device_cache, GFP_KERNEL);
	if (!device2) {
		pr_err("[DCACHE] Second allocation failed\n");
		kmem_cache_free(device_cache, device1);
		goto cleanup;
	}

	pr_info("[DCACHE] Device 2: %p\n", device2);

	/* Use second device differently */
	device2->device_id = 102;
	device2->status = 0xBADC;
	strcpy(device2->name, "wlan0");

	print_device_info(device2, "Device 2");

	long diff = (char *)device2 - (char *)device1;
	pr_info("[DCACHE] Spacing: %ld bytes\n", diff);

	/* Test third allocation */
	device3 = kmem_cache_alloc(device_cache, GFP_KERNEL);
	if (device3) {
		pr_info("[DCACHE] Device 3: %p\n", device3);
		print_device_info(device3, "Device 3");
		kmem_cache_free(device_cache, device3);
	}

	/* Free devices */
	kmem_cache_free(device_cache, device1);
	kmem_cache_free(device_cache, device2);
	pr_info("[DCACHE] Devices freed\n");

 cleanup:
	return 0;
}

static void __exit dcache_exit(void)
{

	kmem_cache_destroy(device_cache);
	pr_info("[DCACHE] Module unloaded\n");
}

module_init(dcache_init);
module_exit(dcache_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("Object caching with constructor");
