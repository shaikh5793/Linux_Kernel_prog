/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 *
 * mmap() Test 2 - mprotect, VMA Splitting, and Multiple Mappings
 * Usage: sudo ./test_mmap2
 * Requires: inspectvma module loaded
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <signal.h>

#define DEBUGFS_FILE "/sys/kernel/debug/inspectvma/vma_details"

/*
 * show_vma_details() - Display complete VMA table and summary information
 * Called by: main() at various points to show mmap() effects on VMA layout
 * Purpose: Shows full VMA table to observe mmap() creating/modifying VMAs
 */
static void show_vma_details(const char *context)
{
    FILE *fp;
    char line[1024];
    int vma_count = 0;
    
    printf("\n=== VMA Details %s ===\n", context);
    
    fp = fopen(DEBUGFS_FILE, "r");
    if (!fp) {
        printf("Error: Cannot read %s\n", DEBUGFS_FILE);
        return;
    }
    
    // Show VMA table and summary
    int in_vma_table = 0;
    while (fgets(line, sizeof(line), fp)) {
        // Show header info
        if (strstr(line, "VMA Inspector") || strstr(line, "Process PID")) {
            printf("%s", line);
        }
        
        // Detect VMA table start
        if (strstr(line, "Virtual Memory Areas")) {
            in_vma_table = 1;
            printf("%s", line);
            continue;
        }
        
        // Show VMA table entries
        if (in_vma_table) {
            printf("%s", line);
            if (strstr(line, "0x") && !strstr(line, "===")) {
                vma_count++;
            }
            // Stop at summary
            if (strstr(line, "Summary:")) {
                in_vma_table = 0;
            }
        }
        
        // Show summary information
        if (strstr(line, "Total VMAs:") ||
            strstr(line, "Total Virtual Memory:") ||
            strstr(line, "RSS")) {
            printf("%s", line);
        }
    }
    
    fclose(fp);
    printf("Displayed %d VMAs\n", vma_count);
}

/*
 * check_lazy_allocation() - Demonstrate lazy allocation using mincore()
 * Called by: main() after mmap() calls to show virtual vs physical memory
 * Purpose: Shows how mmap() creates virtual mappings but physical pages allocated on demand
 */
static void check_lazy_allocation(void *ptr, size_t size, const char *context)
{
    size_t page_size = getpagesize();
    size_t num_pages = (size + page_size - 1) / page_size;
    unsigned char *vec = malloc(num_pages);
    
    if (!vec) return;
    
    printf("\n--- Lazy Allocation Check %s ---\n", context);
    
    if (mincore(ptr, size, vec) == -1) {
        perror("mincore failed");
        free(vec);
        return;
    }
    
    size_t resident = 0;
    size_t not_resident = 0;
    
    for (size_t i = 0; i < num_pages; i++) {
        if (vec[i] & 1) {
            resident++;
        } else {
            not_resident++;
        }
    }
    
    printf("Memory allocated: %zu bytes (%zu pages)\n", size, num_pages);
    printf("Pages in memory:     %zu (%.1f%%) - PHYSICAL\n", 
           resident, (double)resident * 100.0 / num_pages);
    printf("Pages NOT in memory: %zu (%.1f%%) - VIRTUAL ONLY (lazy)\n",
           not_resident, (double)not_resident * 100.0 / num_pages);
    
    // Show first 16 pages status
    printf("First 16 pages: ");
    for (size_t i = 0; i < 16 && i < num_pages; i++) {
        printf("%c", (vec[i] & 1) ? 'M' : 'L');
    }
    printf(" (M=Memory, L=Lazy)\n");
    
    free(vec);
}

/*
 * segfault_handler() - Handle segmentation faults from protection violations
 * Purpose: Demonstrate that mprotect() actually works by catching access violations
 */
static void segfault_handler(int sig)
{
    printf("\n*** SEGFAULT caught! ***\n");
    printf("This proves that mprotect() successfully changed page permissions.\n");
    printf("Attempting to write to read-only memory caused a segmentation fault.\n");
    printf("*** Continuing execution... ***\n\n");
}

