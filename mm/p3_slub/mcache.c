/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 *
 * SLUB Cache: Memory Cache Operations
 * 
 * Creating and destroying memory caches
 * Memory allocation and deallocation patterns
 * Fixed-size memory block handling
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>

#define CACHE_BLOCK_SIZE 128

static struct kmem_cache *raw_cache = NULL;
void *block1, *block2, *block3;

static int __init mcache_init(void)
{

	pr_info("[MCACHE] custom slab cache creation\n");

	/* kmem_cache_create() creates dedicated slab cache for fixed-size objects */
	raw_cache = kmem_cache_create("block_cache", CACHE_BLOCK_SIZE,	/* object size */
				      0,	/* alignment (0 = default) */
				      SLAB_POISON,	/* flags */
				      NULL);	/* constructor */

	if (!raw_cache) {
		pr_err("[MCACHE] Failed to create cache\n");
		return -ENOMEM;
	}

	/* Allocate first block */
	block1 = kmem_cache_alloc(raw_cache, GFP_KERNEL);
	if (!block1) {
		pr_err("[MCACHE] First allocation failed\n");
		goto cleanup;
	}
	pr_info("[MCACHE] Block 1: %p\n", block1);
	memset(block1, 0xAA, CACHE_BLOCK_SIZE);

	/* Allocate second block */
	block2 = kmem_cache_alloc(raw_cache, GFP_KERNEL);
	if (!block2) {
		pr_err("[MCACHE] Second allocation failed\n");
		kmem_cache_free(raw_cache, block1);
		goto cleanup;
	}

	pr_info("[MCACHE] Block 2: %p\n", block2);
	memset(block2, 0xBB, CACHE_BLOCK_SIZE);

	/* Allocate third block */
	block3 = kmem_cache_alloc(raw_cache, GFP_KERNEL);
	if (!block3) {
		pr_err("[MCACHE] Third allocation failed\n");
		kmem_cache_free(raw_cache, block1);
		kmem_cache_free(raw_cache, block2);
		goto cleanup;
	}

	pr_info("[MCACHE] Block 3: %p\n", block3);
	memset(block3, 0xCC, CACHE_BLOCK_SIZE);

	/* Analyze address spacing to understand cache layout */
	long diff12 = (char *)block2 - (char *)block1;
	pr_info("[MCACHE] Spacing: %ld bytes\n", diff12);

 cleanup:
	return 0;
}

static void __exit mcache_exit(void)
{
	/* Free blocks */
	kmem_cache_free(raw_cache, block2);
	kmem_cache_free(raw_cache, block1);
	kmem_cache_free(raw_cache, block3);

	kmem_cache_destroy(raw_cache);
	pr_info("[MCACHE] Cache destroyed\n");
}

module_init(mcache_init);
module_exit(mcache_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("Memory cache operations example");
