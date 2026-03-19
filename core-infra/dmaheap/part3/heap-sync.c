/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 *
 * heap-sync.c - DMA heap allocation with synchronized transfer test (Part 3)
 *
 * Userspace test program that allocates a buffer from /dev/dma_heap/system,
 * fills it with test data, and passes the fd to the importer-sync kernel
 * driver for synchronized CPU read + simulated DMA transfer.
 *
 * Key concepts:
 * - DMA_HEAP_IOCTL_ALLOC to allocate from system heap
 * - mmap() to write test data that the driver will read via vmap
 * - Driver performs both CPU access (read) and DMA mapping (transfer)
 */

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <linux/dma-heap.h>

#define MY_DMA_IOCTL_TRANSFER _IOW('M', 5, int)

int main(void)
{
	int heap_fd, driver_fd, ret;
	struct dma_heap_allocation_data alloc_data;
	void *buf;

	/* Open the DMA heap device (system heap) */
	heap_fd = open("/dev/dma_heap/system", O_RDWR | O_CLOEXEC);
	if (heap_fd < 0) {
		perror("Failed to open /dev/dma_heap/system");
		exit(EXIT_FAILURE);
	}

	/* Initialize allocation parameters */
	memset(&alloc_data, 0, sizeof(alloc_data));
	alloc_data.len = 4096;  /* Allocate 4KB */
	alloc_data.fd_flags = O_RDWR | O_CLOEXEC;
	alloc_data.heap_flags = 0;

	/* Allocate the DMA buffer using the dma_heap interface */
	ret = ioctl(heap_fd, DMA_HEAP_IOCTL_ALLOC, &alloc_data);
	if (ret < 0) {
		perror("DMA_HEAP_IOCTL_ALLOC failed");
		close(heap_fd);
		exit(EXIT_FAILURE);
	}
	printf("Allocated DMA buffer, fd: %d\n", alloc_data.fd);

	/* Map the DMA buffer to fill it with test data */
	buf = mmap(NULL, alloc_data.len, PROT_READ | PROT_WRITE, MAP_SHARED, alloc_data.fd, 0);
	if (buf == MAP_FAILED) {
		perror("mmap failed");
		close(alloc_data.fd);
		close(heap_fd);
		exit(EXIT_FAILURE);
	}
	snprintf(buf, alloc_data.len, "Test data for DMA transfer");
	printf("Buffer filled with data: %s\n", (char *)buf);
	munmap(buf, alloc_data.len);
	close(heap_fd);  /* Close the heap device now; the allocated buffer FD remains valid */

	/* Open the driver device that will program the DMA transfer */
	driver_fd = open("/dev/dummy_dma_transfer_device", O_RDWR);
	if (driver_fd < 0) {
		perror("Failed to open /dev/dummy_dma_transfer_device");
		close(alloc_data.fd);
		exit(EXIT_FAILURE);
	}

	/* Pass the DMA buffer file descriptor to the driver via ioctl */
	ret = ioctl(driver_fd, MY_DMA_IOCTL_TRANSFER, &alloc_data.fd);
	if (ret < 0) {
		perror("MY_DMA_IOCTL_TRANSFER ioctl failed");
	} else {
		printf("DMA transfer programmed successfully.\n");
	}

	close(driver_fd);
	close(alloc_data.fd);
	return 0;
}
