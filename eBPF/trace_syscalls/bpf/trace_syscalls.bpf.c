/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is licensed under GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

/*
 * Trace Syscalls - eBPF Kernel Space Programs
 *
 * This file contains multiple eBPF programs that demonstrate:
 * - Multiple tracepoint attachments in a single object
 * - Different system call tracing (openat, read, write)
 * - Process information extraction using eBPF helpers
 * - Syscall argument access through tracepoint context
 * - Safe kernel-space programming with eBPF constraints
 */

#include "vmlinux.h"          // Kernel type definitions from BTF
#include <bpf/bpf_helpers.h>  // eBPF helper function prototypes
#include <bpf/bpf_tracing.h>  // eBPF tracing utilities and macros

/*
 * License declaration - required for all eBPF programs
 * GPL license allows access to GPL-only kernel functions and helpers
 */
char LICENSE[] SEC("license") = "GPL";

/*
 * eBPF Program #1: Trace openat() system calls
 *
 * SEC() macro defines:
 * - Program type: "tracepoint" (for kernel tracepoints)
 * - Attachment point: "syscalls/sys_enter_openat" (openat syscall entry)
 *
 * Modern Linux uses openat() instead of the legacy open() syscall.
 * This program executes every time any process calls openat().
 */
SEC("tracepoint/syscalls/sys_enter_openat")
int trace_open(struct trace_event_raw_sys_enter *ctx) {
    /*
     * Extract process and thread identifiers
     * bpf_get_current_pid_tgid() is an eBPF helper that returns:
     * - Bits 63-32: PID (Process ID)
     * - Bits 31-0:  TID (Thread ID)
     */
    __u64 id = bpf_get_current_pid_tgid();
    __u32 pid = id >> 32;  // Extract PID from upper 32 bits
    __u32 tid = id;        // Extract TID from lower 32 bits (implicit truncation)

    /*
     * Get the current process name
     * Buffer must be exactly 16 bytes (TASK_COMM_LEN in kernel)
     */
    char comm[16];
    bpf_get_current_comm(comm, sizeof(comm));

    /*
     * Access syscall arguments through tracepoint context
     * For openat() syscall, the arguments are:
     * args[0] = dirfd (directory file descriptor, or AT_FDCWD)
     * args[1] = pathname (pointer to filename string)
     * args[2] = flags (open flags: O_RDONLY, O_WRONLY, O_CREAT, etc.)
     * args[3] = mode (file permissions, used if O_CREAT is set)
     *
     * Note: We can't safely dereference args[1] (pathname pointer)
     * in eBPF without using proper helper functions
     */

    /*
     * Output trace information using bpf_printk()
     * This appears in /sys/kernel/tracing/trace_pipe
     * Limited formatting options compared to regular printf()
     */
    bpf_printk("OPEN: PID=%d TID=%d COMM=%s dirfd=%ld flags=%ld\n",
               pid, tid, comm, ctx->args[0], ctx->args[2]);

    return 0;  // Success - continue normal syscall execution
}

/*
 * eBPF Program #2: Trace read() system calls
 *
 * This program attaches to the read() syscall entry tracepoint.
 * It executes in kernel context whenever any process performs a read operation.
 */
SEC("tracepoint/syscalls/sys_enter_read")
int trace_read(struct trace_event_raw_sys_enter *ctx) {
    /*
     * Same process identification pattern as trace_open()
     * This is a common pattern in eBPF tracing programs
     */
    __u64 id = bpf_get_current_pid_tgid();
    __u32 pid = id >> 32;  // Process ID
    __u32 tid = id;        // Thread ID

    char comm[16];
    bpf_get_current_comm(comm, sizeof(comm));

    /*
     * Access read() syscall arguments:
     * args[0] = fd (file descriptor number)
     * args[1] = buf (pointer to user-space buffer - unsafe to access directly)
     * args[2] = count (number of bytes to read)
     *
     * eBPF Safety Note: We cannot safely dereference user-space pointers
     * like args[1] without using special helper functions like
     * bpf_probe_read_user() which perform safe memory access checks.
     */

    bpf_printk("READ: PID=%d TID=%d COMM=%s fd=%ld count=%ld\n",
               pid, tid, comm, ctx->args[0], ctx->args[2]);

    return 0;  // Allow syscall to proceed normally
}

/*
 * eBPF Program #3: Trace write() system calls
 *
 * This program demonstrates how multiple eBPF programs can coexist
 * in the same object file and be managed together by the user-space loader.
 * Each program attaches to a different tracepoint independently.
 */
SEC("tracepoint/syscalls/sys_enter_write")
int trace_write(struct trace_event_raw_sys_enter *ctx) {
    /*
     * Extract process information - same pattern as other programs
     * Note: We only extract PID here, not TID, to show variation
     */
    __u64 id = bpf_get_current_pid_tgid();
    __u32 pid = id >> 32;  // Only extracting PID for this example

    char comm[16];
    bpf_get_current_comm(comm, sizeof(comm));

    /*
     * Access write() syscall arguments:
     * args[0] = fd (file descriptor to write to)
     * args[1] = buf (pointer to data to write - user-space pointer)
     * args[2] = count (number of bytes to write)
     *
     * Write operations can be high-frequency, so this might generate
     * a lot of trace output. In production, you might want to add
     * filtering logic here to reduce noise.
     */

    bpf_printk("WRITE: PID=%d COMM=%s fd=%ld count=%ld\n",
               pid, comm, ctx->args[0], ctx->args[2]);

    return 0;  // Continue with normal write operation
}
