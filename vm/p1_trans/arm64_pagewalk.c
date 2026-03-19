/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 * 
 * ARM64 4-level page table walk
 * Demonstrates manual page table walk for different memory allocations
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/gfp.h>
#include <linux/pgtable.h>
#include <linux/highmem.h>

#define TEST_STRING "hello world from kernel memory"

static struct page *test_page = NULL;
static void *kmalloc_ptr = NULL;
static void *vmalloc_ptr = NULL;

static void arm64_walk_page_table(unsigned long vaddr, const char *mem_type)
{
    pgd_t *pgd;
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *pte;
    struct page *page;
    unsigned long paddr;
    char *mapped_addr;
    
    pr_info("\n=== ARM64 Page Table Walk for %s ===\n", mem_type);
    pr_info("Virtual Address: 0x%016lx\n", vaddr);
    
    struct mm_struct *mm = current->mm;
    if (!mm) {
        pr_err("No mm_struct available\n");
        return;
    }
    
    pgd = pgd_offset(mm, vaddr);
    pr_info("PGD: %px -> 0x%016lx (L0)\n", pgd, pgd_val(*pgd));
    if (pgd_none(*pgd) || pgd_bad(*pgd)) {
        pr_err("Invalid PGD entry\n");
        return;
    }
    
    p4d = p4d_offset(pgd, vaddr);
    pr_info("P4D: %px -> 0x%016lx (L1)\n", p4d, p4d_val(*p4d));
    if (p4d_none(*p4d) || p4d_bad(*p4d)) {
        pr_err("Invalid P4D entry\n");
        return;
    }
    
    pud = pud_offset(p4d, vaddr);
    pr_info("PUD: %px -> 0x%016lx (L2)\n", pud, pud_val(*pud));
    if (pud_none(*pud) || pud_bad(*pud)) {
        pr_err("Invalid PUD entry\n");
        return;
    }
    
    /*
     * Check for 1GB huge page/section support
     * ARM64 terminology: "sections" or "blocks" for large pages
     * 64K page configurations don't support 1GB sections
     */
    #ifdef CONFIG_ARM64_64K_PAGES
 - only 512MB at PUD level
    #else
    if (pud_sect(*pud)) {
        pr_info("1GB Section/Block detected\n");
        paddr = (pud_val(*pud) & PUD_MASK) | (vaddr & ~PUD_MASK);
        pr_info("Physical Address: 0x%016lx (via 1GB block)\n", paddr);
        return;
    }
    #endif
    
    pmd = pmd_offset(pud, vaddr);
    pr_info("PMD: %px -> 0x%016lx (L3)\n", pmd, pmd_val(*pmd));
    if (pmd_none(*pmd) || pmd_bad(*pmd)) {
        pr_err("Invalid PMD entry\n");
        return;
    }
    
    /*
     * Check for 2MB section/block at PMD level
     * ARM64 uses "section" terminology for large page mappings
     * pmd_sect() detects if this PMD entry points to a 2MB block
     * rather than to a page table containing individual page entries
     */
    if (pmd_sect(*pmd)) {
        pr_info("2MB Section/Block detected\n");
        paddr = (pmd_val(*pmd) & PMD_MASK) | (vaddr & ~PMD_MASK);
        pr_info("Physical Address: 0x%016lx (via 2MB block)\n", paddr);
        
        /*
         * Access content within 2MB section
         * Sections are compound pages, so we need to find the specific
         * page within the section that corresponds to our virtual address
         */
        page = pmd_page(*pmd);
        if (page) {
            pr_info("Page struct: %px, PFN: %lu\n", page, page_to_pfn(page));
            
            /*
             * Calculate page offset within the 2MB section
             * Note: ARM64 uses different calculation due to section addressing
             */
            unsigned long page_offset = (vaddr & PMD_MASK) >> PAGE_SHIFT;
            struct page *target_page = page + page_offset;
            
            /* Direct access to page memory with byte offset calculation */
            mapped_addr = (char *)page_address(target_page) + (vaddr & ~PAGE_MASK);
            pr_info("Original content: '%.20s'\n", mapped_addr);
            
            /* 
             * Demonstrate physical memory modification by converting lowercase to uppercase
             * Loop safely bounds-checks against both string length and null terminator
             */
            int i;
            for (i = 0; i < strlen(TEST_STRING) && mapped_addr[i]; i++) {
                if (mapped_addr[i] >= 'a' && mapped_addr[i] <= 'z') {
                    mapped_addr[i] = mapped_addr[i] - 'a' + 'A';
                }
            }
            
            pr_info("Modified content: '%.20s'\n", mapped_addr);
        }
        return;
    }
    
    pte = pte_offset_kernel(pmd, vaddr);
    pr_info("PTE: %px -> 0x%016lx (L4)\n", pte, pte_val(*pte));
    if (pte_none(*pte)) {
        pr_err("Invalid PTE entry\n");
        return;
    }
    
    /*
     * Extract physical address from PTE
     * ARM64 uses PTE_ADDR_MASK to isolate physical frame bits
     * Combine with byte offset within page for final physical address
     * Page size varies: 4KB, 16KB, or 64KB depending on kernel config
     */
    paddr = (pte_val(*pte) & PTE_ADDR_MASK) | (vaddr & ~PAGE_MASK);
    pr_info("Physical Address: 0x%016lx (via %luKB page)\n", paddr, PAGE_SIZE/1024);
    
    page = pte_page(*pte);
    if (page) {
        pr_info("Page struct: %px, PFN: %lu\n", page, page_to_pfn(page));
        
        /* Direct access to page memory with byte offset calculation */
        mapped_addr = (char *)page_address(page) + (vaddr & ~PAGE_MASK);
        pr_info("Original content: '%.20s'\n", mapped_addr);
        
        /* 
         * Demonstrate physical memory modification by converting lowercase to uppercase
         * Loop safely bounds-checks against both string length and null terminator
         */
        int i;
        for (i = 0; i < strlen(TEST_STRING) && mapped_addr[i]; i++) {
            if (mapped_addr[i] >= 'a' && mapped_addr[i] <= 'z') {
                mapped_addr[i] = mapped_addr[i] - 'a' + 'A';
            }
        }
        
        pr_info("Modified content: '%.20s'\n", mapped_addr);
    }
    
    pr_info("Page table walk completed successfully\n");
}

