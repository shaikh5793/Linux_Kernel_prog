/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is licensed under GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

/*
 * User-Space Headers - Standard C library and system functions
 */
#include <errno.h>      /* Error codes and errno variable */
#include <signal.h>     /* Signal handling for graceful shutdown */
#include <stdio.h>      /* Standard I/O functions */
#include <stdlib.h>     /* Standard library functions */
#include <string.h>     /* String manipulation functions */
#include <sys/stat.h>   /* File and directory status functions */
#include <unistd.h>     /* UNIX standard functions (sleep) */

/*
 * BPF User-Space Libraries
 */
#include <bpf/bpf.h>            /* BPF system call wrappers and map operations */
#include <bpf/libbpf.h>         /* BPF library for user space operations */

/*
 * Generated BPF Program Interface
 */
#include "pin_map_exec.skel.h"  /* Auto-generated skeleton header */

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
 * BPF Filesystem Verification Function
 * Purpose: Ensure that the BPF filesystem is mounted and accessible
 * Required: Map pinning requires /sys/fs/bpf to be mounted as bpffs
 * Returns: 0 on success, -1 if bpffs is not available
 */
static int ensure_bpffs(void)
{
    struct stat st;

    /*
     * Check if /sys/fs/bpf exists and is a directory
     * This is where BPF objects (maps, programs) can be pinned
     */
    if (stat("/sys/fs/bpf", &st) == 0 && S_ISDIR(st.st_mode)) {
        return 0;   /* BPF filesystem is available */
    }

    /*
     * BPF filesystem not found - provide helpful error message
     * User needs to mount bpffs before pinning can work
     */
    fprintf(stderr, "/sys/fs/bpf not found; please mount bpffs: sudo mount -t bpf bpf /sys/fs/bpf\n");
    return -1;
}

/*
 * Main Program - BPF Map Pinner and Data Collector
 * Purpose: Load eBPF program, pin its map, and maintain data collection
 * Architecture: This is the "producer" that collects and persists data
 */
int main(void)
{
    /*
     * Program Management Variables
     */
    struct pin_map_exec_bpf *skel = NULL;   /* BPF skeleton object */
    int err = 0;                            /* Error tracking */

    /*
     * Prerequisite Check: Verify BPF Filesystem Availability
     * Map pinning requires bpffs to be mounted
     */
    if (ensure_bpffs()) {
        return 1;
    }

    /*
     * Initialize libbpf in strict mode for better error checking
     * Strict mode helps catch common programming errors early
     */
    libbpf_set_strict_mode(LIBBPF_STRICT_ALL);

    /*
     * Signal Handler Setup - Enable graceful shutdown
     * Important: Proper cleanup ensures pinned map remains accessible
     */
    signal(SIGINT, on_sig);     /* Ctrl+C */
    signal(SIGTERM, on_sig);    /* Termination request */

    /*
     * Phase 1: Open BPF Program
     * Load the BPF object file into memory and parse it
     * Prepares the program and maps for loading into kernel
     */
    skel = pin_map_exec_bpf__open();
    if (!skel) {
        fprintf(stderr, "open failed\n");
        return 1;
    }

    /*
     * Phase 2: Load BPF Program into Kernel
     * Verify the BPF program and load it into the kernel
     * Creates the map in kernel memory (not yet pinned)
     */
    if ((err = pin_map_exec_bpf__load(skel))) {
        fprintf(stderr, "load failed: %d\n", err);
        goto out;
    }

    /*
     * Phase 3: Attach BPF Program to Hook Point
     * Connect the program to the sys_enter_execve tracepoint
     * From this point, the program starts collecting execution data
     */
    if ((err = pin_map_exec_bpf__attach(skel))) {
        fprintf(stderr, "attach failed: %d\n", err);
        goto out;
    }

    /*
     * Phase 4: Pin the Map for Persistence and Sharing
     * This is the key operation that enables cross-program access
     */

    /*
     * Step 4a: Get the map file descriptor
     * Extract the file descriptor for the exec_cnt map from the skeleton
     */
    int map_fd = bpf_map__fd(skel->maps.exec_cnt);
    if (map_fd < 0) {
        fprintf(stderr, "map fd error\n");
        err = 1;
        goto out;
    }

    /*
     * Step 4b: Pin the map to the filesystem
     * Make the map accessible at /sys/fs/bpf/exec_cnt
     * This enables other programs to access the same map data
     */
    if ((err = bpf_obj_pin(map_fd, "/sys/fs/bpf/exec_cnt"))) {
        /*
         * Handle the case where map is already pinned
         * This allows restarting the pinner without conflicts
         */
        if (errno == EEXIST) {
            /* Map already exists - this is okay, continue */
            err = 0;
        } else {
            /* Real error occurred during pinning */
            perror("bpf_obj_pin");
            goto out;
        }
    }

    /*
     * Status Message and Main Collection Loop
     * Inform user that data collection is active and map is accessible
     */
    printf("pinner running. Map pinned at /sys/fs/bpf/exec_cnt. Ctrl-C to exit.\n");
    printf("Other programs can now access the map via: bpf_obj_get(\"/sys/fs/bpf/exec_cnt\")\n");

    /*
     * Main Data Collection Loop
     * Keep the program running to maintain eBPF program attachment
     * The eBPF program runs in kernel space and updates the pinned map
     */
    while (!exiting) {
        /*
         * Sleep to reduce CPU usage
         * The actual data collection happens in kernel space
         * This loop just keeps the user program alive
         */
        sleep(1);
    }

/*
 * Cleanup Section - Proper Resource Management
 * Always reached via either normal exit or goto from error handling
 */
out:
    /*
     * Destroy BPF program and free resources
     * Note: This detaches the eBPF program but leaves the map pinned
     * The pinned map remains accessible even after this program exits
     */
    pin_map_exec_bpf__destroy(skel);

    /*
     * Return appropriate exit code
     * 0 for success, non-zero for failure
     */
    return err != 0;
}

