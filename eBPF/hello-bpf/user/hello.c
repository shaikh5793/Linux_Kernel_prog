/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is licensed under GPL v2.
 * See the accompanying LICENSE file for the full text.
 *
 * Hello eBPF - User Space Program
 *
 * This program demonstrates the basic eBPF workflow:
 * 1. Load an eBPF program from a skeleton header
 * 2. Attach it to a kernel tracepoint
 * 3. Let it run and capture events
 * 4. Clean up resources on exit
 */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "hello.skel.h"  // Auto-generated skeleton header from hello.bpf.c

static volatile sig_atomic_t exiting = 0;

/*
 * Signal handler for graceful shutdown
 * Sets the exit flag when Ctrl+C is pressed
 */
static void handle_sigint(int signo) {
    (void)signo;  // Suppress unused parameter warning
    exiting = 1;
}

int main(void) {
    struct hello_bpf *skel = NULL;  // eBPF skeleton structure
    int err;

    /*
     * Set up signal handlers for graceful cleanup
     * This ensures we properly detach eBPF programs on exit
     */
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);

    /*
     * STEP 1: Open the eBPF object
     * This parses the embedded eBPF bytecode from the skeleton header
     * and prepares it for loading into the kernel
     */
    skel = hello_bpf__open();
    if (!skel) {
        fprintf(stderr, "Failed to open BPF skeleton\n");
        return 1;
    }

    /*
     * STEP 2: Load the eBPF program into the kernel
     * This verifies the eBPF bytecode and loads it into the kernel's
     * eBPF virtual machine for execution
     */
    err = hello_bpf__load(skel);
    if (err) {
        fprintf(stderr, "Failed to load BPF skeleton: %d\n", err);
        goto cleanup;
    }

    /*
     * STEP 3: Attach the eBPF program to its tracepoint
     * This connects our eBPF program to the sys_enter_execve tracepoint,
     * so it will be called whenever any process calls execve()
     */
    err = hello_bpf__attach(skel);
    if (err) {
        fprintf(stderr, "Failed to attach BPF programs: %d\n", err);
        goto cleanup;
    }

    printf("hello_bpf loaded. Press Ctrl-C to exit.\n");
    printf("Open another terminal and run: sudo cat /sys/kernel/debug/tracing/trace_pipe\n");
    printf("You will see execve syscalls with PID and process name.\n");
    printf("Try running commands like 'ls', 'ps', or 'date' to generate trace events.\n");

    /*
     * STEP 4: Keep the program running
     * The eBPF program executes in kernel space automatically.
     * We just need to keep this user-space program alive to maintain
     * the attachment to the tracepoint.
     */
    while (!exiting) {
        sleep(1);
    }

cleanup:
    /*
     * STEP 5: Clean up resources
     * This detaches the eBPF program from the tracepoint and
     * frees all associated resources
     */
    hello_bpf__destroy(skel);
    return err != 0;
}

