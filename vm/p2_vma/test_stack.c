/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 *
 * Usage: sudo ./test_stack
 * Requires: inspectvma module loaded
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define DEBUGFS_FILE "/sys/kernel/debug/inspectvma/vma_details"

/*
 * show_vma_details() - Display filtered VMA info focusing on stack-related data
 * Called by: main() and recursive_function() to show stack changes
 * Purpose: Shows only stack VMA and summary information at different execution points
 */
static void show_vma_details(const char *context)
{
    FILE *fp;
    char line[1024];
    
    printf("\n=== VMA Details %s ===\n", context);
    
    fp = fopen(DEBUGFS_FILE, "r");
    if (!fp) {
        printf("Error: Cannot read %s\n", DEBUGFS_FILE);
        return;
    }
    
    /* Show only stack-related information and summary */
    int show_line = 0;
    while (fgets(line, sizeof(line), fp)) {
        /* Show header information */
        if (strstr(line, "VMA Inspector") || 
            strstr(line, "Memory Layout Overview") ||
            strstr(line, "Stack Start:") ||
            strstr(line, "Summary:") ||
            strstr(line, "Total VMAs:") ||
            strstr(line, "RSS")) {
            printf("%s", line);
            show_line = 1;
            continue;
        }
        
        /* Show stack VMA specifically */
        if (strstr(line, "[stack]")) {
            printf("STACK VMA: %s", line);
        }
        
        /* Show separator lines */
        if (strstr(line, "===") || strstr(line, "---")) {
            printf("%s", line);
        }
    }
    
    fclose(fp);
}

/*
 * show_current_rss() - Display current RSS to show actual memory usage changes
 * Called by: main() to demonstrate that RSS changes even when VMA size doesn't
 * Purpose: Shows physical memory usage changes that VMA size doesn't reflect
 */
static void show_current_rss(const char *context)
{
    FILE *fp = fopen("/proc/self/status", "r");
    char line[256];
    
    if (!fp) {
        printf("Cannot read /proc/self/status\n");
        return;
    }
    
    printf("\n--- RSS Analysis %s ---\n", context);
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "VmRSS:") || strstr(line, "VmHWM:") || 
            strstr(line, "VmStk:") || strstr(line, "VmSize:")) {
            printf("%s", line);
        }
    }
    fclose(fp);
}

/*
 * show_stack_info() - Display stack variable address to show stack pointer movement
 * Called by: main() and recursive_function() to demonstrate stack growth
 * Purpose: Shows how stack addresses change with recursion depth
 */
static void show_stack_info(const char *context, int depth)
{
    char local_var = 'X';
    printf("%s - Depth %d: Stack variable at %p\n", context, depth, &local_var);
}

/*
 * recursive_function() - Recursively allocate stack space to demonstrate stack growth
 * Called by: main() and itself (recursion) to grow stack usage
 * Purpose: Each call allocates 4KB on stack to force stack VMA expansion
 */
static void recursive_function(int depth, int max_depth)
{
    /* Large local array to consume stack space */
    char stack_buffer[4096];  /* 4KB per recursion level */
    
    /* Initialize buffer to ensure it's allocated */
    memset(stack_buffer, depth % 256, sizeof(stack_buffer));
    
    show_stack_info("Recursive call", depth);
    
    if (depth >= max_depth) {
        printf("\nReached maximum recursion depth: %d\n", depth);
        show_vma_details("(At Maximum Stack Depth)");
        show_current_rss("(At Maximum Stack Depth)");
        printf("\nHit any key to start unwinding the recursion...");
        getchar();
        return;
    }
    
    /* Continue recursing */
    recursive_function(depth + 1, max_depth);
}

int main()
{
    printf("Process PID: %d\n", getpid());
    
    /* Check access to debugfs file */
    if (access(DEBUGFS_FILE, R_OK) != 0) {
        printf("Error: Cannot access %s\n", DEBUGFS_FILE);
        printf("Make sure module is loaded and running as root\n");
        return 1;
    }
    
    /* Show initial stack state */
    show_stack_info("Initial main()", 0);
    show_vma_details("(Initial State)");
    show_current_rss("(Initial State)");
    printf("\nHit any key to continue to recursive stack growth test...");
    getchar();
    
    printf("\n--- Growing Stack Through Recursion ---\n");
    printf("Each recursion level allocates 4KB on the stack\n");
    
    /* Recursive calls to grow stack */
    recursive_function(1, 8);  /* 8 levels deep = ~32KB stack growth */
    
    printf("\n--- Back to Main Function ---\n");
    show_stack_info("Back in main()", 0);
    
    /* Note: Stack VMA size typically won't change after unwinding - Linux stack VMAs
     * represent the high-water mark of stack usage, not current stack pointer position.
     * However, RSS should show decreased physical memory usage. */
    show_vma_details("(After Recursion Unwinds)");
    show_current_rss("(After Recursion Unwinds)");
    printf("\nHit any key to continue to large stack allocation test (256KB)...");
    getchar();
    
    printf("\n--- Testing Large Stack Allocation ---\n");
    {
        char large_stack_array[256 * 1024];  /* 256KB on stack */
        
        printf("Allocated 256KB array on stack at: %p\n", large_stack_array);
        
        /* Show state after variable declaration but before touching pages
         * This demonstrates virtual space allocation vs physical page allocation */
        show_vma_details("(Large Stack Variable Declared - Before Touching Pages)");
        show_current_rss("(Large Stack Variable Declared - Before Touching Pages)");
        printf("\nHit any key to touch the pages (trigger actual allocation)...");
        getchar();
        
        printf("Touching pages to ensure physical allocation...\n");
        /* Touch pages to ensure allocation */
        for (int i = 0; i < sizeof(large_stack_array); i += 4096) {
            large_stack_array[i] = 'S';
            printf("Touched page at offset %d KB\n", i / 1024);
        }
        
        /* Show state after touching pages - RSS should increase significantly */
        show_vma_details("(After Touching All Pages - Physical Allocation Complete)");
        show_current_rss("(After Touching All Pages - Physical Allocation Complete)");
        printf("\nHit any key to let the large array go out of scope...");
        getchar();
    }
    
    printf("\n--- After Large Array Goes Out of Scope ---\n");
    /* Note: Again, Stack VMA size likely unchanged (high-water mark behavior)
     * but RSS might decrease as stack pages become available for reuse */
    show_vma_details("(After Large Array Cleanup)");
    show_current_rss("(After Large Array Cleanup)");
    
    return 0;
}
