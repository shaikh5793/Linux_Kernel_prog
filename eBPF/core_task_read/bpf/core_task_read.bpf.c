/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is licensed under GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

/*
 * eBPF Headers - Required for kernel data structures and CO-RE functionality
 */
#include "vmlinux.h"            /* Complete kernel data structure definitions */
#include <bpf/bpf_helpers.h>    /* BPF helper functions */
#include <bpf/bpf_tracing.h>     /* Tracing-related definitions */
#include <bpf/bpf_core_read.h>   /* CO-RE (Compile Once - Run Everywhere) functions */

/*
 * License Declaration - Required for eBPF programs
 * Must be GPL-compatible to access kernel internal structures
 */
char LICENSE[] SEC("license") = "GPL";

/*
 * Main eBPF Program - Process Genealogy Reader using CO-RE
 * Attaches to: sys_enter_execve tracepoint
 * Triggers: Every time any process calls execve() system call
 * Purpose: Demonstrate safe kernel structure reading across kernel versions
 * Technology: Uses CO-RE for kernel-version-independent field access
 */
SEC("tracepoint/syscalls/sys_enter_execve")
int handle_exec(struct trace_event_raw_sys_enter *ctx)
{
    /*
     * Get Current Task Structure
     * Retrieve pointer to the current process's task_struct
     * This structure contains all process-related information in the kernel
     */
    struct task_struct *task = (struct task_struct *)bpf_get_current_task();

    /*
     * Local variables for process information
     */
    pid_t tgid = 0;         /* Thread Group ID (process ID) */
    pid_t ppid = 0;         /* Parent Process ID */
    char comm[16] = {};     /* Process command name */

    /*
     * Step 1: Read Process ID using CO-RE
     * Use CO-RE to safely read the tgid field from task_struct
     * CO-RE automatically handles field offset differences across kernel versions
     * This is much safer than direct memory access or hardcoded offsets
     */
    bpf_core_read(&tgid, sizeof(tgid), &task->tgid);

    /*
     * Step 2: Navigate to Parent Process Structure
     * Read the pointer to the parent task_struct
     * task->real_parent points to the actual parent process
     * Note: real_parent vs parent - real_parent is the biological parent
     */
    struct task_struct *real_parent = NULL;
    bpf_core_read(&real_parent, sizeof(real_parent), &task->real_parent);

    /*
     * Step 3: Read Parent Process ID
     * If parent exists, read its tgid field
     * Two-step process: first get parent pointer, then read parent's data
     * Check for NULL pointer to prevent kernel panic
     */
    if (real_parent)
        bpf_core_read(&ppid, sizeof(ppid), &real_parent->tgid);

    /*
     * Step 4: Get Process Name
     * Use standard helper to get current process command name
     * This is simpler than reading from task_struct->comm via CO-RE
     */
    bpf_get_current_comm(&comm, sizeof(comm));

    /*
     * Output the Process Genealogy Information
     * Print process name, process ID, and parent process ID
     * Output goes to kernel trace buffer (/sys/kernel/debug/tracing/trace_pipe)
     */
    bpf_printk("core_read exec: comm=%s tgid=%d ppid=%d\n", comm, tgid, ppid);

    /*
     * Return success
     * Process information has been successfully extracted and logged
     */
    return 0;
}

