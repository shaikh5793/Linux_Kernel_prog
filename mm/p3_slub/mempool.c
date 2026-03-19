/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 *
 * SLUB + Mempool: Reliable Memory Allocation
 * 
 * This demonstrates memory pools built on top of SLUB allocator
 * to guarantee allocation success in critical paths (I/O, networking).
 * 
 * Shows:
 * 1. Creating kmem_cache for objects
 * 2. Creating mempool backed by the cache
 * 3. Guaranteed allocation even under memory pressure
 * 4. Proper cleanup and error handling
 * 
 * Use cases: Block I/O, network packet processing, interrupt contexts
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mempool.h>
#include <linux/string.h>

#define OBJECT_SIZE 256
#define POOL_MIN_ELEMENTS 8

struct my_object {
	char data[OBJECT_SIZE];
	int id;
	struct list_head list;
};

static struct kmem_cache *object_cache = NULL;
static mempool_t *object_pool = NULL;
static struct my_object *obj1, *obj2, *obj3;

static int __init mempool_example_init(void)
{
	/* 1. Create custom kmem_cache for our objects */
	object_cache = kmem_cache_create("my_object_cache",
					 sizeof(struct my_object),
					 0,
					 SLAB_HWCACHE_ALIGN,
					 NULL);
	if (!object_cache) {
		pr_err("[MEMPOOL] failed to create kmem_cache\n");
		return -ENOMEM;
	}
	pr_info("[MEMPOOL] created kmem_cache: %zu bytes per object\n", 
		sizeof(struct my_object));

	/* 2. Create mempool backed by the cache */
	object_pool = mempool_create(POOL_MIN_ELEMENTS,
				     mempool_alloc_slab,
				     mempool_free_slab,
				     object_cache);
	if (!object_pool) {
		pr_err("[MEMPOOL] failed to create mempool\n");
		kmem_cache_destroy(object_cache);
		return -ENOMEM;
	}
	pr_info("[MEMPOOL] created mempool with %d reserved elements\n", 
		POOL_MIN_ELEMENTS);

	/* 3. Test allocations - these are guaranteed to succeed */
	obj1 = mempool_alloc(object_pool, GFP_KERNEL);
	if (obj1) {
		obj1->id = 1;
		snprintf(obj1->data, OBJECT_SIZE, "Object 1 from mempool");
		pr_info("[MEMPOOL] allocated obj1: %s\n", obj1->data);
	}

	obj2 = mempool_alloc(object_pool, GFP_ATOMIC);
	if (obj2) {
		obj2->id = 2;
		snprintf(obj2->data, OBJECT_SIZE, "Object 2 from mempool (atomic)");
		pr_info("[MEMPOOL] allocated obj2: %s\n", obj2->data);
	}

	/* 4. Even under memory pressure, this should work */
	obj3 = mempool_alloc(object_pool, GFP_NOWAIT);
	if (obj3) {
		obj3->id = 3;
		snprintf(obj3->data, OBJECT_SIZE, "Object 3 from mempool (nowait)");
		pr_info("[MEMPOOL] allocated obj3: %s\n", obj3->data);
	}

	pr_info("[MEMPOOL] mempool guarantees allocation success in critical paths\n");
	pr_info("[MEMPOOL] used by: block I/O, networking, interrupt handlers\n");
	pr_info("[MEMPOOL] mempool demonstration successful\n");
	return 0;
}

static void __exit mempool_example_exit(void)
{
	/* Free all allocated objects back to mempool */
	if (obj1) {
		mempool_free(obj1, object_pool);
		pr_info("[MEMPOOL] freed obj1\n");
	}
	if (obj2) {
		mempool_free(obj2, object_pool);
		pr_info("[MEMPOOL] freed obj2\n");
	}
	if (obj3) {
		mempool_free(obj3, object_pool);
		pr_info("[MEMPOOL] freed obj3\n");
	}

	/* Destroy mempool and cache */
	if (object_pool) {
		mempool_destroy(object_pool);
		pr_info("[MEMPOOL] destroyed mempool\n");
	}
	if (object_cache) {
		kmem_cache_destroy(object_cache);
		pr_info("[MEMPOOL] destroyed kmem_cache\n");
	}
}

module_init(mempool_example_init);
module_exit(mempool_example_exit);

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("SLUB + mempool reliable allocation");
MODULE_LICENSE("Dual MIT/GPL");