int main()
{
    printf("=== mmap() Test 2 ===\n");
    printf("Tests: Multiple mappings, mprotect(), and VMA splitting\n");
    printf("Process PID: %d\n", getpid());
    printf("Page size: %d bytes\n", getpagesize());
    
    // Check access to debugfs file
    if (access(DEBUGFS_FILE, R_OK) != 0) {
        printf("Error: Cannot access %s\n", DEBUGFS_FILE);
        printf("Make sure module is loaded and running as root\n");
        return 1;
    }
    
    // Set up signal handler for segfaults
    signal(SIGSEGV, segfault_handler);
    
    // Show initial VMA state
    show_vma_details("(Initial State)");
    printf("\nHit any key to continue to multiple mmap() test...");
    getchar();
    
    printf("\n--- Test 1: Multiple mmap() with Different Properties ---\n");
    
    void *mappings[4];
    size_t sizes[4] = {1024*1024, 2*1024*1024, 4*1024*1024, 8*1024*1024};  // 1MB, 2MB, 4MB, 8MB
    
    printf("Creating 4 different anonymous mappings with various sizes...\n");
    for (int i = 0; i < 4; i++) {
        mappings[i] = mmap(NULL, sizes[i], PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mappings[i] != MAP_FAILED) {
            printf("Mapping %d: %zu MB at %p\n", i+1, sizes[i]/(1024*1024), mappings[i]);
            // Touch first page only to trigger allocation
            *(char*)mappings[i] = '1' + i;
            printf("  - Touched first page with value '%c'\n", '1' + i);
        } else {
            perror("mmap failed");
        }
    }
    
    show_vma_details("(With Multiple Anonymous Mappings)");
    printf("\nObserve: Multiple separate VMA entries for each mapping\n");
    printf("Hit any key to continue to mprotect() VMA splitting test...");
    getchar();
    
    printf("\n--- Test 2: mprotect() - VMA Splitting Demonstration ---\n");
    
    if (mappings[1] != MAP_FAILED) {  // Use the 2MB mapping
        printf("Working with 2MB mapping at %p\n", mappings[1]);
        printf("Original mapping: 2MB with READ+WRITE permissions\n");
        
        // Show memory layout before mprotect
        printf("\nMemory layout:\n");
        printf("  Start:  %p (0MB - READ+WRITE)\n", mappings[1]);
        printf("  Middle: %p (512KB - will become READ-ONLY)\n", (char*)mappings[1] + 512*1024);
        printf("  End:    %p (2MB - READ+WRITE)\n", (char*)mappings[1] + sizes[1]);
        
        show_vma_details("(Before mprotect - Single 2MB VMA)");
        printf("Hit any key to apply mprotect() and potentially split the VMA...");
        getchar();
        
        // Change protection of middle 1MB to read-only
        void *middle = (char*)mappings[1] + 512*1024;  // 512KB offset
        size_t middle_size = 1024*1024;  // 1MB size
        
        printf("\nApplying mprotect() to make middle 1MB (512KB-1536KB) read-only...\n");
        if (mprotect(middle, middle_size, PROT_READ) == 0) {
            printf("✓ mprotect() successful - middle section is now read-only\n");
            show_vma_details("(After mprotect - Check for VMA Split)");
            printf("\nLook for VMA splitting: The original 2MB VMA may be split into:\n");
            printf("  1. First 512KB:  READ+WRITE\n");
            printf("  2. Middle 1MB:   READ-ONLY\n");
            printf("  3. Last 512KB:   READ+WRITE\n");
            
            printf("\nHit any key to test the protection by trying to write to read-only region...");
            getchar();
            
            // Test the protection - this should cause a segfault
            printf("\nTesting memory protection...\n");
            printf("Writing to first part (should work): ");
            *(char*)mappings[1] = 'X';
            printf("✓ Success\n");
            
            printf("Writing to read-only middle part (should cause SEGFAULT): ");
            fflush(stdout);
            
            // This should trigger our signal handler
            volatile char *test_ptr = (volatile char*)middle;
            *test_ptr = 'Y';  // This should cause SIGSEGV
            
            printf("Unexpectedly succeeded (no segfault occurred)\n");
            
        } else {
            perror("mprotect failed");
        }
    }
    
    printf("Hit any key to continue to advanced protection test...");
    getchar();
    
    printf("\n--- Test 3: Advanced Protection Changes ---\n");
    
    if (mappings[2] != MAP_FAILED) {  // Use the 4MB mapping
        printf("Working with 4MB mapping at %p\n", mappings[2]);
        
        // Create multiple protection regions
        void *region1 = mappings[2];                           // First 1MB
        void *region2 = (char*)mappings[2] + 1024*1024;       // Second 1MB
        void *region3 = (char*)mappings[2] + 2*1024*1024;     // Third 1MB
        void *region4 = (char*)mappings[2] + 3*1024*1024;     // Fourth 1MB
        
        printf("\nCreating complex protection pattern in 4MB mapping:\n");
        printf("  Region 1 (0-1MB):   READ+WRITE (original)\n");
        printf("  Region 2 (1-2MB):   READ-ONLY\n");
        printf("  Region 3 (2-3MB):   NO ACCESS\n");
        printf("  Region 4 (3-4MB):   READ+WRITE (original)\n");
        
        // Apply different protections
        if (mprotect(region2, 1024*1024, PROT_READ) == 0 &&
            mprotect(region3, 1024*1024, PROT_NONE) == 0) {
            
            printf("✓ Complex protection pattern applied\n");
            show_vma_details("(After Complex mprotect - Multiple VMA Splits)");
            
            printf("\nThis should show the original 4MB VMA split into up to 4 separate VMAs\n");
            printf("with different protection flags.\n");
        } else {
            perror("Complex mprotect failed");
        }
    }
    
    printf("Hit any key to continue to cleanup...");
    getchar();
    
    printf("\n--- Cleanup ---\n");
    
    // Unmap all mappings
    for (int i = 0; i < 4; i++) {
        if (mappings[i] != MAP_FAILED) {
            munmap(mappings[i], sizes[i]);
            printf("Unmapped %zu MB mapping %d\n", sizes[i]/(1024*1024), i+1);
        }
    }
    
    show_vma_details("(After Complete Cleanup)");
    
    printf("\nHit any key to exit...");
    getchar();
    return 0;
}
