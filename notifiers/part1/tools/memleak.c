/*
 * memleak.c - Simple memory pressure trigger
 *
 * WARNING: This program aggressively allocates memory and can hang your system.
 *          Run only in a virtual machine or constrained cgroup.
 *
 * Usage:
 *   make            # in notifiers/part1/tools
 *   ./memleak       # allocate 1MB chunks indefinitely
 *
 * Safer usage (cgroup v2 example):
 *   mkdir -p /sys/fs/cgroup/oomtest
 *   echo $$ > /sys/fs/cgroup/oomtest/cgroup.procs
 *   echo 100M > /sys/fs/cgroup/oomtest/memory.max
 *   ./memleak
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    for (;;) {
        char *p = malloc(1024 * 1024); /* 1 MB */
        if (!p) {
            /* Allocation failed: back off slightly but keep pressure */
            continue;
        }
        memset(p, 0, 1024 * 1024); /* Touch pages to commit */
    }
    return 0;
}

