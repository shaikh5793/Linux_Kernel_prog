/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is licensed under GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <errno.h>

#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include "dynamic_traces.skel.h"

/* Must match the BPF program's trace_data structure */
struct trace_data {
    __u32 pid;
    __u32 stack_id;
    char comm[16];
    char function[32];
};

/* Global state for signal handling and output management */
static volatile bool stop = false;
static FILE *output_file = NULL;
static int event_count = 0;
static time_t last_summary = 0;

static void sig_handler(int sig) {
    stop = true;
}

/*
 * Ring buffer callback - processes each event from BPF program
 * This is called for every syscall event that passes the sampling filter
 */
static int handle_event(void *ctx, void *data, size_t data_sz) {
    const struct trace_data *event = data;
    __u64 stack_trace[127];
    struct dynamic_traces_bpf *skel = ctx;
    time_t now = time(NULL);

    event_count++;

    FILE *out = output_file ? output_file : stdout;

    fprintf(out, "[%ld] Function: %-12s PID: %-6u Process: %-10s Stack ID: %u\n",
           now, event->function, event->pid, event->comm, event->stack_id);

    /*
     * Resolve stack ID to actual kernel addresses
     * Each stack_id maps to a unique sequence of kernel function calls
     */
    if (bpf_map_lookup_elem(bpf_map__fd(skel->maps.stacks), &event->stack_id, stack_trace) == 0) {
        fprintf(out, "  Stack trace:\n");
        /* Show top 5 frames - these are kernel function addresses */
        for (int i = 0; i < 5 && stack_trace[i] != 0; i++) {
            fprintf(out, "    [%d] 0x%llx\n", i, (unsigned long long)stack_trace[i]);
        }
    }
    fprintf(out, "\n");

    fflush(out);

    /* Progress indicator when writing to file */
    if (now - last_summary >= 10) {
        printf("Events captured: %d\n", event_count);
        last_summary = now;
    }

    return 0;
}

int main(int argc, char **argv) {
    struct dynamic_traces_bpf *skel;
    struct ring_buffer *rb = NULL;
    int err;

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    /*
     * Handle output file argument
     * If just filename given, use /tmp for sudo compatibility
     */
    if (argc > 1) {
        char filepath[256];
        if (argv[1][0] != '/') {
            snprintf(filepath, sizeof(filepath), "/tmp/%s", argv[1]);
        } else {
            strncpy(filepath, argv[1], sizeof(filepath) - 1);
            filepath[sizeof(filepath) - 1] = '\0';
        }

        output_file = fopen(filepath, "w");
        if (!output_file) {
            fprintf(stderr, "Failed to open output file %s: %s\n", filepath, strerror(errno));
            return 1;
        }
        printf("Writing output to %s\n", filepath);
    }

    /* Load BPF program from compiled skeleton */
    skel = dynamic_traces_bpf__open();
    if (!skel) {
        fprintf(stderr, "Failed to open BPF skeleton\n");
        return 1;
    }

    /* Load BPF program into kernel */
    err = dynamic_traces_bpf__load(skel);
    if (err) {
        fprintf(stderr, "Failed to load BPF skeleton: %d\n", err);
        goto cleanup;
    }

    /* Attach kprobes to kernel functions */
    err = dynamic_traces_bpf__attach(skel);
    if (err) {
        fprintf(stderr, "Failed to attach BPF skeleton: %d\n", err);
        goto cleanup;
    }

    /*
     * Set up ring buffer for receiving events from BPF program
     * This provides efficient communication between kernel and user-space
     */
    rb = ring_buffer__new(bpf_map__fd(skel->maps.events), handle_event, skel, NULL);
    if (!rb) {
        err = -1;
        fprintf(stderr, "Failed to create ring buffer\n");
        goto cleanup;
    }

    printf("Dynamic stack tracing started. Press Ctrl-C to exit.\n");
    printf("Tracing: sys_openat, sys_read, sys_write, sys_close\n");
    printf("Sampling rates: openat(1/10), read(1/20), write(1/15), close(1/5)\n");
    if (output_file) {
        printf("Output being written to file. Progress shown every 10 seconds.\n");
    }
    printf("\n");

    last_summary = time(NULL);

    /*
     * Main event loop - poll ring buffer for events from BPF program
     * Each poll processes any pending events and calls handle_event()
     */
    while (!stop) {
        err = ring_buffer__poll(rb, 100 /* timeout, ms */);
        if (err == -EINTR) {
            err = 0;
            break;
        }
        if (err < 0) {
            printf("Error polling ring buffer: %d\n", err);
            break;
        }
    }

cleanup:
    if (output_file) {
        fclose(output_file);
        printf("Final count: %d events captured\n", event_count);
    }
    ring_buffer__free(rb);
    dynamic_traces_bpf__destroy(skel);
    return err < 0 ? -err : 0;
}