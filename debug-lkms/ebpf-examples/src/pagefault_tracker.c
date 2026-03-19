/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is licensed under GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

// Page Fault Tracker - Monitor page faults and memory access
// For ARM VExpress without BTF

#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>

#define MAX_ENTRIES 1024

// Count page faults by type
struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 4);
    __type(key, u32);
    __type(value, u64);
} pf_stats SEC(".maps");

// Page fault address histogram (by page)
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, MAX_ENTRIES);
    __type(key, u64);   // page address
    __type(value, u64); // count
} pf_addrs SEC(".maps");

SEC("tracepoint/exceptions/page_fault_user")
int trace_page_fault_user(void *ctx)
{
    // User page fault
    u32 key = 0;
    u64 *count = bpf_map_lookup_elem(&pf_stats, &key);
    if (count)
        __sync_fetch_and_add(count, 1);
    else {
        u64 init_val = 1;
        bpf_map_update_elem(&pf_stats, &key, &init_val, BPF_ANY);
    }
    
    return 0;
}

SEC("tracepoint/exceptions/page_fault_kernel")
int trace_page_fault_kernel(void *ctx)
{
    // Kernel page fault
    u32 key = 1;
    u64 *count = bpf_map_lookup_elem(&pf_stats, &key);
    if (count)
        __sync_fetch_and_add(count, 1);
    else {
        u64 init_val = 1;
        bpf_map_update_elem(&pf_stats, &key, &init_val, BPF_ANY);
    }
    
    return 0;
}

char _license[] SEC("license") = "GPL";
