/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is licensed under GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

/*
 * User-Space Headers - Standard C library and BPF userspace APIs
 */
#include <signal.h>     /* Signal handling for graceful shutdown */
#include <stdio.h>      /* Standard I/O functions */
#include <stdlib.h>     /* Standard library functions */
#include <string.h>     /* String manipulation functions */
#include <unistd.h>     /* UNIX standard functions */

/*
 * BPF User-Space Libraries and Shared Definitions
 */
#include <bpf/libbpf.h>             /* BPF library for user space operations */
#include "ringbuf_exec.skel.h"      /* Auto-generated skeleton header */
#include "ringbuf_exec.h"           /* Shared event structure definition */

/*
 * Global Variables for Signal Handling
 * volatile sig_atomic_t ensures safe access from signal handlers
 */
static volatile sig_atomic_t exiting;

/*
 * Signal Handler - Clean Exit on Interrupt
 * Called when user presses Ctrl+C (SIGINT) or sends SIGTERM
 */
static void on_sig(int s)
{
    (void)s;        /* Suppress unused parameter warning */
    exiting = 1;    /* Set flag to exit main event loop */
}

/*
 * Event Handler Function - Process Incoming Ring Buffer Events
 * Purpose: Callback function triggered for each event received from kernel
 * Parameters:
 *   ctx  - User context (unused in this example)
 *   data - Pointer to the event data from ring buffer
 *   sz   - Size of the event data (unused in this example)
 * Returns: 0 for success, negative for error
 */
static int handle_event(void *ctx, void *data, size_t sz)
{
    (void)ctx;  /* Suppress unused parameter warnings */

    /*
     * Cast the raw data to our event structure
     * The kernel and user space share the same event structure definition
     */
    const struct event *e = data;

    /*
     * Display the process execution event
     * Precision specifier limits string length to prevent buffer overruns
     */
    printf("ringbuf exec: pid=%u comm=%.*s\n", e->pid,
           (int)sizeof(e->comm), e->comm);

    /*
     * Return success - continue processing more events
     * Returning non-zero would stop event processing
     */
    return 0;
}

/*
 * Main Program - Ring Buffer Event Consumer
 */
int main(void)
{
    /*
     * Program Management Variables
     */
    struct ringbuf_exec_bpf *skel = NULL;   /* BPF skeleton object */
    struct ring_buffer *rb = NULL;          /* Ring buffer consumer object */
    int err = 0;                            /* Error tracking */

    /*
     * Initialize libbpf in strict mode for better error checking
     * Strict mode helps catch common programming errors early
     */
    libbpf_set_strict_mode(LIBBPF_STRICT_ALL);

    /*
     * Signal Handler Setup - Enable graceful shutdown
     * Register handlers for common termination signals
     */
    signal(SIGINT, on_sig);     /* Ctrl+C */
    signal(SIGTERM, on_sig);    /* Termination request */

    /*
     * Phase 1: Open BPF Program
     * Load the BPF object file into memory and parse it
     * Does not yet load into kernel or verify the program
     */
    skel = ringbuf_exec_bpf__open();
    if (!skel) {
        fprintf(stderr, "open failed\n");
        return 1;
    }

    /*
     * Phase 2: Load BPF Program into Kernel
     * Verify the BPF program and load it into the kernel
     * Kernel performs safety checks and JIT compilation
     */
    if ((err = ringbuf_exec_bpf__load(skel))) {
        fprintf(stderr, "load failed: %d\n", err);
        goto out;
    }

    /*
     * Phase 3: Attach BPF Program to Hook Point
     * Connect the program to the sys_enter_execve tracepoint
     * From this point, the program will start generating events
     */
    if ((err = ringbuf_exec_bpf__attach(skel))) {
        fprintf(stderr, "attach failed: %d\n", err);
        goto out;
    }

    /*
     * Ring Buffer Consumer Setup
     * Create user-space ring buffer consumer that connects to kernel ring buffer
     * Register our event handler callback for processing incoming events
     * Parameters: map_fd, callback_function, callback_context, options
     */
    rb = ring_buffer__new(bpf_map__fd(skel->maps.events), handle_event,
                          NULL, NULL);
    if (!rb) {
        fprintf(stderr, "ring buffer setup failed\n");
        err = 1;
        goto out;
    }

    /*
     * Main Event Processing Loop
     * Continuously poll for new events and process them via callbacks
     */
    printf("ringbuf_exec running. Ctrl-C to exit.\n");
    while (!exiting) {
        /*
         * Poll for events with 100ms timeout
         * This blocks until events arrive or timeout occurs
         * When events are available, our handle_event callback is triggered
         * Returns: number of events processed, or negative on error
         */
        err = ring_buffer__poll(rb, 100 /* ms */);

        /*
         * Check for polling errors
         * Negative return indicates system-level error, not just timeout
         */
        if (err < 0) {
            fprintf(stderr, "poll error: %d\n", err);
            break;
        }
    }

    /*
     * Normal exit path - no errors occurred during event processing
     */
    err = 0;

/*
 * Cleanup Section - Proper Resource Management
 * Always reached via either normal exit or goto from error handling
 */
out:
    /*
     * Free ring buffer consumer resources
     * This stops event polling and releases associated memory
     */
    ring_buffer__free(rb);

    /*
     * Destroy BPF program and free all resources
     * This detaches the program and cleans up all associated resources
     */
    ringbuf_exec_bpf__destroy(skel);

    /*
     * Return appropriate exit code
     * 0 for success, non-zero for failure
     */
    return err != 0;
}
