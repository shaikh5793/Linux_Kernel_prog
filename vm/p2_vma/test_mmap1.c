/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 *
 * mmap() Test 1 - Anonymous and File-backed Mappings
 * Usage: sudo ./test_mmap1
 * Requires: inspectvma module loaded
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>

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

int main()
{
    printf("=== mmap() Test 1 ===\n");
    printf("Tests: Anonymous mappings and File-backed mappings\n");
    printf("Process PID: %d\n", getpid());
    printf("Page size: %d bytes\n", getpagesize());
    
    // Check access to debugfs file
    if (access(DEBUGFS_FILE, R_OK) != 0) {
        printf("Error: Cannot access %s\n", DEBUGFS_FILE);
        printf("Make sure module is loaded and running as root\n");
        return 1;
    }
    
    // Show initial VMA state
    show_vma_details("(Initial State)");
    printf("\nHit any key to continue to anonymous mmap() test...");
    getchar();
    
    printf("\n--- Test 1: Anonymous mmap() ---\n");
    printf("Creating large anonymous mapping with lazy allocation\n");
    
    size_t anon_size = 4 * 1024 * 1024;  // 4MB
    void *anon_ptr = mmap(NULL, anon_size, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
    if (anon_ptr == MAP_FAILED) {
        perror("anonymous mmap failed");
    } else {
        printf("Anonymous mapping: %zu MB at %p\n", anon_size/(1024*1024), anon_ptr);
        show_vma_details("(After Anonymous mmap)");
        check_lazy_allocation(anon_ptr, anon_size, "(Before touching pages)");
        printf("\nHit any key to touch pages and trigger physical allocation...");
        getchar();
        
        // Touch scattered pages to trigger allocation
        printf("\nTouching scattered pages to trigger physical allocation...\n");
        char *ptr = (char*)anon_ptr;
        for (int i = 0; i < 10; i++) {
            size_t offset = i * (anon_size / 10);
            ptr[offset] = 'A' + i;
            printf("Touched page at offset %zu MB\n", offset/(1024*1024));
        }
        
        check_lazy_allocation(anon_ptr, anon_size, "(After touching scattered pages)");
        printf("\nHit any key to continue to file-backed mmap test...");
        getchar();
    }
    
    printf("\n--- Test 2: File-backed mmap() ---\n");
    printf("Creating file mapping to demonstrate file-backed VMA\n");
    
    // Create temporary file
    char filename[] = "/tmp/mmap_test_XXXXXX";
    int fd = mkstemp(filename);
    if (fd >= 0) {
        // Write test data
        const char *data = "This is test data for file mapping demonstration.\n";
        size_t file_size = 64 * 1024;  // 64KB file
        
        for (size_t written = 0; written < file_size; ) {
            ssize_t n = write(fd, data, strlen(data));
            if (n > 0) written += n;
            else break;
        }
        
        printf("Created test file: %s (%zu bytes)\n", filename, file_size);
        printf("Hit any key to create file mapping...");
        getchar();
        
        // Map the file
        void *file_ptr = mmap(NULL, file_size, PROT_READ | PROT_WRITE, 
                             MAP_SHARED, fd, 0);
        
        if (file_ptr == MAP_FAILED) {
            perror("file mmap failed");
        } else {
            printf("File mapping: %zu KB at %p\n", file_size/1024, file_ptr);
            show_vma_details("(After File mmap - Should Show Filename)");
            check_lazy_allocation(file_ptr, file_size, "(File mapping)");
            printf("\nHit any key to modify file through mapping...");
            getchar();
            
            // Modify file through mapping
            printf("\nModifying file content through memory mapping...\n");
            strcpy((char*)file_ptr, "MODIFIED via mmap: ");
            printf("File content modified through memory mapping\n");
            
            printf("Hit any key to unmap file...");
            getchar();
            munmap(file_ptr, file_size);
        }
        
        close(fd);
        unlink(filename);
        printf("File mapping unmapped and file deleted\n");
        show_vma_details("(After File Unmap)");
    }
    
    printf("\n--- Cleanup ---\n");
    
    // Unmap anonymous mapping
    if (anon_ptr != MAP_FAILED) {
        munmap(anon_ptr, anon_size);
        printf("Unmapped 4MB anonymous mapping\n");
    }
    
    show_vma_details("(After Complete Cleanup)");
    getchar();
    return 0;
}
