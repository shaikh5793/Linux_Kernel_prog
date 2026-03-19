/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 * 
 * Kernel module for userspace address translation demonstration
 * Receives userspace virtual addresses via debugfs and demonstrates page table walking
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/pgtable.h>
#include <linux/highmem.h>
#include <linux/sched.h>
#include <linux/pid.h>
#include <linux/slab.h>

#define DEBUGFS_DIR "userwalk"
#define MAX_ADDR_LEN 32
#define TEST_STRING "userspace"

static struct dentry *debugfs_root;
static struct dentry *vaddr_file;
static struct dentry *status_file;
static char status_buffer[512];
static struct task_struct *target_task = NULL;

/*
 * Validate virtual address against VMA before page table walking
 * Returns 0 on success, negative error code on failure
 */
static int validate_user_address(struct mm_struct *mm, unsigned long vaddr)
{
    struct vm_area_struct *vma;
    
    if (!mm) {
        pr_err("[USERWALK] No mm_struct available for validation\n");
        return -EINVAL;
    }
    
    mmap_read_lock(mm);
    
    vma = find_vma(mm, vaddr);
    if (!vma || vaddr < vma->vm_start) {
        pr_err("[USERWALK] Address 0x%lx is not in any VMA (unmapped)\n", vaddr);
        mmap_read_unlock(mm);
        return -EFAULT;
    }
    
    pr_info("VMA found: 0x%lx-0x%lx\n", vma->vm_start, vma->vm_end);
    pr_info("VMA flags: 0x%lx (%s%s%s%s)\n", vma->vm_flags,
            (vma->vm_flags & VM_READ) ? "r" : "-",
            (vma->vm_flags & VM_WRITE) ? "w" : "-",
            (vma->vm_flags & VM_EXEC) ? "x" : "-",
            (vma->vm_flags & VM_SHARED) ? "s" : "p");
    
    /* Check if address has appropriate permissions for our operation */
    if (!(vma->vm_flags & VM_READ)) {
        pr_warn("[USERWALK] VMA is not readable\n");
    }
    if (!(vma->vm_flags & VM_WRITE)) {
        pr_warn("[USERWALK] VMA is not writable - may trigger COW\n");
    }
    
    mmap_read_unlock(mm);
    return 0;
}

/*
 * Walk userspace page table for given virtual address
 * Uses the target process's mm_struct instead of kernel's init_mm
 */
