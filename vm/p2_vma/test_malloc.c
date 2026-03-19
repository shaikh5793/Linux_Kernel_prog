/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 * 
 * Usage: sudo ./test_malloc
 * Requires: inspectvma module loaded
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>

#define DEBUGFS_FILE "/sys/kernel/debug/inspectvma/vma_details"

/*
 * show_vma_summary() - Display filtered VMA info focusing on heap and anonymous regions
 * Called by: main() at various points to show malloc() effects on VMAs
 * Purpose: Shows heap VMA changes and anonymous mappings created by large malloc()
 */
static void show_vma_summary(const char *context)
{
    FILE *fp;
    char line[1024];
    
    printf("\n=== VMA Summary %s ===\n", context);
    
    fp = fopen(DEBUGFS_FILE, "r");
    if (!fp) {
        printf("Error: Cannot read %s\n", DEBUGFS_FILE);
        return;
    }
    
    // Show heap and summary information
    while (fgets(line, sizeof(line), fp)) {
        // Show key information
        if (strstr(line, "Heap:") ||
            strstr(line, "[heap]") ||
            strstr(line, "Total VMAs:") ||
            strstr(line, "Total Virtual Memory:") ||
            strstr(line, "RSS")) {
            printf("%s", line);
        }
        
        // Show any anonymous mappings (large malloc uses mmap)
        if (strstr(line, "[anon]") && !strstr(line, "VIRTUAL")) {
            printf("ANON VMA: %s", line);
        }
    }
    
    fclose(fp);
}

/*
 * check_memory_with_mincore() - Check which pages are actually resident in memory
 * Called by: main() after large allocations to demonstrate lazy allocation
 * Purpose: Uses mincore() to show difference between virtual and physical memory
 */
static void check_memory_with_mincore(void *ptr, size_t size, const char *context)
{
    if (!ptr || size == 0) return;
    
    size_t page_size = getpagesize();
    size_t num_pages = (size + page_size - 1) / page_size;
    unsigned char *vec = malloc(num_pages);
    
    if (!vec) return;
    
    printf("\n--- Memory Residency Check %s ---\n", context);
    
    if (mincore(ptr, size, vec) == -1) {
        printf("mincore() failed (normal for heap memory)\n");
        free(vec);
        return;
    }
    
    size_t resident = 0;
    for (size_t i = 0; i < num_pages; i++) {
        if (vec[i] & 1) resident++;
    }
    
    printf("Pages in memory: %zu/%zu (%.1f%%)\n", 
           resident, num_pages, (double)resident * 100.0 / num_pages);
    
    free(vec);
}

int main()
{
    printf("Process PID: %d\n", getpid());
    
    // Check access to debugfs file
    if (access(DEBUGFS_FILE, R_OK) != 0) {
        printf("Error: Cannot access %s\n", DEBUGFS_FILE);
        printf("Make sure module is loaded and running as root\n");
        return 1;
    }
    
    // Show initial heap state
    show_vma_summary("(Initial State)");
    printf("\nHit any key to continue to small malloc() test...");
    getchar();
    
    printf("\n--- Test 1: Small malloc() Allocations ---\n");
    printf("Small allocations typically extend the heap using brk()\n");
    
    void *small_ptrs[5];
    for (int i = 0; i < 5; i++) {
        small_ptrs[i] = malloc(1024);  // 1KB each
        if (small_ptrs[i]) {
            memset(small_ptrs[i], 'A' + i, 1024);
            printf("Allocated 1KB block %d at: %p\n", i+1, small_ptrs[i]);
        }
    }
    
    show_vma_summary("(After Small malloc Allocations)");
    printf("\nHit any key to continue to medium malloc() test...");
    getchar();
    
    printf("\n--- Test 2: Medium malloc() Allocation ---");
    printf("Medium allocations may still use heap extension\n");
    
    void *medium_ptr = malloc(64 * 1024);  // 64KB
    if (medium_ptr) {
        memset(medium_ptr, 'M', 64 * 1024);
        printf("Allocated 64KB at: %p\n", medium_ptr);
        show_vma_summary("(After 64KB malloc)");
    }
    printf("\nHit any key to continue to large malloc() test (2MB - will use mmap)...");
    getchar();
    
    printf("\n--- Test 3: Large malloc() Allocation ---\n");
    printf("Large allocations typically use mmap() instead of brk()\n");
    
    void *large_ptr = malloc(2 * 1024 * 1024);  // 2MB
    if (large_ptr) {
        printf("Allocated 2MB at: %p\n", large_ptr);
        printf("This large allocation likely uses mmap() internally\n");
        
        // Touch scattered pages
        char *ptr = (char*)large_ptr;
        for (int i = 0; i < 10; i++) {
            ptr[i * 200 * 1024] = 'L';  // Every 200KB
        }
        
        show_vma_summary("(After 2MB malloc - Should Show New Anonymous VMA)");
        check_memory_with_mincore(large_ptr, 2 * 1024 * 1024, "(2MB allocation)");
    }
    printf("\nHit any key to continue to very large malloc() test (16MB)...");
    getchar();
    
    printf("\n--- Test 4: Very Large malloc() Allocation ---\n");
    printf("Very large allocations definitely use mmap()\n");
    
    void *huge_ptr = malloc(16 * 1024 * 1024);  // 16MB
    if (huge_ptr) {
        printf("Allocated 16MB at: %p\n", huge_ptr);
        
        // Touch first and last page
        ((char*)huge_ptr)[0] = 'H';
        ((char*)huge_ptr)[16*1024*1024 - 1] = 'E';
        
        show_vma_summary("(After 16MB malloc - New mmap VMA)");
        check_memory_with_mincore(huge_ptr, 16 * 1024 * 1024, "(16MB allocation)");
    }
    printf("\nHit any key to continue to cleanup and observe VMA changes...");
    getchar();
    
    printf("\n--- Cleanup and Observation ---\n");
    
    // Free large allocations first
    if (huge_ptr) {
        free(huge_ptr);
        printf("Freed 16MB allocation\n");
        show_vma_summary("(After freeing 16MB)");
    }
    
    if (large_ptr) {
        free(large_ptr);
        printf("Freed 2MB allocation\n");
        show_vma_summary("(After freeing 2MB)");
    }
    
    if (medium_ptr) {
        free(medium_ptr);
        printf("Freed 64KB allocation\n");
    }
    
    // Free small allocations
    for (int i = 0; i < 5; i++) {
        if (small_ptrs[i]) {
            free(small_ptrs[i]);
        }
    }
    printf("Freed all small allocations\n");
    
    show_vma_summary("(After All Cleanup)");
    return 0;
}
