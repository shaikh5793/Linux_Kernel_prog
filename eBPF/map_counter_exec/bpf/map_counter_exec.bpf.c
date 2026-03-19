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
#include "vmlinux.h"        /* Kernel data structures (like task_struct) */
#include <bpf/bpf_helpers.h> /* BPF helper functions (bpf_get_current_comm, etc.) */
#include <bpf/bpf_tracing.h>  /* Tracing-related definitions */

/*
 * BPF Map Definition - Hash table to store process execution counts
 * This map is shared between kernel-space eBPF program and user-space application
 */
struct {
    __uint(type, BPF_MAP_TYPE_HASH);    /* Hash map for key-value storage */
    __type(key, char[16]);              /* Key: process name (16 bytes max) */
    __type(value, __u64);               /* Value: execution count (64-bit) */
    __uint(max_entries, 1024);          /* Maximum 1024 different process names */
} exec_cnt SEC(".maps");

/*
 * License Declaration - Required for eBPF programs
 * Must be GPL-compatible to access certain kernel functions
 */
char LICENSE[] SEC("license") = "GPL";

/*
 * Main eBPF Program - Execve Counter
 * Attaches to: sys_enter_execve tracepoint
 * Triggers: Every time any process calls execve() system call
 * Purpose: Count how many times each process name executes
 */
SEC("tracepoint/syscalls/sys_enter_execve")
int handle_exec(struct trace_event_raw_sys_enter *ctx)
{
    /* Local variables for map operations */
    char key[16] = {0};     /* Process name buffer (initialized to zeros) */
    __u64 init = 1;         /* Initial count value for new entries */
    __u64 *val;             /* Pointer to existing count in map */

    /*
     * Step 1: Get the filename being executed
     * Read the filename from execve() arguments instead of current process name
     * ctx->args[0] contains the pointer to the filename string
     */
    const char *filename_ptr = (const char *)ctx->args[0];
    long ret = bpf_probe_read_user_str(key, sizeof(key), filename_ptr);

    /*
     * If reading the filename fails, fall back to current process name
     * This ensures we always have some identifier for the process
     */
    if (ret < 0) {
        bpf_get_current_comm(&key, sizeof(key));
    }

    /*
     * Step 2: Look up existing entry in the map
     * Returns pointer to value if key exists, NULL if not found
     */
    val = bpf_map_lookup_elem(&exec_cnt, &key);

    /*
     * Step 3: Update counter based on whether entry exists
     */
    if (val) {
        /*
         * Entry exists - increment the existing count
         * Atomic increment operation provides thread-safe counting
         * This prevents race conditions if multiple processes with same name
         * execute simultaneously
         */
        __sync_fetch_and_add(val, 1);
    } else {
        /*
         * New entry - create new map entry with initial count of 1
         * ANY flag allows both creation of new entries and updates
         */
        bpf_map_update_elem(&exec_cnt, &key, &init, BPF_ANY);
    }

    /*
     * Return 0 to indicate successful execution
     * eBPF programs must return 0 or negative error code
     */
    return 0;
}

