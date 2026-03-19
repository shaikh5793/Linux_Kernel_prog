/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 *
 * VMA Inspector Kernel Module
 * 
 * This module provides a debugfs interface to inspect Virtual Memory Areas
 * (VMAs) of the calling process. It displays detailed information about
 * memory layout, VMA properties, flags, and statistics.
 * 
 * Features:
 * 1. Complete VMA listing with start/end addresses
 * 2. Memory region classification (heap, stack, anonymous, file-backed)
 * 3. VMA flags decoding (permissions, special properties)
 * 4. Memory statistics (RSS, virtual memory usage)
 * 5. Cross-kernel version compatibility (6.1+ and older)
 * 
 * Usage: Mount debugfs and read /sys/kernel/debug/inspectvma/vma_details
 * Applications: Memory debugging, VMA behavior analysis, educational purposes
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/pid.h>
#include <linux/slab.h>
#include <linux/mmap_lock.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
#include <linux/mm_types.h>
#endif

static struct dentry *debugfs_dir;
static struct dentry *vma_file;

/*
 * vma_flags_to_string() - Convert VMA flags to human-readable string
 * Called by: vma_show() for each VMA to decode permission flags
 */
static const char *vma_flags_to_string(unsigned long flags)
{
    static char buffer[128];
    int pos = 0;
    
    buffer[0] = '\0';
    
    if (flags & VM_READ) pos += snprintf(buffer + pos, sizeof(buffer) - pos, "r");
    else pos += snprintf(buffer + pos, sizeof(buffer) - pos, "-");
    
    if (flags & VM_WRITE) pos += snprintf(buffer + pos, sizeof(buffer) - pos, "w");
    else pos += snprintf(buffer + pos, sizeof(buffer) - pos, "-");
    
    if (flags & VM_EXEC) pos += snprintf(buffer + pos, sizeof(buffer) - pos, "x");
    else pos += snprintf(buffer + pos, sizeof(buffer) - pos, "-");
    
    if (flags & VM_SHARED) pos += snprintf(buffer + pos, sizeof(buffer) - pos, "s");
    else pos += snprintf(buffer + pos, sizeof(buffer) - pos, "p");
    
    pos += snprintf(buffer + pos, sizeof(buffer) - pos, " ");
    
    if (flags & VM_GROWSDOWN) pos += snprintf(buffer + pos, sizeof(buffer) - pos, "gd ");
    if (flags & VM_GROWSUP) pos += snprintf(buffer + pos, sizeof(buffer) - pos, "gu ");
    if (flags & VM_LOCKED) pos += snprintf(buffer + pos, sizeof(buffer) - pos, "lo ");
    if (flags & VM_IO) pos += snprintf(buffer + pos, sizeof(buffer) - pos, "io ");
    if (flags & VM_SEQ_READ) pos += snprintf(buffer + pos, sizeof(buffer) - pos, "sr ");
    if (flags & VM_RAND_READ) pos += snprintf(buffer + pos, sizeof(buffer) - pos, "rr ");
    if (flags & VM_DONTCOPY) pos += snprintf(buffer + pos, sizeof(buffer) - pos, "dc ");
    if (flags & VM_DONTEXPAND) pos += snprintf(buffer + pos, sizeof(buffer) - pos, "de ");
    if (flags & VM_HUGETLB) pos += snprintf(buffer + pos, sizeof(buffer) - pos, "ht ");
    if (flags & VM_NOHUGEPAGE) pos += snprintf(buffer + pos, sizeof(buffer) - pos, "nh ");
    if (flags & VM_MERGEABLE) pos += snprintf(buffer + pos, sizeof(buffer) - pos, "mg ");
    
    return buffer;
}

/*
 * get_vma_type() - Determine VMA type (heap, stack, anonymous, or file-backed)
 * Called by: vma_show() for each VMA to classify memory region
 */
static const char *get_vma_type(struct vm_area_struct *vma)
{
    if (!vma->vm_file) {
        if (vma->vm_start <= vma->vm_mm->start_brk && 
            vma->vm_end >= vma->vm_mm->brk)
            return "[heap]";
        else if (vma->vm_start <= vma->vm_mm->start_stack &&
                 vma->vm_end >= vma->vm_mm->start_stack)
            return "[stack]";
        else
            return "[anon]";
    } else {
        return vma->vm_file->f_path.dentry->d_name.name;
    }
}

/*
 * vma_show() - Main function to generate VMA information for calling process
 * Called by: seq_file framework when user reads debugfs file
 * Call chain: user fopen() -> vma_open() -> user fread() -> seq_read() -> vma_show()
 * Context: Runs in context of process reading the file, 'current' = calling process
 */
