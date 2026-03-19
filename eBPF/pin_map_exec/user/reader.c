/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is licensed under GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

/*
 * User-Space Headers - Standard C library and BPF functions
 */
#include <errno.h>      /* Error codes and errno variable */
#include <stdio.h>      /* Standard I/O functions */
#include <string.h>     /* String manipulation functions */
#include <unistd.h>     /* UNIX standard functions (sleep) */

/*
 * BPF User-Space Library
 */
#include <bpf/bpf.h>    /* BPF system call wrappers for map operations */

/*
 * Map Reader Function - Display Pinned Map Contents
 * Purpose: Iterate through the pinned BPF map and display all entries
 * Parameter: map_fd - File descriptor of the pinned BPF map
 * Note: This function accesses the map without loading any eBPF program
 */
static void print_counts(int map_fd)
{
    /* Buffers for map iteration */
    char key[16] = {0};         /* Current key (executable filename) */
    char next_key[16] = {0};    /* Next key in iteration */
    __u64 val = 0;              /* Value (execution count) */

    /*
     * Initialize key buffer to start iteration from beginning
     * BPF map iteration requires starting with an empty/zero key
     */
    memset(key, 0, sizeof(key));

    /*
     * Map Iteration Process:
     * BPF hash maps don't support direct iteration like arrays
     * We use the next key operation to traverse all entries in the pinned map
     */
    while (bpf_map_get_next_key(map_fd, &key, &next_key) == 0) {
        /*
         * Look up the value for this key in the pinned map
         * Element lookup returns 0 on success, -1 if key not found
         */
        if (bpf_map_lookup_elem(map_fd, &next_key, &val) == 0) {
            /*
             * Display the executable and its execution count
             * Precision specifier limits string length to prevent buffer overruns
             * Now shows actual executable paths instead of just process names
             */
            printf("%.*s: %llu\n", (int)sizeof(next_key), next_key, (unsigned long long)val);
        }

        /*
         * Advance to next key for continued iteration
         * Copy next_key to key for the next iteration cycle
         */
        memcpy(key, next_key, sizeof(key));
    }
}

/*
 * Main Program - Pinned Map Reader and Monitor
 * Purpose: Independent reader that accesses pinned BPF map data
 * Architecture: This is a "consumer" that reads persisted data
 * Key Feature: No eBPF program loading required - pure map access
 */
int main(void)
{
    /*
     * Access the Pinned Map
     * Open the map that was pinned by the pinner program
     * This demonstrates cross-program BPF map access via filesystem
     */
    int map_fd = bpf_obj_get("/sys/fs/bpf/exec_cnt");
    if (map_fd < 0) {
        /*
         * Handle map access failure
         * Common causes: map not pinned yet, permission issues, bpffs not mounted
         */
        perror("bpf_obj_get /sys/fs/bpf/exec_cnt");
        printf("Make sure:\n");
        printf("1. The pinner program is running (or has run)\n");
        printf("2. BPF filesystem is mounted: sudo mount -t bpf bpf /sys/fs/bpf\n");
        printf("3. You have permission to access /sys/fs/bpf/exec_cnt\n");
        return 1;
    }

    /*
     * Status Message
     * Inform user that map access is successful and monitoring begins
     */
    printf("Successfully connected to pinned map /sys/fs/bpf/exec_cnt\n");
    printf("Displaying executable execution counts (updated every 2 seconds):\n");
    printf("==========================================\n");

    /*
     * Continuous Map Monitoring Loop
     * Repeatedly read and display the current contents of the pinned map
     * This shows real-time updates to the execution counters
     */
    while (1) {
        /*
         * Read and display all current map entries
         * Each call shows a snapshot of the current execution counts
         */
        print_counts(map_fd);

        /*
         * Print separator for readability between snapshots
         */
        printf("----\n");

        /*
         * Wait 2 seconds before next reading
         * This prevents overwhelming the console with output
         * Allows time for new executions to be counted
         */
        sleep(2);
    }

    /*
     * Note: This program runs indefinitely
     * Use Ctrl+C to exit when done monitoring
     * The pinned map remains accessible even after this program exits
     */
    return 0;
}

