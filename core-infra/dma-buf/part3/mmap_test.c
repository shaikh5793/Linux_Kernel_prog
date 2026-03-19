/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#include <stdio.h>          /* printf, perror */
#include <stdlib.h>         /* exit codes */
#include <fcntl.h>          /* open flags */
#include <unistd.h>         /* close */
#include <sys/mman.h>       /* mmap, munmap */
#include <sys/ioctl.h>      /* ioctl */
#include <linux/ioctl.h>    /* IOCTL macros */
#include <string.h>         /* strlen */
#include <errno.h>          /* errno */

/* IOCTL command to get DMA-BUF file descriptor from exporter */
#define EXPORTER_GET_DMABUF_FD _IOR('E', 0, int)

/* Buffer size (matches PAGE_SIZE in kernel) */
#define BUFFER_SIZE 4096

/**
 * main() - Demonstrate userspace DMA-BUF memory mapping
 *
 * Calling Context:
 *   This is the entry point of the userspace test application.
 *
 * Call Chain:
 *   (shell) -> ./mmap_test
 *
 * Steps to be handled:
 *   1. Open the exporter misc device (`/dev/exporter`).
 *   2. Request a DMA-BUF file descriptor via an IOCTL call.
 *   3. Use `mmap()` on the received file descriptor to map the buffer's
 *      memory into the application's virtual address space.
 *   4. Read from the mapped memory and print the contents to verify access.
 *   5. Clean up by unmapping the memory (`munmap`) and closing all file
 *      descriptors.
 *
 * Return: 0 on success, 1 on failure
 */

int main(void)
{
	int exporter_fd = -1;
	int dmabuf_fd = -1;
	void *mapped_addr = NULL;
	int ret = 0;

	printf("DMA-BUF Userspace Mmap Test Starting...\n");

	/* Step 1: Open the exporter device */
	exporter_fd = open("/dev/exporter", O_RDONLY);
	if (exporter_fd < 0) {
		perror("Failed to open /dev/exporter");
		printf("Make sure the exporter-mmap.ko module is loaded\n");
		return 1;
	}
	printf("Successfully opened /dev/exporter (fd: %d)\n", exporter_fd);

	/* Step 2: Request DMA-BUF file descriptor */
	if (ioctl(exporter_fd, EXPORTER_GET_DMABUF_FD, &dmabuf_fd) < 0) {
		perror("IOCTL EXPORTER_GET_DMABUF_FD failed");
		printf("Failed to get DMA-BUF file descriptor\n");
		ret = 1;
		goto cleanup_exporter;
	}
	printf("Received DMA-BUF file descriptor: %d\n", dmabuf_fd);

	/* Step 3: Map DMA-BUF memory into userspace */
	mapped_addr = mmap(NULL, BUFFER_SIZE, PROT_READ, MAP_SHARED, dmabuf_fd, 0);
	if (mapped_addr == MAP_FAILED) {
		perror("mmap failed");
		printf("Failed to map DMA-BUF memory to userspace\n");
		ret = 1;
		goto cleanup_dmabuf;
	}
	printf("Successfully mapped %d bytes at address %p\n", 
	       BUFFER_SIZE, mapped_addr);

	/* Step 4: Read and display buffer contents */
	printf("Buffer contents: \"%s\"\n", (char *)mapped_addr);
	printf("Buffer length: %zu bytes\n", strlen((char *)mapped_addr));

	/* Demonstrate zero-copy access */
	printf("First few bytes (hex): ");
	for (int i = 0; i < 16 && i < BUFFER_SIZE; i++) {
		printf("%02x ", ((unsigned char *)mapped_addr)[i]);
	}
	printf("\n");

	printf("DMA-BUF userspace mapping demonstration completed successfully!\n");

	/* Step 5: Cleanup resources */
	munmap(mapped_addr, BUFFER_SIZE);
	printf("Unmapped userspace memory\n");

cleanup_dmabuf:
	if (dmabuf_fd >= 0) {
		close(dmabuf_fd);
		printf("Closed DMA-BUF file descriptor\n");
	}

cleanup_exporter:
	close(exporter_fd);
	printf("Closed exporter device\n");

	return ret;
}
