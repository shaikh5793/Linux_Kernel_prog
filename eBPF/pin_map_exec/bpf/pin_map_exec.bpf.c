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
#include <bpf/bpf_helpers.h> /* BPF helper functions for map operations */
#include <bpf/bpf_tracing.h>  /* Tracing-related definitions and macros */

/*
 * Pinnable Hash Map Definition - Persistent Process Execution Counter
 * This map can be pinned to the filesystem for persistence and sharing
 * Key difference from regular maps: designed for pinning and cross-program access
 */
struct {
    __uint(type, BPF_MAP_TYPE_HASH);    /* Hash map for key-value storage */
    __type(key, char[16]);              /* Key: executable filename (16 bytes max) */
    __type(value, __u64);               /* Value: execution count (64-bit) */
    __uint(max_entries, 1024);          /* Maximum 1024 different executables */
} exec_cnt SEC(".maps");

/*
 * License Declaration - Required for eBPF programs
 * Must be GPL-compatible to access kernel tracing functions and map pinning
 */
char LICENSE[] SEC("license") = "GPL";

/*
 * Main eBPF Program - Persistent Executable Counter with Map Pinning
 * Attaches to: sys_enter_execve tracepoint
 * Triggers: Every time any process calls execve() system call
 * Purpose: Count executions by filename and store in a pinnable map
 * Data Persistence: Map can be pinned to filesystem for cross-program access
 */
SEC("tracepoint/syscalls/sys_enter_execve")
int handle_exec(struct trace_event_raw_sys_enter *ctx)
{
    /* Local variables for map operations */
    char key[16] = {0};     /* Executable filename buffer (initialized to zeros) */
    __u64 init = 1;         /* Initial count value for new entries */
    __u64 *val;             /* Pointer to existing count in map */

    /*
     * Step 1: Extract the executable filename from execve() arguments
     * Read the filename from execve() arguments instead of current process name
     * ctx->args[0] contains the pointer to the filename string in user space
     */
    const char *filename_ptr = (const char *)ctx->args[0];
    long ret = bpf_probe_read_user_str(key, sizeof(key), filename_ptr);

    /*
     * If reading the filename fails, fall back to current process name
     * This ensures we always have some identifier for the process
     * Fallback maintains robustness in case of user space access issues
     */
    if (ret < 0) {
        bpf_get_current_comm(&key, sizeof(key));
    }

    /*
     * Step 2: Look up existing entry in the pinnable map
     * Check if this executable has been seen before
     * Returns pointer to value if key exists, NULL if not found
     */
    val = bpf_map_lookup_elem(&exec_cnt, &key);

    /*
     * Step 3: Update counter based on whether entry exists
     * Use atomic operations to ensure thread safety in concurrent scenarios
     */
    if (val) {
        /*
         * Entry exists - increment the existing count atomically
         * Atomic increment prevents race conditions when multiple processes
         * with the same executable name execute simultaneously
         */
        __sync_fetch_and_add(val, 1);
    } else {
        /*
         * New entry - create new map entry with initial count of 1
         * ANY flag allows both creation of new entries and updates
         * This data will persist in the pinned map even after program exit
         */
        bpf_map_update_elem(&exec_cnt, &key, &init, BPF_ANY);
    }

    /*
     * Return success code
     * Data has been successfully stored in the pinnable map
     * Map can now be accessed by other programs via pinning
     */
    return 0;
}

