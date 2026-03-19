/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is licensed under GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

// Lock Contention Analyzer - Track mutex lock/unlock timing
// For ARM without BTF

#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

#define MAX_ENTRIES 10240

// Store lock acquisition start time
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, MAX_ENTRIES);
    __type(key, u64);   // pid_tgid
    __type(value, u64); // start timestamp
} lock_start SEC(".maps");

// Store lock hold time histogram (in microseconds)
struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 32);
    __type(key, u32);
    __type(value, u64);
} hold_time_hist SEC(".maps");

// Lock contention stats
struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 4);
    __type(key, u32);
    __type(value, u64);
} lock_stats SEC(".maps");

// Trace mutex_lock entry
SEC("kprobe/mutex_lock")
int BPF_KPROBE(trace_mutex_lock_entry)
{
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u64 ts = bpf_ktime_get_ns();
    
    bpf_map_update_elem(&lock_start, &pid_tgid, &ts, BPF_ANY);
    
    // Count lock attempts
    u32 key = 0;
    u64 *count = bpf_map_lookup_elem(&lock_stats, &key);
    if (count)
        __sync_fetch_and_add(count, 1);
    
    return 0;
}

// Trace mutex_unlock
SEC("kprobe/mutex_unlock")
int BPF_KPROBE(trace_mutex_unlock)
{
    u64 pid_tgid = bpf_get_current_pid_tgid();
    u64 *start_ts = bpf_map_lookup_elem(&lock_start, &pid_tgid);
    
    if (!start_ts)
        return 0;
    
    u64 delta_ns = bpf_ktime_get_ns() - *start_ts;
    u64 delta_us = delta_ns / 1000;
    
    // Update histogram (buckets: 0-1us, 1-10us, 10-100us, etc.)
    u32 bucket = 0;
    if (delta_us < 1) bucket = 0;
    else if (delta_us < 10) bucket = 1;
    else if (delta_us < 100) bucket = 2;
    else if (delta_us < 1000) bucket = 3;
    else if (delta_us < 10000) bucket = 4;
    else bucket = 5;
    
    u64 *count = bpf_map_lookup_elem(&hold_time_hist, &bucket);
    if (count)
        __sync_fetch_and_add(count, 1);
    
    // Count successful unlocks
    u32 key = 1;
    u64 *stat_count = bpf_map_lookup_elem(&lock_stats, &key);
    if (stat_count)
        __sync_fetch_and_add(stat_count, 1);
    
    bpf_map_delete_elem(&lock_start, &pid_tgid);
    
    return 0;
}

char _license[] SEC("license") = "GPL";
