/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is licensed under GPL v2.
 * See the accompanying LICENSE file for the full text.
 *
 * Hello eBPF - Kernel Space Program
 *
 * This eBPF program runs in kernel space and demonstrates:
 * - Attaching to a syscall tracepoint
 * - Extracting process information
 * - Using eBPF helper functions
 * - Printing trace output
 */

#include "vmlinux.h"          // Kernel type definitions generated from BTF
#include <bpf/bpf_helpers.h>  // eBPF helper function definitions
#include <bpf/bpf_tracing.h>  // eBPF tracing macros and utilities

/*
 * License declaration - required for eBPF programs
 * Must be GPL-compatible to access certain kernel functions
 */
char LICENSE[] SEC("license") = "GPL";

/*
 * eBPF program to trace execve() system calls
 *
 * SEC() macro specifies the program type and attachment point:
 * - "tracepoint" = program type for kernel tracepoints
 * - "syscalls/sys_enter_execve" = specific tracepoint for execve syscall entry
 *
 * This program will be called every time any process calls execve()
 */
SEC("tracepoint/syscalls/sys_enter_execve")
int tp__sys_enter_execve(struct trace_event_raw_sys_enter *ctx) {
    /*
     * Get the current process and thread ID
     * bpf_get_current_pid_tgid() returns a 64-bit value where:
     * - Upper 32 bits = PID (Process ID)
     * - Lower 32 bits = TID (Thread ID)
     */
    __u64 id = bpf_get_current_pid_tgid();
    __u32 pid = id >> 32;  // Extract PID from upper 32 bits
    char comm[16];         // Buffer for process name (max 16 chars in Linux)

    /*
     * Get the current process name (command name)
     * bpf_get_current_comm() fills the buffer with the process name
     * This is the same name you see in 'ps' command output
     */
    bpf_get_current_comm(comm, sizeof(comm));

    /*
     * Print a trace message using bpf_printk()
     * Output appears in /sys/kernel/debug/tracing/trace_pipe
     * Note: bpf_printk() is limited and mainly for debugging
     */
    bpf_printk("hello_bpf: execve by pid=%d comm=%s\n", pid, comm);

    /*
     * Return 0 to indicate successful execution
     * Non-zero return could filter or modify behavior in some contexts
     */
    return 0;
}