static int vma_show(struct seq_file *m, void *v)
{
    struct task_struct *task;
    struct mm_struct *mm;
    struct vm_area_struct *vma;
    unsigned long total_size = 0;
    int vma_count = 0;
    
    task = current;
    mm = task->mm;
    
    if (!mm) {
        seq_printf(m, "No memory descriptor for current process\n");
        return 0;
    }
    
    seq_printf(m, "VMA Inspector for Process: %s (PID: %d)\n", task->comm, task->pid);
    seq_printf(m, "==========================================================\n\n");
    
    seq_printf(m, "Memory Layout Overview:\n");
    seq_printf(m, "  Code Start:  0x%08lx\n", mm->start_code);
    seq_printf(m, "  Code End:    0x%08lx\n", mm->end_code);
    seq_printf(m, "  Data Start:  0x%08lx\n", mm->start_data);
    seq_printf(m, "  Data End:    0x%08lx\n", mm->end_data);
    seq_printf(m, "  Heap Start:  0x%08lx\n", mm->start_brk);
    seq_printf(m, "  Heap End:    0x%08lx\n", mm->brk);
    seq_printf(m, "  Stack Start: 0x%08lx\n", mm->start_stack);
    seq_printf(m, "  Arg Start:   0x%08lx\n", mm->arg_start);
    seq_printf(m, "  Arg End:     0x%08lx\n", mm->arg_end);
    seq_printf(m, "  Env Start:   0x%08lx\n", mm->env_start);
    seq_printf(m, "  Env End:     0x%08lx\n", mm->env_end);
    seq_printf(m, "\n");
    
    mmap_read_lock(mm);
    
    seq_printf(m, "Virtual Memory Areas (VMAs):\n");
    seq_printf(m, "%-18s %-18s %-10s %-6s %-20s %-s\n", 
               "START", "END", "SIZE", "OFFSET", "FLAGS", "TYPE/FILE");
    seq_printf(m, "%-18s %-18s %-10s %-6s %-20s %-s\n",
               "==================", "==================", "==========",
               "======", "====================", "================");
    
    /* Use VMA iterator for newer kernels */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
    struct vma_iterator vmi;
    vma_iter_init(&vmi, mm, 0);
    for_each_vma(vmi, vma) {
#else
    for (vma = mm->mmap; vma; vma = vma->vm_next) {
#endif
        unsigned long size = vma->vm_end - vma->vm_start;
        const char *flags_str = vma_flags_to_string(vma->vm_flags);
        const char *type_str = get_vma_type(vma);
        
        seq_printf(m, "0x%016lx 0x%016lx %8luk %6lx %-20s %s\n",
                   vma->vm_start, vma->vm_end, size / 1024,
                   vma->vm_file ? (vma->vm_pgoff << PAGE_SHIFT) : 0,
                   flags_str, type_str);
        
        total_size += size;
        vma_count++;
    }
    
    seq_printf(m, "\nSummary:\n");
    seq_printf(m, "  Total VMAs: %d\n", vma_count);
    seq_printf(m, "  Total Virtual Memory Mapped: %lu KB (%lu MB)\n", 
               total_size / 1024, total_size / (1024 * 1024));
    seq_printf(m, "  RSS (Resident Set Size): %lu KB\n", 
               get_mm_rss(mm) * PAGE_SIZE / 1024);
    seq_printf(m, "  Virtual Memory Peak: %lu KB\n", 
               mm->hiwater_vm * PAGE_SIZE / 1024);
    seq_printf(m, "  RSS Peak: %lu KB\n", 
               mm->hiwater_rss * PAGE_SIZE / 1024);
    
    mmap_read_unlock(mm);
    
    seq_printf(m, "\nFlag Legend:\n");
    seq_printf(m, "  r/w/x/s = read/write/execute/shared\n");
    seq_printf(m, "  gd/gu = grows down/up, lo = locked, io = I/O mapping\n");
    seq_printf(m, "  sr/rr = sequential/random read, dc = don't copy\n");
    seq_printf(m, "  de = don't expand, ht = huge pages, nh = no huge pages\n");
    seq_printf(m, "  mg = mergeable\n");
    
    return 0;
}

/*
 * vma_open() - Called when user opens the debugfs file
 * Called by: VFS when user does fopen("/sys/kernel/debug/inspectvma/vma_details", "r")
 * Purpose: Sets up seq_file framework with vma_show() as content generator
 */
static int vma_open(struct inode *inode, struct file *file)
{
    return single_open(file, vma_show, NULL);
}

static const struct file_operations vma_fops = {
    .open = vma_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static int __init inspectvma_init(void)
{
    debugfs_dir = debugfs_create_dir("inspectvma", NULL);
    if (!debugfs_dir) {
        pr_err("Failed to create debugfs directory\n");
        return -ENOMEM;
    }
    
    vma_file = debugfs_create_file("vma_details", 0444, debugfs_dir, NULL, &vma_fops);
    if (!vma_file) {
        pr_err("Failed to create vma_details file\n");
        debugfs_remove_recursive(debugfs_dir);
        return -ENOMEM;
    }
    
    pr_info("VMA Inspector loaded: /sys/kernel/debug/inspectvma/vma_details\n");
    return 0;
}

static void __exit inspectvma_exit(void)
{
    debugfs_remove_recursive(debugfs_dir);
    pr_info("VMA Inspector unloaded\n");
}

module_init(inspectvma_init);
module_exit(inspectvma_exit);

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("VMA Inspector - Virtual Memory Area analysis and debugging");
MODULE_LICENSE("Dual MIT/GPL");
