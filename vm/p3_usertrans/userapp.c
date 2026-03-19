/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 * 
 * Userspace application for demonstrating address translation
 * Allocates memory in different ways and passes addresses to kernel module
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>

#define DEBUGFS_VADDR "/sys/kernel/debug/userwalk/vaddr"
#define DEBUGFS_STATUS "/sys/kernel/debug/userwalk/status"
#define TEST_STRING "userspace"
#define BUFFER_SIZE 1024

/* Global variable - in data segment */
static char global_buffer[64] = "global_data";

static void print_memory_info(void *addr, const char *type)
{
    printf("\n=== %s Memory ===\n", type);
    printf("Virtual Address: %p\n", addr);
    printf("Content before: '%.20s'\n", (char *)addr);
}

static int send_address_to_kernel(void *addr)
{
    int fd;
    char addr_str[32];
    
    fd = open(DEBUGFS_VADDR, O_WRONLY);
    if (fd < 0) {
        perror("Failed to open debugfs vaddr file");
        return -1;
    }
    
    snprintf(addr_str, sizeof(addr_str), "%p", addr);
    if (write(fd, addr_str, strlen(addr_str)) < 0) {
        perror("Failed to write address to kernel");
        close(fd);
        return -1;
    }
    
    close(fd);
    return 0;
}

static int check_kernel_status(void)
{
    int fd;
    char status[256];
    ssize_t bytes_read;
    
    fd = open(DEBUGFS_STATUS, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open debugfs status file");
        return -1;
    }
    
    bytes_read = read(fd, status, sizeof(status) - 1);
    if (bytes_read > 0) {
        status[bytes_read] = '\0';
        printf("Kernel status: %s", status);
    }
    
    close(fd);
    return 0;
}

int main(void)
{
    char *heap_mem;
    char *mmap_mem;
    char stack_buffer[64];
    
    printf("=== Userspace Address Translation Demo ===\n");
    printf("Process PID: %d\n", getpid());
    
    /* Initialize different memory regions */
    strcpy(stack_buffer, TEST_STRING);
    strcpy(global_buffer, TEST_STRING);
    
    /* Heap allocation */
    heap_mem = malloc(BUFFER_SIZE);
    if (!heap_mem) {
        perror("malloc failed");
        return 1;
    }
    strcpy(heap_mem, TEST_STRING);
    
    /* Memory mapped allocation */
    mmap_mem = mmap(NULL, BUFFER_SIZE, PROT_READ | PROT_WRITE, 
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mmap_mem == MAP_FAILED) {
        perror("mmap failed");
        free(heap_mem);
        return 1;
    }
    strcpy(mmap_mem, TEST_STRING);
    
    /* Test 1: Stack variable */
    print_memory_info(stack_buffer, "Stack");
    if (send_address_to_kernel(stack_buffer) == 0) {
        sleep(1);
        printf("Content after:  '%.20s'\n", stack_buffer);
        check_kernel_status();
    }
    
    /* Test 2: Heap allocation */
    print_memory_info(heap_mem, "Heap");
    if (send_address_to_kernel(heap_mem) == 0) {
        sleep(1);
        printf("Content after:  '%.20s'\n", heap_mem);
        check_kernel_status();
    }
    
    /* Test 3: Global data */
    print_memory_info(global_buffer, "Global Data");
    if (send_address_to_kernel(global_buffer) == 0) {
        sleep(1);
        printf("Content after:  '%.20s'\n", global_buffer);
        check_kernel_status();
    }
    
    /* Test 4: Memory mapped region */
    print_memory_info(mmap_mem, "Mmap");
    if (send_address_to_kernel(mmap_mem) == 0) {
        sleep(1);
        printf("Content after:  '%.20s'\n", mmap_mem);
        check_kernel_status();
    }
    
    printf("\n=== Memory Layout Summary ===\n");
    printf("Stack:       %p\n", stack_buffer);
    printf("Heap:        %p\n", heap_mem);
    printf("Global Data: %p\n", global_buffer);
    printf("Mmap:        %p\n", mmap_mem);
    
    /* Cleanup */
    free(heap_mem);
    munmap(mmap_mem, BUFFER_SIZE);
    
    printf("\nDemo completed. Check dmesg for kernel module output.\n");
    return 0;
}
