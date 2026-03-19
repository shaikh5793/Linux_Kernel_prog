/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is licensed under GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

// Function Latency Profiler - Measure kernel function execution time
// Uses kprobe/kretprobe without BTF

#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

#define MAX_ENTRIES 10240

// Store function entry time per thread
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, MAX_ENTRIES);
    __type(key, u64);   // pid_tgid
    __type(value, u64); // start timestamp
} func_start SEC(".maps");

// Store latency histogram (buckets in microseconds)
struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 64);  // 64 buckets
    __type(key, u32);
    __type(value, u64);
} latency_hist SEC(".maps");

// Stats: min, max, total, count
struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 4);
    __type(key, u32);
    __type(value, u64);
} stats SEC(".maps");

// Generic kprobe handler for function entry
SEC("kprobe/placeholder")
int trace_func_entry(struct pt_regs *ctx)
{
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u64 ts = bpf_ktime_get_ns();
    
    bpf_map_update_elem(&func_start, &pid_tgid, &ts, BPF_ANY);
    
    return 0;
}

// Generic kretprobe handler for function exit
SEC("kretprobe/placeholder")
int trace_func_exit(struct pt_regs *ctx)
{
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u64 *start_ts = bpf_map_lookup_elem(&func_start, &pid_tgid);
    
    if (!start_ts)
        return 0;
    
    u64 delta_ns = bpf_ktime_get_ns() - *start_ts;
    u64 delta_us = delta_ns / 1000;
    
    // Update histogram
    u32 bucket = delta_us / 10; // 10us buckets
    if (bucket >= 64)
        bucket = 63;
    
    u64 *count = bpf_map_lookup_elem(&latency_hist, &bucket);
    if (count)
        __sync_fetch_and_add(count, 1);
    
    // Update stats
    // Total count
    u32 stat_key = 0;
    u64 *stat_val = bpf_map_lookup_elem(&stats, &stat_key);
    if (stat_val)
        __sync_fetch_and_add(stat_val, 1);
    
    // Total time
    stat_key = 1;
    stat_val = bpf_map_lookup_elem(&stats, &stat_key);
    if (stat_val)
        __sync_fetch_and_add(stat_val, delta_ns);
    
    // Min latency
    stat_key = 2;
    stat_val = bpf_map_lookup_elem(&stats, &stat_key);
    if (stat_val && (*stat_val == 0 || delta_ns < *stat_val))
        *stat_val = delta_ns;
    
    // Max latency
    stat_key = 3;
    stat_val = bpf_map_lookup_elem(&stats, &stat_key);
    if (stat_val && delta_ns > *stat_val)
        *stat_val = delta_ns;
    
    bpf_map_delete_elem(&func_start, &pid_tgid);
    
    return 0;
}

char _license[] SEC("license") = "GPL";
