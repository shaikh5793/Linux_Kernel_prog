/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 * 
 * x86_64 5-level page table walk
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
#include <asm/pgtable.h>

#define TEST_STRING "techveda"

static struct page *test_page = NULL;
static void *kmalloc_ptr = NULL;
static void *vmalloc_ptr = NULL;

static void x86_64_walk_page_table(unsigned long vaddr, const char *mem_type)
{
    pgd_t *pgd;
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *pte;
    struct page *page;
    unsigned long paddr;
    char *mapped_addr;
    
    pr_info("\n=== x86_64 Page Table Walk for %s ===\n", mem_type);
    pr_info("Virtual Address: 0x%016lx\n", vaddr);
    
    struct mm_struct *mm = current->mm;
    if (!mm) {
        pr_err("No mm_struct available\n");
        return;
    }
    
    pgd = pgd_offset(mm, vaddr);
    pr_info("PGD: %px -> 0x%016lx (L5)\n", pgd, pgd_val(*pgd));
    if (pgd_none(*pgd) || pgd_bad(*pgd)) {
        pr_err("Invalid PGD entry\n");
        return;
    }
    
    p4d = p4d_offset(pgd, vaddr);
    pr_info("P4D: %px -> 0x%016lx (L4)\n", p4d, p4d_val(*p4d));
    if (p4d_none(*p4d) || p4d_bad(*p4d)) {
        pr_err("Invalid P4D entry\n");
        return;
    }
    
    pud = pud_offset(p4d, vaddr);
    pr_info("PUD: %px -> 0x%016lx (L3)\n", pud, pud_val(*pud));
    if (pud_none(*pud) || pud_bad(*pud)) {
        pr_err("Invalid PUD entry\n");
        return;
    }
    
    /*
     * Check for 1GB huge page at PUD level
     * If PUD_LEAF is set, this PUD entry points directly to a 1GB page
     * rather than to the next level of page tables (PMD)
     */
    if (pud_leaf(*pud)) {
        pr_info("1GB Huge Page detected\n");
        paddr = (pud_val(*pud) & PUD_MASK) | (vaddr & ~PUD_MASK);
        pr_info("Physical Address: 0x%016lx (via 1GB page)\n", paddr);
        return;
    }
    
    pmd = pmd_offset(pud, vaddr);
    pr_info("PMD: %px -> 0x%016lx (L2)\n", pmd, pmd_val(*pmd));
    if (pmd_none(*pmd)) {
        pr_err("PMD entry is empty\n");
        return;
    }
    
    /*
     * Check for 2MB huge page at PMD level
     * PMD_LEAF indicates this PMD points directly to a 2MB page
     * This check must be done BEFORE pmd_bad() since huge page PMDs
     * have different bit patterns that might be considered "bad" for regular PMDs
     */
    if (pmd_leaf(*pmd)) {
        pr_info("2MB Huge Page detected\n");
        paddr = (pmd_val(*pmd) & PMD_MASK) | (vaddr & ~PMD_MASK);
        pr_info("Physical Address: 0x%016lx (via 2MB page)\n", paddr);
        
        /*
         * Access and modify content within 2MB huge page
         * Since huge pages are compound pages, we need to find the specific
         * 4KB page within the 2MB huge page that contains our virtual address
         */
        struct page *page = pmd_page(*pmd);
        if (page) {
            pr_info("Page struct: %px, PFN: %lu\n", page, page_to_pfn(page));
            
            /*
             * Calculate which 4KB page within the 2MB huge page contains our address
             * page_offset = (offset within 2MB page) / PAGE_SIZE
             */
            unsigned long page_offset = (vaddr & ~PMD_MASK) >> PAGE_SHIFT;
            struct page *target_page = page + page_offset;
            
            /* Direct access to page memory with byte offset calculation */
            char *mapped_addr = (char *)page_address(target_page) + (vaddr & ~PAGE_MASK);
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
    
    if (pmd_bad(*pmd)) {
        pr_err("Invalid PMD entry\n");
        return;
    }
    
    pte = pte_offset_kernel(pmd, vaddr);
    pr_info("PTE: %px -> 0x%016lx (L1)\n", pte, pte_val(*pte));
    if (pte_none(*pte)) {
        pr_err("Invalid PTE entry\n");
        return;
    }
    
    /*
     * Extract physical address from PTE
     * PTE_PFN_MASK extracts the Page Frame Number bits
     * Combine with byte offset within the page (vaddr & ~PAGE_MASK)
     */
    paddr = (pte_val(*pte) & PTE_PFN_MASK) | (vaddr & ~PAGE_MASK);
    pr_info("Physical Address: 0x%016lx (via 4KB page)\n", paddr);
    
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

static int __init x86_pagewalk_init(void)
{
    pr_info("=== x86_64 Page Table Walk Demo ===\n");
    
    test_page = alloc_pages(GFP_KERNEL, 0);
    if (test_page) {
        void *page_addr = page_address(test_page);
        strcpy((char *)page_addr, TEST_STRING);
        pr_info("\nTest 1: alloc_pages() allocation\n");
        x86_64_walk_page_table((unsigned long)page_addr, "alloc_pages");
    } else {
        pr_err("alloc_pages failed\n");
    }
    
    kmalloc_ptr = kmalloc(PAGE_SIZE, GFP_KERNEL);
    if (kmalloc_ptr) {
        strcpy((char *)kmalloc_ptr, TEST_STRING);
        pr_info("\nTest 2: kmalloc() allocation\n");
        x86_64_walk_page_table((unsigned long)kmalloc_ptr, "kmalloc");
    } else {
        pr_err("kmalloc failed\n");
    }
    
    vmalloc_ptr = vmalloc(PAGE_SIZE);
    if (vmalloc_ptr) {
        strcpy((char *)vmalloc_ptr, TEST_STRING);
        pr_info("\nTest 3: vmalloc() allocation\n");
        x86_64_walk_page_table((unsigned long)vmalloc_ptr, "vmalloc");
    } else {
        pr_err("vmalloc failed\n");
    }
    
    return 0;
}

static void __exit x86_pagewalk_exit(void)
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
    
    pr_info("x86_64 Page Table Walk Demo unloaded\n");
}

module_init(x86_pagewalk_init);
module_exit(x86_pagewalk_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("x86_64 5-level page table walk demonstration");
