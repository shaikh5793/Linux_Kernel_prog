/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is licensed under GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

// Memory Leak Tracker - Track kmalloc/kfree to detect leaks
// Compiled for ARM without BTF

#include <linux/bpf.h>
#include <linux/ptrace.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

#define MAX_ENTRIES 10240

// Map to store allocated addresses and their sizes
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, MAX_ENTRIES);
    __type(key, u64);    // address
    __type(value, u64);  // size
} allocs SEC(".maps");

// Map to count total allocations and frees
struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 2);
    __type(key, u32);
    __type(value, u64);
} stats SEC(".maps");

// Hook kmalloc
SEC("kprobe/__kmalloc")
int BPF_KPROBE(trace_kmalloc, size_t size)
{
    return 0;
}

// Hook kmalloc return to get allocated address
SEC("kretprobe/__kmalloc")
int BPF_KRETPROBE(trace_kmalloc_ret, void *ret)
{
    u64 addr = (u64)ret;
    u64 size = 0; // We'll get size from args in real implementation
    
    if (addr == 0)
        return 0;
    
    // Store allocation
    bpf_map_update_elem(&allocs, &addr, &size, BPF_ANY);
    
    // Update stats
    u32 key = 0; // alloc counter
    u64 *count = bpf_map_lookup_elem(&stats, &key);
    if (count)
        __sync_fetch_and_add(count, 1);
    
    return 0;
}

// Hook kfree
SEC("kprobe/kfree")
int BPF_KPROBE(trace_kfree, void *ptr)
{
    u64 addr = (u64)ptr;
    
    if (addr == 0)
        return 0;
    
    // Remove from allocations map
    bpf_map_delete_elem(&allocs, &addr);
    
    // Update stats
    u32 key = 1; // free counter
    u64 *count = bpf_map_lookup_elem(&stats, &key);
    if (count)
        __sync_fetch_and_add(count, 1);
    
    return 0;
}

char _license[] SEC("license") = "GPL";
