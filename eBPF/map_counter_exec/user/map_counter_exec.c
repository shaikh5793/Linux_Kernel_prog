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
#include <errno.h>      /* Error codes and errno variable */
#include <signal.h>     /* Signal handling (SIGINT, SIGTERM) */
#include <stdio.h>      /* Standard I/O functions */
#include <string.h>     /* String manipulation functions */
#include <unistd.h>     /* UNIX standard functions (sleep) */

/*
 * BPF User-Space Libraries
 */
#include <bpf/bpf.h>                    /* BPF system call wrappers */
#include "map_counter_exec.skel.h"      /* Auto-generated skeleton header */

/*
 * Global Variables for Signal Handling
 * volatile sig_atomic_t ensures safe access from signal handlers
 */
static volatile sig_atomic_t exiting;

/*
 * Signal Handler - Clean Exit on Ctrl+C
 * Called when user presses Ctrl+C (SIGINT) or sends SIGTERM
 */
static void on_sig(int s) {
    (void)s;        /* Suppress unused parameter warning */
    exiting = 1;    /* Set flag to exit main loop */
}

/*
 * Map Reader Function - Print All Process Execution Counts
 * Purpose: Iterate through the BPF map and display all entries
 * Parameter: map_fd - File descriptor of the BPF map
 */
static void print_counts(int map_fd)
{
    /* Buffers for map iteration */
    char key[16] = {0};         /* Current key (process name) */
    char next_key[16] = {0};    /* Next key in iteration */
    __u64 val = 0;              /* Value (execution count) */

    /*
     * Map Iteration Process:
     * BPF hash maps don't support direct iteration like arrays.
     * We use the next key operation to traverse all entries.
     */
    memset(key, 0, sizeof(key));    /* Start with empty key */

    /*
     * Main iteration loop - continues until no more keys
     * Next key operation returns 0 on success, -1 when no more entries
     */
    while (bpf_map_get_next_key(map_fd, &key, &next_key) == 0) {
        /*
         * Look up the value for this key
         * Element lookup returns 0 on success, -1 if key not found
         */
        if (bpf_map_lookup_elem(map_fd, &next_key, &val) == 0) {
            /*
             * Print the entry: process_name: count
             * Precision specifier limits string length to prevent buffer overruns
             */
            printf("%.*s: %llu\n", (int)sizeof(next_key), next_key,
                   (unsigned long long)val);
        }

        /*
         * Advance to next key
         * Copy next_key to key for the next iteration
         */
        memcpy(key, next_key, sizeof(key));
    }
}

/*
 * Main Program - eBPF Program Management and Monitoring Loop
 */
int main(void)
{
    /*
     * BPF Program Management Variables
     */
    struct map_counter_exec_bpf *skel = NULL;   /* BPF skeleton object */
    int err = 0;                                /* Error tracking */

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
    skel = map_counter_exec_bpf__open();
    if (!skel) {
        fprintf(stderr, "open failed\n");
        return 1;
    }

    /*
     * Phase 2: Load BPF Program into Kernel
     * Verify the BPF program and load it into the kernel
     * Kernel performs safety checks and JIT compilation
     */
    if ((err = map_counter_exec_bpf__load(skel))) {
        fprintf(stderr, "load failed: %d\n", err);
        goto out;
    }

    /*
     * Phase 3: Attach BPF Program to Hook Point
     * Connect the program to the sys_enter_execve tracepoint
     * From this point, the program will start executing on every execve()
     */
    if ((err = map_counter_exec_bpf__attach(skel))) {
        fprintf(stderr, "attach failed: %d\n", err);
        goto out;
    }

    /*
     * Main Monitoring Loop
     * Continuously read and display the execution counts
     */
    printf("map_counter_exec running. Printing counts every 2s. Ctrl-C to exit.\n");
    while (!exiting) {
        /*
         * Wait 2 seconds between readings
         * This prevents overwhelming the console with output
         */
        sleep(2);

        /*
         * Read and display current map contents
         * Get the file descriptor for the exec_cnt map and read all entries
         */
        print_counts(bpf_map__fd(skel->maps.exec_cnt));

        /*
         * Print separator for readability
         */
        printf("----\n");
    }

/*
 * Cleanup Section - Proper Resource Management
 * Always reached via either normal exit or goto from error handling
 */
out:
    /*
     * Destroy BPF program and free resources
     * This detaches the program and cleans up all associated resources
     */
    map_counter_exec_bpf__destroy(skel);

    /*
     * Return appropriate exit code
     * 0 for success, non-zero for failure
     */
    return err != 0;
}

