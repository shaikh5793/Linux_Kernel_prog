/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is licensed under GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

/*
 * User-Space Headers - Standard C library functions
 */
#include <signal.h>     /* Signal handling for graceful shutdown */
#include <stdio.h>      /* Standard I/O functions */
#include <unistd.h>     /* UNIX standard functions (sleep) */

/*
 * BPF Program Interface
 */
#include "core_task_read.skel.h"    /* Auto-generated skeleton header */

/*
 * Global Variables for Signal Handling
 * volatile sig_atomic_t ensures safe access from signal handlers
 */
static volatile sig_atomic_t exiting;

/*
 * Signal Handler - Clean Exit on Interrupt
 * Called when user presses Ctrl+C (SIGINT) or sends SIGTERM
 */
static void on_sig(int s) {
    (void)s;        /* Suppress unused parameter warning */
    exiting = 1;    /* Set flag to exit main loop */
}

/*
 * Main Program - CO-RE Task Reader Launcher
 * Purpose: Simple launcher for the CO-RE kernel structure reading demo
 * Note: This program doesn't process events - output goes to trace_pipe
 */
int main(void)
{
    /*
     * BPF Program Management Variables
     */
    struct core_task_read_bpf *skel = NULL;     /* BPF skeleton object */
    int err = 0;                                /* Error tracking */

    /*
     * Phase 1: Open BPF Program
     * Load the BPF object file into memory and parse it
     * CO-RE relocations are prepared but not yet applied
     */
    skel = core_task_read_bpf__open();
    if (!skel) {
        fprintf(stderr, "open failed\n");
        return 1;
    }

    /*
     * Phase 2: Load BPF Program into Kernel
     * Verify the BPF program and load it into the kernel
     * CO-RE relocations are applied here for current kernel version
     * Kernel performs safety checks and JIT compilation
     */
    if ((err = core_task_read_bpf__load(skel))) {
        fprintf(stderr, "load failed: %d\n", err);
        goto out;
    }

    /*
     * Phase 3: Attach BPF Program to Hook Point
     * Connect the program to the sys_enter_execve tracepoint
     * From this point, the program starts reading kernel structures
     */
    if ((err = core_task_read_bpf__attach(skel))) {
        fprintf(stderr, "attach failed: %d\n", err);
        goto out;
    }

    /*
     * Signal Handler Setup - Enable graceful shutdown
     * Register handlers for common termination signals
     */
    signal(SIGINT, on_sig);     /* Ctrl+C */
    signal(SIGTERM, on_sig);    /* Termination request */

    /*
     * User Instructions and Program Status
     * Inform user how to see the output and how to exit
     */
    printf("core_task_read running. Ctrl-C to exit.\n");
    printf("View output with: sudo cat /sys/kernel/debug/tracing/trace_pipe\n");

    /*
     * Main Loop - Keep Program Running
     * The eBPF program runs in kernel space and outputs to trace_pipe
     * This loop just keeps the user program alive to maintain attachment
     */
    while (!exiting) {
        /*
         * Sleep for 1 second between loop iterations
         * No active processing needed - eBPF program handles everything
         */
        sleep(1);
    }

/*
 * Cleanup Section - Proper Resource Management
 * Always reached via either normal exit or goto from error handling
 */
out:
    /*
     * Destroy BPF program and free all resources
     * This detaches the program and cleans up CO-RE relocations
     * All kernel structure access stops here
     */
    core_task_read_bpf__destroy(skel);

    /*
     * Return appropriate exit code
     * 0 for success, non-zero for failure
     */
    return err != 0;
}

