/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 * 
 * Huge Page Allocation 
 * Simple demonstration of 2MB and 1GB huge page allocation
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/gfp.h>

#define TEST_STRING "hugepage"

static void *hugepage_2mb = NULL;
static void *hugepage_1gb = NULL;

static int __init hugepage_init(void)
{
    pr_info("=== Huge Page Allocation Demo ===\n");
    
    // Allocate 2MB huge page
    pr_info("\nAllocating 2MB huge page...\n");
    hugepage_2mb = alloc_pages_exact(PMD_SIZE, GFP_KERNEL | __GFP_COMP);
    if (hugepage_2mb) {
        pr_info("✓ 2MB page allocated at: 0x%px\n", hugepage_2mb);
        pr_info("  Size: %lu bytes (%lu KB)\n", PMD_SIZE, PMD_SIZE / 1024);
        
        // Test writing to the page
        strcpy((char *)hugepage_2mb, TEST_STRING);
        pr_info("  Written test string: '%s'\n", (char *)hugepage_2mb);
    } else {
        pr_err("✗ Failed to allocate 2MB huge page\n");
    }
    
    // Allocate 1GB huge page
    pr_info("\nAllocating 1GB huge page...\n");
    hugepage_1gb = alloc_pages_exact(PUD_SIZE, GFP_KERNEL | __GFP_COMP | __GFP_NOWARN);
    if (hugepage_1gb) {
        pr_info("✓ 1GB page allocated at: 0x%px\n", hugepage_1gb);
        pr_info("  Size: %lu bytes (%lu MB)\n", PUD_SIZE, PUD_SIZE / (1024 * 1024));
        
        // Test writing to the page
        strcpy((char *)hugepage_1gb, TEST_STRING);
        pr_info("  Written test string: '%s'\n", (char *)hugepage_1gb);
    } else {
        pr_info("✗ 1GB page allocation failed (expected on most systems)\n");
        pr_info("  Requires: sufficient contiguous memory + kernel config\n");
    }
    
    pr_info("\nPage size comparison:\n");
    pr_info("  Regular: %lu bytes (4KB)\n", PAGE_SIZE);
    pr_info("  2MB:     %lu bytes (%lux larger)\n", PMD_SIZE, PMD_SIZE / PAGE_SIZE);
    pr_info("  1GB:     %lu bytes (%lux larger)\n", PUD_SIZE, PUD_SIZE / PAGE_SIZE);
    
    return 0;
}

static void __exit hugepage_exit(void)
{
    if (hugepage_2mb) {
        pr_info("Freeing 2MB huge page\n");
        free_pages_exact(hugepage_2mb, PMD_SIZE);
    }
    
    if (hugepage_1gb) {
        pr_info("Freeing 1GB huge page\n");
        free_pages_exact(hugepage_1gb, PUD_SIZE);
    }
    
    pr_info("Huge Page Allocation Demo unloaded\n");
}

module_init(hugepage_init);
module_exit(hugepage_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("Simple huge page allocation demo for 2MB and 1GB pages");