static int __init arm64_pagewalk_init(void)
{
    pr_info("=== ARM64 Page Table Walk Demo ===\n");
    
    test_page = alloc_pages(GFP_KERNEL, 0);
    if (test_page) {
        void *page_addr = page_address(test_page);
        strcpy((char *)page_addr, TEST_STRING);
        pr_info("\nTest 1: alloc_pages() allocation\n");
        arm64_walk_page_table((unsigned long)page_addr, "alloc_pages");
    } else {
        pr_err("alloc_pages failed\n");
    }
    
    kmalloc_ptr = kmalloc(PAGE_SIZE, GFP_KERNEL);
    if (kmalloc_ptr) {
        strcpy((char *)kmalloc_ptr, TEST_STRING);
        pr_info("\nTest 2: kmalloc() allocation\n");
        arm64_walk_page_table((unsigned long)kmalloc_ptr, "kmalloc");
    } else {
        pr_err("kmalloc failed\n");
    }
    
    vmalloc_ptr = vmalloc(PAGE_SIZE);
    if (vmalloc_ptr) {
        strcpy((char *)vmalloc_ptr, TEST_STRING);
        pr_info("\nTest 3: vmalloc() allocation\n");
        arm64_walk_page_table((unsigned long)vmalloc_ptr, "vmalloc");
    } else {
        pr_err("vmalloc failed\n");
    }
    
    pr_info("\nARM64 page table structure:\n");
    #ifdef CONFIG_ARM64_4K_PAGES
    pr_info("  4KB Pages (CONFIG_ARM64_4K_PAGES):\n");
    pr_info("  L0 (PGD): 512GB per entry, 9 bits [47:39]\n");
    pr_info("  L1 (P4D): 1GB per entry,   9 bits [38:30]\n");
    pr_info("  L2 (PUD): 2MB per entry,   9 bits [29:21]\n");
    pr_info("  L3 (PMD): 4KB per entry,   9 bits [20:12]\n");
    pr_info("  Offset:   Byte in page,   12 bits [11:0]\n");
    #elif CONFIG_ARM64_16K_PAGES
    pr_info("  16KB Pages (CONFIG_ARM64_16K_PAGES):\n");
    pr_info("  L0 (PGD): 128TB per entry, 1 bit [47]\n");
    pr_info("  L1 (P4D): 64GB per entry,  11 bits [46:36]\n");
    pr_info("  L2 (PUD): 32MB per entry,  11 bits [35:25]\n");
    pr_info("  L3 (PMD): 16KB per entry,  11 bits [24:14]\n");
    pr_info("  Offset:   Byte in page,   14 bits [13:0]\n");
    #elif CONFIG_ARM64_64K_PAGES
    pr_info("  64KB Pages (CONFIG_ARM64_64K_PAGES):\n");
    pr_info("  L0 (PGD): Unused\n");
    pr_info("  L1 (P4D): 4TB per entry,   6 bits [47:42]\n");
    pr_info("  L2 (PUD): 512MB per entry, 13 bits [41:29]\n");
    pr_info("  L3 (PMD): 64KB per entry,  13 bits [28:16]\n");
    pr_info("  Offset:   Byte in page,   16 bits [15:0]\n");
    #else
    pr_info("  Default configuration\n");
    #endif
    
    return 0;
}

static void __exit arm64_pagewalk_exit(void)
{
    if (test_page) {
        pr_info("Freeing alloc_pages memory\n");
        __free_pages(test_page, 0);
    }
    
    if (kmalloc_ptr) {
        pr_info("Freeing kmalloc memory\n");
        kfree(kmalloc_ptr);
    }
    
    if (vmalloc_ptr) {
        pr_info("Freeing vmalloc memory\n");
        vfree(vmalloc_ptr);
    }
    
    pr_info("ARM64 Page Table Walk Demo unloaded\n");
}

module_init(arm64_pagewalk_init);
module_exit(arm64_pagewalk_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("ARM64 4-level page table walk demonstration");
