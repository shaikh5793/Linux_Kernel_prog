/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is licensed under GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

/*
 * Syscall Tracer - User space program
 *
 * This program loads and manages the eBPF syscall tracing program.
 * It demonstrates:
 * - Loading multiple eBPF programs from one object
 * - Attaching to multiple tracepoints
 * - Graceful shutdown and cleanup
 * - User-friendly output and instructions
 */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "trace_syscalls.skel.h"

/* Global variable for the eBPF skeleton */
static struct trace_syscalls_bpf *skel = NULL;
static volatile sig_atomic_t exiting = 0;

/*
 * Signal handler for graceful shutdown
 */
static void cleanup_and_exit(int sig) {
    (void)sig;  // Suppress unused parameter warning
    exiting = 1;
    
    if (skel) {
        printf("\n🧹 Cleaning up eBPF programs...\n");
        trace_syscalls_bpf__destroy(skel);
        skel = NULL;
    }
    
    printf("👋 Syscall tracer stopped. Goodbye!\n");
    exit(0);
}

/*
 * Print current timestamp
 */
static void print_timestamp(void) {
    time_t now;
    struct tm *tm_info;
    char timestamp[20];
    
    time(&now);
    tm_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%H:%M:%S", tm_info);
    printf("[%s] ", timestamp);
}

/*
 * Load and attach the eBPF programs
 */
static int load_bpf_programs(void) {
    int err;
    
    printf("🔧 Loading syscall tracer eBPF programs...\n");
    
    /*
     * Step 1: Open the eBPF object
     *
     * This function:
     * - Parses the embedded eBPF bytecode from the skeleton header
     * - Sets up the eBPF object structure with program and map metadata
     * - Prepares multiple eBPF programs (trace_open, trace_read, trace_write)
     * - Does NOT load anything into the kernel yet
     */
    skel = trace_syscalls_bpf__open();
    if (!skel) {
        fprintf(stderr, "❌ Failed to open eBPF skeleton: %s\n", strerror(errno));
        return 1;
    }

    /*
     * Step 2: Load the eBPF programs into the kernel
     *
     * This function:
     * - Verifies the eBPF bytecode passes the kernel verifier
     * - Loads all eBPF programs into the kernel's eBPF virtual machine
     * - Creates any eBPF maps defined in the programs
     * - Programs are loaded but not yet attached to any hook points
     */
    err = trace_syscalls_bpf__load(skel);
    if (err) {
        fprintf(stderr, "❌ Failed to load eBPF programs: %d\n", err);
        goto cleanup;
    }

    /*
     * Step 3: Attach the eBPF programs to their tracepoints
     *
     * This function:
     * - Attaches trace_open to sys_enter_openat tracepoint
     * - Attaches trace_read to sys_enter_read tracepoint
     * - Attaches trace_write to sys_enter_write tracepoint
     * - Programs will now execute whenever these syscalls are invoked
     * - Creates file descriptors that represent the attachments
     */
    err = trace_syscalls_bpf__attach(skel);
    if (err) {
        fprintf(stderr, "❌ Failed to attach eBPF programs: %d\n", err);
        goto cleanup;
    }
    
    printf("✅ Syscall tracer loaded and attached successfully!\n");
    printf("📊 Tracing: openat(), read(), write() system calls\n");
    return 0;
    
cleanup:
    /*
     * Cleanup function:
     * - Detaches all eBPF programs from their tracepoints
     * - Unloads eBPF programs from the kernel
     * - Frees all associated resources and file descriptors
     */
    trace_syscalls_bpf__destroy(skel);
    skel = NULL;
    return err;
}

int main(void) {
    int err;
    
    printf("========================================\n");
    printf("🕵️  eBPF Syscall Tracer\n");
    printf("========================================\n");
    printf("This program traces open(), read(), and write() system calls.\n");
    printf("It will show PID, process name, and syscall arguments.\n\n");
    
    /*
     * Set up signal handlers for graceful shutdown
     */
    signal(SIGINT, cleanup_and_exit);
    signal(SIGTERM, cleanup_and_exit);
    
    /*
     * Load and attach the eBPF programs
     */
    err = load_bpf_programs();
    if (err) {
        fprintf(stderr, "❌ Failed to load eBPF programs\n");
        return err;
    }
    
    /*
     * Print instructions for the user
     */
    printf("\n📖 Instructions:\n");
    printf("1. Open another terminal window\n");
    printf("2. Run: sudo cat /sys/kernel/tracing/trace_pipe\n");
    printf("3. In a third terminal, run some file operations like:\n");
    printf("   • ls /tmp\n");
    printf("   • cat /etc/passwd\n");
    printf("   • echo 'test' > /tmp/testfile\n");
    printf("   • cat /tmp/testfile\n");
    printf("4. Watch the syscall traces in the second terminal\n");
    printf("5. Press Ctrl+C here to stop tracing\n\n");
    
    print_timestamp();
    printf("🎯 Syscall tracer is running... (Press Ctrl+C to exit)\n\n");
    
    /*
     * Main loop - keep the program running
     *
     * Key points:
     * - The eBPF programs execute in kernel space automatically
     * - They're triggered by syscall tracepoints, not by this loop
     * - This user-space program just needs to stay alive to maintain
     *   the attachment between eBPF programs and kernel tracepoints
     * - Output appears in /sys/kernel/tracing/trace_pipe, not here
     */
    while (!exiting) {
        sleep(1);

        /* Optional: Print a periodic status message */
        static int counter = 0;
        if (++counter % 30 == 0) {  // Every 30 seconds
            print_timestamp();
            printf("📊 Still tracing syscalls... (%d seconds elapsed)\n", counter);
        }
    }
    
    /*
     * Cleanup is handled by the signal handler
     */
    return 0;
}
