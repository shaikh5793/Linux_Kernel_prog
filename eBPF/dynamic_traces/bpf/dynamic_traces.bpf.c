/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is licensed under GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

/* Event data structure sent to user-space for each traced system call */
struct trace_data {
    __u32 pid;
    __u32 stack_id;
    char comm[16];
    char function[32];
};

/*
 * Stack trace map - stores kernel stack traces
 * Key: stack_id (returned by bpf_get_stackid)
 * Value: array of up to 127 kernel addresses representing the call stack
 */
struct {
    __uint(type, BPF_MAP_TYPE_STACK_TRACE);
    __uint(key_size, sizeof(__u32));
    __uint(value_size, 127 * sizeof(__u64));
    __uint(max_entries, 16384);
} stacks SEC(".maps");

/* Ring buffer for sending events to user-space efficiently */
struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 256 * 1024);
} events SEC(".maps");

char LICENSE[] SEC("license") = "GPL";

/*
 * Global sampling counter - shared across all kprobes to reduce event frequency.
 * Without sampling, system calls are too frequent and overwhelm the output.
 */
static __u64 sample_count = 0;

/*
 * Hook file opening syscalls - captures different paths through VFS layer
 * Different file types (regular, socket, pipe) and filesystems create varied stack traces
 */
SEC("kprobe/__x64_sys_openat")
int trace_sys_openat(struct pt_regs *ctx) {
    struct trace_data *event;

    /* Sample only every 10th call to reduce noise */
    __sync_fetch_and_add(&sample_count, 1);
    if (sample_count % 10 != 0) return 0;

    /* Reserve space in ring buffer for event data */
    event = bpf_ringbuf_reserve(&events, sizeof(*event), 0);
    if (!event) return 0;

    event->pid = bpf_get_current_pid_tgid() >> 32;
    /* Capture kernel stack trace - this is the key value we're analyzing */
    event->stack_id = bpf_get_stackid(ctx, &stacks, BPF_F_FAST_STACK_CMP);
    bpf_get_current_comm(&event->comm, sizeof(event->comm));
    __builtin_memcpy(event->function, "sys_openat", 11);

    bpf_ringbuf_submit(event, 0);
    return 0;
}

/*
 * Hook read syscalls - most frequent, shows varied I/O stack traces
 * Different sources: disk, cache, network sockets, pipes create different paths
 */
SEC("kprobe/__x64_sys_read")
int trace_sys_read(struct pt_regs *ctx) {
    struct trace_data *event;

    /* Higher sampling rate (1/20) due to read frequency */
    __sync_fetch_and_add(&sample_count, 1);
    if (sample_count % 20 != 0) return 0;

    event = bpf_ringbuf_reserve(&events, sizeof(*event), 0);
    if (!event) return 0;

    event->pid = bpf_get_current_pid_tgid() >> 32;
    event->stack_id = bpf_get_stackid(ctx, &stacks, BPF_F_FAST_STACK_CMP);
    bpf_get_current_comm(&event->comm, sizeof(event->comm));
    __builtin_memcpy(event->function, "sys_read", 9);

    bpf_ringbuf_submit(event, 0);
    return 0;
}

/*
 * Hook write syscalls - shows output path diversity
 * Terminal, files, sockets, pipes all have different kernel paths
 */
SEC("kprobe/__x64_sys_write")
int trace_sys_write(struct pt_regs *ctx) {
    struct trace_data *event;

    __sync_fetch_and_add(&sample_count, 1);
    if (sample_count % 15 != 0) return 0;

    event = bpf_ringbuf_reserve(&events, sizeof(*event), 0);
    if (!event) return 0;

    event->pid = bpf_get_current_pid_tgid() >> 32;
    event->stack_id = bpf_get_stackid(ctx, &stacks, BPF_F_FAST_STACK_CMP);
    bpf_get_current_comm(&event->comm, sizeof(event->comm));
    __builtin_memcpy(event->function, "sys_write", 10);

    bpf_ringbuf_submit(event, 0);
    return 0;
}

/*
 * Hook close syscalls - resource cleanup paths
 * Different fd types (files, sockets, pipes) show different cleanup stacks
 */
SEC("kprobe/__x64_sys_close")
int trace_sys_close(struct pt_regs *ctx) {
    struct trace_data *event;

    /* Lower sampling rate since close is less frequent but still common */
    __sync_fetch_and_add(&sample_count, 1);
    if (sample_count % 5 != 0) return 0;

    event = bpf_ringbuf_reserve(&events, sizeof(*event), 0);
    if (!event) return 0;

    event->pid = bpf_get_current_pid_tgid() >> 32;
    event->stack_id = bpf_get_stackid(ctx, &stacks, BPF_F_FAST_STACK_CMP);
    bpf_get_current_comm(&event->comm, sizeof(event->comm));
    __builtin_memcpy(event->function, "sys_close", 10);

    bpf_ringbuf_submit(event, 0);
    return 0;
}
