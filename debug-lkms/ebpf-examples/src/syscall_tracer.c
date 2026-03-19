/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is licensed under GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

// Syscall Tracer - Trace system calls with timing
// Simple version without BTF for ARM

#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

#define MAX_ENTRIES 10240

// Store syscall entry time
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, MAX_ENTRIES);
    __type(key, u64);   // pid_tgid
    __type(value, u64); // timestamp
} syscall_start SEC(".maps");

// Store syscall latency histogram
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 512);
    __type(key, u32);   // syscall_nr
    __type(value, u64); // total latency
} syscall_latency SEC(".maps");

// Store syscall count
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 512);
    __type(key, u32);   // syscall_nr
    __type(value, u64); // count
} syscall_count SEC(".maps");

// Trace syscall entry
SEC("tracepoint/raw_syscalls/sys_enter")
int trace_sys_enter(struct trace_event_raw_sys_enter *ctx)
{
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u64 ts = bpf_ktime_get_ns();
    
    bpf_map_update_elem(&syscall_start, &pid_tgid, &ts, BPF_ANY);
    
    // Count syscall
    u32 syscall_nr = (u32)ctx->id;
    u64 *count = bpf_map_lookup_elem(&syscall_count, &syscall_nr);
    if (count)
        __sync_fetch_and_add(count, 1);
    else {
        u64 init_val = 1;
        bpf_map_update_elem(&syscall_count, &syscall_nr, &init_val, BPF_ANY);
    }
    
    return 0;
}

// Trace syscall exit
SEC("tracepoint/raw_syscalls/sys_exit")
int trace_sys_exit(struct trace_event_raw_sys_exit *ctx)
{
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u64 *start_ts = bpf_map_lookup_elem(&syscall_start, &pid_tgid);
    
    if (!start_ts)
        return 0;
    
    u64 delta = bpf_ktime_get_ns() - *start_ts;
    u32 syscall_nr = (u32)ctx->id;
    
    // Update latency
    u64 *latency = bpf_map_lookup_elem(&syscall_latency, &syscall_nr);
    if (latency)
        __sync_fetch_and_add(latency, delta);
    else
        bpf_map_update_elem(&syscall_latency, &syscall_nr, &delta, BPF_ANY);
    
    bpf_map_delete_elem(&syscall_start, &pid_tgid);
    
    return 0;
}

char _license[] SEC("license") = "GPL";
