/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is licensed under GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

/*
 * eBPF Headers - Required for kernel data structures and BPF helper functions
 */
#include "vmlinux.h"        /* Kernel data structures and definitions */
#include <bpf/bpf_helpers.h> /* BPF helper functions for ring buffer operations */
#include <bpf/bpf_tracing.h>  /* Tracing-related definitions and macros */
#include "ringbuf_exec.h"     /* Shared event structure definition */

/*
 * Ring Buffer Map Definition - High-performance event streaming buffer
 * This ring buffer enables real-time communication between kernel and user space
 */
struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);    /* Ring buffer type for event streaming */
    __uint(max_entries, 1 << 24);          /* 16MB buffer size (16,777,216 bytes) */
} events SEC(".maps");

/*
 * License Declaration - Required for eBPF programs
 * Must be GPL-compatible to access kernel tracing functions
 */
char LICENSE[] SEC("license") = "GPL";

/*
 * Main eBPF Program - Real-time Process Execution Monitor
 * Attaches to: sys_enter_execve tracepoint
 * Triggers: Every time any process calls execve() system call
 * Purpose: Stream individual execution events to user space in real-time
 */
SEC("tracepoint/syscalls/sys_enter_execve")
int handle_exec(struct trace_event_raw_sys_enter *ctx)
{
    /*
     * Step 1: Reserve space in the ring buffer
     * Allocate memory for one event structure in the ring buffer
     * This is a zero-copy operation - data written directly to shared memory
     */
    struct event *e = bpf_ringbuf_reserve(&events, sizeof(*e), 0);

    /*
     * Check if reservation was successful
     * Ring buffer can be full during high event rates
     * Gracefully handle failure by returning without crashing
     */
    if (!e)
        return 0;

    /*
     * Step 2: Populate the event structure with process and execution information
     * Extract process ID from the upper 32 bits of the combined PID/TGID value
     */
    e->pid = bpf_get_current_pid_tgid() >> 32;

    /*
     * Read the filename being executed from execve() arguments
     * ctx->args[0] contains the pointer to the filename string
     * Use probe_read_user_str to safely read the string from user space
     */
    const char *filename_ptr = (const char *)ctx->args[0];
    long ret = bpf_probe_read_user_str(e->comm, sizeof(e->comm), filename_ptr);

    /*
     * If reading the filename fails, fall back to current process name
     * This ensures we always have some identifier for the process
     */
    if (ret < 0) {
        bpf_get_current_comm(&e->comm, sizeof(e->comm));
    }

    /*
     * Step 3: Submit the event to user space
     * Make the populated event available for user space consumption
     * This triggers notifications to waiting user space consumers
     */
    bpf_ringbuf_submit(e, 0);

    /*
     * Return success code
     * Event has been successfully queued for user space processing
     */
    return 0;
}
