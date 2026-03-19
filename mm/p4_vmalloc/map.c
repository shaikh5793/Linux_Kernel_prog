/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>

static struct page **pages;
static void *vmap_ptr, *vm_map_ptr;
static int num_pages = 4;

static int map_init(void)
{
	int i;

	pages = kmalloc_array(num_pages, sizeof(struct page *), GFP_KERNEL);
	if (!pages) {
		pr_err("pages array allocation failed\n");
		return -ENOMEM;
	}

	for (i = 0; i < num_pages; i++) {
		pages[i] = alloc_page(GFP_KERNEL);
		if (!pages[i]) {
			pr_err("page allocation failed\n");
			goto err_free_pages;
		}
	}

	vmap_ptr = vmap(pages, num_pages, VM_MAP, PAGE_KERNEL);
	if (!vmap_ptr) {
		pr_err("vmap failed\n");
		goto err_free_pages;
	}

	vm_map_ptr = vm_map_ram(pages, num_pages, -1);
	if (!vm_map_ptr) {
		pr_err("vm_map_ram failed\n");
		vunmap(vmap_ptr);
		goto err_free_pages;
	}

	pr_info("vmalloc mapping successful\n");
	return 0;

 err_free_pages:
	for (i = 0; i < num_pages; i++) {
		if (pages[i])
			__free_page(pages[i]);
	}
	kfree(pages);
	return -ENOMEM;
}

static void map_exit(void)
{
	int i;

	if (vm_map_ptr)
		vm_unmap_ram(vm_map_ptr, num_pages);
	if (vmap_ptr)
		vunmap(vmap_ptr);

	for (i = 0; i < num_pages; i++)
		__free_page(pages[i]);
	kfree(pages);
}

module_init(map_init);
module_exit(map_exit);

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("vmalloc mapping test");
MODULE_LICENSE("Dual MIT/GPL");
