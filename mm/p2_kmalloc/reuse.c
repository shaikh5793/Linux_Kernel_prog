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

static int __init reuse_init(void)
{
	void *ptr1, *ptr2;
	u32 *data1, *data2;

	pr_info("[REUSE] memory reuse patterns\n");

	/* Test memory reuse: freed objects may be reused for same-size allocations */
	ptr1 = kmalloc(64, GFP_KERNEL);
	if (!ptr1) {
		pr_err("[REUSE] First allocation failed\n");
		return -ENOMEM;
	}

	data1 = (u32 *) ptr1;
	*data1 = 0xDECE5EDA;	/* TECH VEDA(www.techveda.org) equivalent */
	pr_info("[REUSE] First: %p, data: 0x%08x\n", ptr1, *data1);
	kfree(ptr1);

	ptr2 = kmalloc(64, GFP_KERNEL);
	if (!ptr2) {
		pr_err("[REUSE] Second allocation failed\n");
		return -ENOMEM;
	}

	data2 = (u32 *) ptr2;
	pr_info("[REUSE] Second: %p, found: 0x%08x\n", ptr2, *data2);

	if (ptr1 == ptr2) {
		pr_info("[REUSE] [REUSE] Same address\n");
	} else {
		pr_info("[REUSE] [NEW] Different address\n");
	}

	if (*data2 == 0xDECE5EDA) {
		pr_warn("[REUSE] [LEAK] Previous data found!\n");
	} else {
		pr_info("[REUSE] [CLEAN] Data cleared\n");
	}

	/* Different sizes may have different reuse patterns based on slab caches */
	size_t sizes[] = { 32, 64, 128, 256 };
	for (int i = 0; i < 4; i++) {
		void *test_ptr1 = kmalloc(sizes[i], GFP_KERNEL);
		if (test_ptr1) {
			*(u32 *) test_ptr1 = 0xCAFE0000 + i;
			kfree(test_ptr1);

			void *test_ptr2 = kmalloc(sizes[i], GFP_KERNEL);
			if (test_ptr2) {
				bool same_addr = (test_ptr1 == test_ptr2);
				bool data_found =
				    (*(u32 *) test_ptr2 == (0xCAFE0000 + i));

				pr_info("[REUSE] Size %zu: %s addr, %s data\n",
					sizes[i], same_addr ? "REUSED" : "NEW",
					data_found ? "LEAKED" : "CLEAN");

				kfree(test_ptr2);
			}
		}
	}

	kfree(ptr2);
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
MODULE_DESCRIPTION("Memory reuse patterns demonstration");
