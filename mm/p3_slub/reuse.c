/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>

#define RAW_BLOCK_SIZE 128

/* Simple network device structure */
struct net_device {
	u32 magic;
	u32 device_id;
	char name[12];
	u32 ip_addr;
	u32 flags;
};

#define NET_MAGIC 0xDECE1234

static struct kmem_cache *block_cache = NULL;
static struct kmem_cache *object_cache = NULL;

/* Constructor for object cache */
static void net_constructor(void *obj)
{
	struct net_device *dev = (struct net_device *)obj;

	dev->magic = NET_MAGIC;
	dev->device_id = 0;
	memset(dev->name, 0, sizeof(dev->name));
	dev->ip_addr = 0;
	dev->flags = 0;
}

static void fill_device_data(struct net_device *dev, u32 id, const char *name,
			     u32 ip)
{
	dev->magic = NET_MAGIC;
	dev->device_id = id;
	strncpy(dev->name, name, sizeof(dev->name) - 1);
	dev->name[sizeof(dev->name) - 1] = '\0';	/* Ensure null termination */
	dev->ip_addr = ip;
	dev->flags = 0xACE50000 + id;
}

static void print_device_data(struct net_device *dev, const char *label)
{
	pr_info("[REUSE] %s: ID=%u, IP=%u.%u.%u.%u, Name='%s'\n", label,
		dev->device_id, (dev->ip_addr >> 24) & 0xFF,
		(dev->ip_addr >> 16) & 0xFF, (dev->ip_addr >> 8) & 0xFF,
		dev->ip_addr & 0xFF, dev->name);
}

static int __init reuse_init(void)
{
	void *raw_block1, *raw_block2;
	struct net_device *obj1, *obj2;

	pr_info("[REUSE] Cache reuse test\n");

	/* Compare raw cache (no constructor) vs object cache (with constructor) */
	block_cache =
	    kmem_cache_create("raw_block_cache", RAW_BLOCK_SIZE, 0, 0, NULL);
	object_cache =
	    kmem_cache_create("net_device_cache", sizeof(struct net_device), 0,
			      SLAB_POISON, net_constructor);

	if (!block_cache || !object_cache) {
		pr_err("[REUSE] Failed to create caches\n");
		return -ENOMEM;
	}

	pr_info("[REUSE] Raw: %d bytes, Object: %zu bytes\n",
		RAW_BLOCK_SIZE, sizeof(struct net_device));

	raw_block1 = kmem_cache_alloc(block_cache, GFP_KERNEL);
	if (!raw_block1) {
		pr_err("[REUSE] Raw allocation failed\n");
		goto cleanup;
	}

	pr_info("[REUSE] Raw block: %p\n", raw_block1);

	/* Fill raw block with test data pattern */
	memset(raw_block1, 0xAB, RAW_BLOCK_SIZE);
	*(u32 *) raw_block1 = 0x12345678;	/* Magic marker */
	*((u32 *) raw_block1 + 1) = 201;	/* Test ID */
	pr_info("[REUSE] Raw filled: magic=0x%08x, id=%u\n",
		*(u32 *) raw_block1, *((u32 *) raw_block1 + 1));

	/* Free and reallocate */
	kmem_cache_free(block_cache, raw_block1);
	pr_info("[REUSE] Raw freed\n");

	raw_block2 = kmem_cache_alloc(block_cache, GFP_KERNEL);
	if (raw_block2) {
		pr_info("[REUSE] Raw realloc: %p\n", raw_block2);
		if (raw_block1 == raw_block2) {
			u32 magic = *(u32 *) raw_block2;
			u32 test_id = *((u32 *) raw_block2 + 1);
			if (magic == 0x12345678 && test_id == 201) {
				pr_warn
				    ("[REUSE] *** RAW LEAKED! *** magic=0x%08x, id=%u\n",
				     magic, test_id);
			} else {
				pr_info
				    ("[REUSE] Raw data cleared: magic=0x%08x, id=%u\n",
				     magic, test_id);
			}
		}

		kmem_cache_free(block_cache, raw_block2);
	}

	/* Object cache has constructor - but only runs on initial allocation, not reuse */
	obj1 = kmem_cache_alloc(object_cache, GFP_KERNEL);
	if (!obj1) {
		pr_err("[REUSE] Obj allocation failed\n");
		goto cleanup;
	}

	pr_info("[REUSE] Object 1: %p\n", obj1);
	print_device_data(obj1, "Obj fresh");

	/* Fill with device data */
	fill_device_data(obj1, 301, "wlan0", 0xC0A80165);	/* 192.168.1.101 */
	print_device_data(obj1, "Obj filled");

	kmem_cache_free(object_cache, obj1);
	pr_info("[REUSE] Object freed\n");

	obj2 = kmem_cache_alloc(object_cache, GFP_KERNEL);
	if (obj2) {
		pr_info("[REUSE] Object 2: %p\n", obj2);
		if (obj1 == obj2) {
			if (obj2->magic == NET_MAGIC && obj2->device_id == 301
			    && strncmp(obj2->name, "wlan0", 5) == 0) {
				pr_warn("[REUSE] *** DATA LEAKED! ***\n");
				print_device_data(obj2, "Obj LEAKED");
			} else if (obj2->magic == NET_MAGIC
				   && obj2->device_id == 0) {
				print_device_data(obj2, "Obj cleared");
			} else {
				print_device_data(obj2, "Obj unexpected");
			}
		}

		kmem_cache_free(object_cache, obj2);
	}

 cleanup:
	if (block_cache) {
		kmem_cache_destroy(block_cache);
		pr_info("[REUSE] Raw cache destroyed\n");
	}

	if (object_cache) {
		kmem_cache_destroy(object_cache);
		pr_info("[REUSE] Obj cache destroyed\n");
	}

	return 0;
}

static void __exit reuse_exit(void)
{
	pr_info("[REUSE] Module unloaded\n");
}

module_init(reuse_init);
module_exit(reuse_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("Cache reuse security leak analysis");
