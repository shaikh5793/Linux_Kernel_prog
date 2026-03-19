/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>

/* Same network device structure from Example 3 */
struct net_device {
	u32 magic;
	u32 device_id;
	char name[12];
	u32 ip_addr;
	u32 flags;
};

#define NET_MAGIC 0xDECE1234

static struct kmem_cache *net_device_cache = NULL;

/* Constructor for net_device cache - initializes objects to clean state */
static void net_device_constructor(void *obj)
{
	struct net_device *dev = (struct net_device *)obj;

	dev->magic = NET_MAGIC;
	dev->device_id = 0;
	memset(dev->name, 0, sizeof(dev->name));
	dev->ip_addr = 0;
	dev->flags = 0;
}

/* Safe allocation wrapper - always re-initializes objects */
static struct net_device *net_device_alloc(void)
{
	struct net_device *dev = kmem_cache_alloc(net_device_cache, GFP_KERNEL);

	if (dev) {
		pr_info("[SAFE_ALLOC] Allocated object at: %p\n", dev);
	}

	return dev;
}

/* Safe deallocation wrapper - clears driver data before freeing */
static void net_device_free(struct net_device *dev)
{
	if (!dev)
		return;

	/* Show what driver data we're clearing */
	pr_info("[SAFE_ALLOC] Freeing object: %p\n", dev);
	pr_info
	    ("[SAFE_ALLOC] State Before clearing: ID=%u Name='%s' IP=%u.%u.%u.%u Flags=0x%08x\n",
	     dev->device_id, dev->name, (dev->ip_addr >> 24) & 0xFF,
	     (dev->ip_addr >> 16) & 0xFF, (dev->ip_addr >> 8) & 0xFF,
	     dev->ip_addr & 0xFF, dev->flags);

	/* Clear driver data before returning to cache */
	dev->magic = 0x12345678;
	dev->device_id = 0;
	memset(dev->name, 0, sizeof(dev->name));
	dev->ip_addr = 0;
	dev->flags = 0;

	pr_info
	    ("[SAFE_ALLOC] After clear: ID=%u Name='%s' IP=0.0.0.0 Flags=0x00000000\n",
	     dev->device_id, dev->name);
	pr_info("[SAFE_ALLOC] Object %p returned to cache\n", dev);

	kmem_cache_free(net_device_cache, dev);
}

static void fill_device_data(struct net_device *dev, u32 id, const char *name,
			     u32 ip)
{
	dev->magic = NET_MAGIC;
	dev->device_id = id;
	strncpy(dev->name, name, sizeof(dev->name) - 1);
	dev->ip_addr = ip;
	dev->flags = 0xACE50000 + id;
}

static int __init safe_alloc_init(void)
{
	struct net_device *dev1, *dev2;

	pr_info("[SAFE_ALLOC] Testing wrapper functions\n");

	/* Create net_device cache with constructor */
	net_device_cache = kmem_cache_create("net_device_cache",
					     sizeof(struct net_device), 0, 0,
					     net_device_constructor);

	if (!net_device_cache) {
		pr_err("[SAFE_ALLOC] Failed to create cache\n");
		return -ENOMEM;
	}

	dev1 = net_device_alloc();
	if (!dev1)
		goto cleanup;

	fill_device_data(dev1, 501, "wlan1", 0xC0A80103);
	pr_info("[SAFE_ALLOC] === STEP 1: Fill object with driver data ===\n");
	pr_info
	    ("[SAFE_ALLOC] Object %p now contains: ID=%u Name='%s' IP=%u.%u.%u.%u\n",
	     dev1, dev1->device_id, dev1->name, (dev1->ip_addr >> 24) & 0xFF,
	     (dev1->ip_addr >> 16) & 0xFF, (dev1->ip_addr >> 8) & 0xFF,
	     dev1->ip_addr & 0xFF);

	pr_info
	    ("[SAFE_ALLOC] === STEP 2: Free object (wrapper clears data) ===\n");
	net_device_free(dev1);

	/* Check if we get same object back and if data was cleared */
	pr_info
	    ("[SAFE_ALLOC] === STEP 3: Reallocate to test object reuse ===\n");
	dev2 = net_device_alloc();
	if (dev2) {
		if (dev1 == dev2) {
			pr_info
			    ("[SAFE_ALLOC] *** OBJECT REUSED *** Same address: %p\n",
			     dev2);
			if (dev2->device_id == 501
			    && strncmp(dev2->name, "wlan1", 5) == 0) {
				pr_warn
				    ("[SAFE_ALLOC] *** DATA LEAK DETECTED! *** Object still contains: ID=%u Name='%s'\n",
				     dev2->device_id, dev2->name);
			} else {
				pr_info
				    ("[SAFE_ALLOC] *** SAFE! *** Data successfully cleared: ID=%u Name='%s'\n",
				     dev2->device_id, dev2->name);
				pr_info
				    ("[SAFE_ALLOC] *** LEAK PREVENTION WORKING ***\n");
			}
		} else {
			pr_info
			    ("[SAFE_ALLOC] Different object allocated: %p (previous was %p)\n",
			     dev2, dev1);
		}

		pr_info
		    ("[SAFE_ALLOC] === STEP 4: Clean up second object ===\n");
		net_device_free(dev2);
	}

 cleanup:
	if (net_device_cache) {
		kmem_cache_destroy(net_device_cache);
		pr_info("[SAFE_ALLOC] Cache destroyed\n");
	}

	return 0;
}

static void __exit safe_alloc_exit(void)
{
	pr_info("[SAFE_ALLOC] Module unloaded\n");
}

module_init(safe_alloc_init);
module_exit(safe_alloc_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("Safe allocation with custom wrapper functions");
