/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 *
 * Usage: sudo ./test_vma
 * Requires: inspectvma module loaded
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define DEBUGFS_FILE "/sys/kernel/debug/inspectvma/vma_details"

/*
 * show_vma_details() - Read and display VMA information from kernel module
 * Called by: main() to demonstrate basic VMA inspection
 * Purpose: Opens debugfs file and prints all VMA data for current process
 */
static void show_vma_details()
{
    FILE *fp;
    char line[1024];
    
    printf("\n=== Current Process VMA Details ===\n");
    printf("Process PID: %d\n", getpid());
    printf("Process Name: ");
    system("cat /proc/self/comm");
    
    fp = fopen(DEBUGFS_FILE, "r");
    if (!fp) {
        printf("Error: Cannot read %s\n", DEBUGFS_FILE);
        printf("Make sure VMA inspector module is loaded and running as root\n");
        return;
    }
    
    // Read and display all VMA information
    while (fgets(line, sizeof(line), fp)) {
        printf("%s", line);
    }
    
    fclose(fp);
}

int main()
{
    // Check access to debugfs file
    if (access(DEBUGFS_FILE, R_OK) != 0) {
        printf("Error: Cannot access %s\n", DEBUGFS_FILE);
        return 1;
    }
    
    show_vma_details();
    return 0;
}