static int walk_userspace_page_table(struct mm_struct *mm, unsigned long vaddr)
{
    pgd_t *pgd;
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *pte;
    struct page *page;
    unsigned long paddr;
    char *mapped_addr;
    
    if (!mm) {
        pr_err("[USERWALK] No mm_struct available\n");
        return -EINVAL;
    }
    
    pr_info("\n=== Userspace Page Table Walk ===\n");
    pr_info("Target PID: %d, Command: %s\n", target_task->pid, target_task->comm);
    pr_info("Virtual Address: 0x%016lx\n", vaddr);
    pr_info("MM struct: %px\n", mm);
    
    /* Take mmap_lock to safely access page tables */
    mmap_read_lock(mm);
    
    pgd = pgd_offset(mm, vaddr);
    pr_info("PGD: %px -> 0x%016lx\n", pgd, pgd_val(*pgd));
    if (pgd_none(*pgd) || pgd_bad(*pgd)) {
        pr_err("Invalid PGD entry\n");
        mmap_read_unlock(mm);
        return -EFAULT;
    }
    
    p4d = p4d_offset(pgd, vaddr);
    pr_info("P4D: %px -> 0x%016lx\n", p4d, p4d_val(*p4d));
    if (p4d_none(*p4d) || p4d_bad(*p4d)) {
        pr_err("Invalid P4D entry\n");
        mmap_read_unlock(mm);
        return -EFAULT;
    }
    
    pud = pud_offset(p4d, vaddr);
    pr_info("PUD: %px -> 0x%016lx\n", pud, pud_val(*pud));
    if (pud_none(*pud)) {
        pr_err("PUD entry is empty - page not present\n");
        mmap_read_unlock(mm);
        return -EFAULT;
    }
    
    /*
     * Check for 1GB huge page at PUD level
     * Less common in userspace but possible with hugetlbfs
     */
    if (pud_leaf(*pud)) {
        pr_info("1GB Huge Page detected in userspace\n");
        paddr = (pud_val(*pud) & PUD_MASK) | (vaddr & ~PUD_MASK);
        pr_info("Physical Address: 0x%016lx (via 1GB page)\n", paddr);
        mmap_read_unlock(mm);
        return 0;
    }
    
    if (pud_bad(*pud)) {
        pr_err("Invalid PUD entry\n");
        mmap_read_unlock(mm);
        return -EFAULT;
    }
    
    pmd = pmd_offset(pud, vaddr);
    pr_info("PMD: %px -> 0x%016lx\n", pmd, pmd_val(*pmd));
    if (pmd_none(*pmd)) {
        pr_err("PMD entry is empty - page not present\n");
        mmap_read_unlock(mm);
        return -EFAULT;
    }
    
    /*
     * Check for 2MB huge page (Transparent Huge Pages)
     * More common in userspace applications with large allocations
     */
    if (pmd_leaf(*pmd)) {
        pr_info("2MB Huge Page (THP) detected in userspace\n");
        paddr = (pmd_val(*pmd) & PMD_MASK) | (vaddr & ~PMD_MASK);
        pr_info("Physical Address: 0x%016lx (via 2MB THP)\n", paddr);
        
        /* Access and modify content within THP */
        page = pmd_page(*pmd);
        if (page) {
            pr_info("Page struct: %px, PFN: %lu\n", page, page_to_pfn(page));
            
            /* Calculate which 4KB page within the 2MB THP contains our address */
            unsigned long page_offset = (vaddr & ~PMD_MASK) >> PAGE_SHIFT;
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
        mmap_read_unlock(mm);
        return 0;
    }
    
    if (pmd_bad(*pmd)) {
        pr_err("Invalid PMD entry\n");
        mmap_read_unlock(mm);
        return -EFAULT;
    }
    
    pte = pte_offset_kernel(pmd, vaddr);
    
    pr_info("PTE: %px -> 0x%016lx\n", pte, pte_val(*pte));
    if (pte_none(*pte)) {
        pr_err("PTE entry is empty - page not present\n");
        mmap_read_unlock(mm);
        return -EFAULT;
    }
    
    if (!pte_present(*pte)) {
        pr_info("Page is swapped out or not present\n");
        mmap_read_unlock(mm);
        return -EFAULT;
    }
    
    /*
     * Extract physical address from PTE
     * For userspace pages, check permissions and handle copy-on-write
     */
    paddr = (pte_val(*pte) & PTE_PFN_MASK) | (vaddr & ~PAGE_MASK);
    pr_info("Physical Address: 0x%016lx (via 4KB page)\n", paddr);
    pr_info("Page permissions: %s%s\n",
            pte_write(*pte) ? "W" : "-",
            pte_exec(*pte) ? "X" : "-");
    
    /* Get page and modify content */
    page = pte_page(*pte);
    if (page) {
        pr_info("Page struct: %px, PFN: %lu\n", page, page_to_pfn(page));
        
        /* Check if page is writable to avoid COW issues */
        if (!pte_write(*pte)) {
            pr_warn("Page is not writable - may trigger COW\n");
        }
        
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
    
    mmap_read_unlock(mm);
    
    pr_info("Userspace page table walk completed successfully\n");
    return 0;
}

static ssize_t vaddr_write(struct file *file, const char __user *user_buf,
                          size_t count, loff_t *ppos)
{
    char addr_str[MAX_ADDR_LEN];
    unsigned long vaddr;
    int ret;
    
    if (count >= MAX_ADDR_LEN)
        return -EINVAL;
    
    if (copy_from_user(addr_str, user_buf, count))
        return -EFAULT;
    
    addr_str[count] = '\0';
    
    /* Parse the virtual address */
    ret = kstrtoul(addr_str, 16, &vaddr);
    if (ret) {
        pr_err("[USERWALK] Invalid address format: %s\n", addr_str);
        snprintf(status_buffer, sizeof(status_buffer), 
                "Error: Invalid address format\n");
        return ret;
    }
    
    /* Find the process that wrote to us */
    target_task = current;
    if (!target_task || !target_task->mm) {
        pr_err("[USERWALK] No valid process context\n");
        snprintf(status_buffer, sizeof(status_buffer), 
                "Error: No valid process context\n");
        return -ESRCH;
    }
    
    pr_info("[USERWALK] Received address 0x%lx from PID %d (%s)\n", 
            vaddr, target_task->pid, target_task->comm);
    
    /* First validate the address against VMA */
    ret = validate_user_address(target_task->mm, vaddr);
    if (ret) {
        snprintf(status_buffer, sizeof(status_buffer), 
                "Error: Address validation failed (%d)\n", ret);
        return ret;
    }
    
    /* Perform the page table walk */
    ret = walk_userspace_page_table(target_task->mm, vaddr);
    if (ret) {
        snprintf(status_buffer, sizeof(status_buffer), 
                "Error: Page table walk failed (%d)\n", ret);
    } else {
        snprintf(status_buffer, sizeof(status_buffer), 
                "Success: Address translation completed\n");
    }
    
    return count;
}

static ssize_t status_read(struct file *file, char __user *user_buf,
                          size_t count, loff_t *ppos)
{
    return simple_read_from_buffer(user_buf, count, ppos, 
                                  status_buffer, strlen(status_buffer));
}

static const struct file_operations vaddr_fops = {
    .write = vaddr_write,
    .llseek = default_llseek,
};

static const struct file_operations status_fops = {
    .read = status_read,
    .llseek = default_llseek,
};

static int __init userwalk_init(void)
{
    pr_info("[USERWALK] Userspace address translation module loaded\n");
    
    /* Create debugfs directory */
    debugfs_root = debugfs_create_dir(DEBUGFS_DIR, NULL);
    if (IS_ERR(debugfs_root)) {
        pr_err("[USERWALK] Failed to create debugfs directory\n");
        return PTR_ERR(debugfs_root);
    }
    
    /* Create vaddr file for receiving addresses */
    vaddr_file = debugfs_create_file("vaddr", 0200, debugfs_root, 
                                    NULL, &vaddr_fops);
    if (IS_ERR(vaddr_file)) {
        pr_err("[USERWALK] Failed to create vaddr file\n");
        debugfs_remove_recursive(debugfs_root);
        return PTR_ERR(vaddr_file);
    }
    
    /* Create status file for reading results */
    status_file = debugfs_create_file("status", 0400, debugfs_root, 
                                     NULL, &status_fops);
    if (IS_ERR(status_file)) {
        pr_err("[USERWALK] Failed to create status file\n");
        debugfs_remove_recursive(debugfs_root);
        return PTR_ERR(status_file);
    }
    
    snprintf(status_buffer, sizeof(status_buffer), 
            "Ready: Waiting for userspace addresses\n");
    
    pr_info("[USERWALK] Debugfs interface created at /sys/kernel/debug/%s/\n", 
            DEBUGFS_DIR);
    pr_info("[USERWALK] Write addresses to: /sys/kernel/debug/%s/vaddr\n", 
            DEBUGFS_DIR);
    pr_info("[USERWALK] Read status from: /sys/kernel/debug/%s/status\n", 
            DEBUGFS_DIR);
    
    return 0;
}

static void __exit userwalk_exit(void)
{
    debugfs_remove_recursive(debugfs_root);
    pr_info("[USERWALK] Userspace address translation module unloaded\n");
}

module_init(userwalk_init);
module_exit(userwalk_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("Userspace address translation demonstration via debugfs");
MODULE_VERSION("1.0");
