/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 *
 * heap-alloc.c - DMA heap allocation and driver submission test (Part 1)
 *
 * Userspace test program that allocates a buffer from /dev/dma_heap/system,
 * fills it with test data via mmap, and passes the file descriptor to the
 * importer-map kernel driver via ioctl.
 *
 * Key concepts:
 * - DMA_HEAP_IOCTL_ALLOC to allocate from system heap
 * - mmap() to write test data into the DMA buffer
 * - ioctl() to pass the buffer fd to the kernel importer
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

#define MY_DMA_IOCTL_MAP _IOW('M', 2, int)

int main(void)
{
	int heap_fd, driver_fd;
	struct dma_heap_allocation_data alloc_data;
	void *buf;

	/* Open the DMA heap device (system heap in this example) */
	heap_fd = open("/dev/dma_heap/system", O_RDWR | O_CLOEXEC);
	if (heap_fd < 0) {
		perror("Failed to open /dev/dma_heap/system");
		exit(EXIT_FAILURE);
	}

	/* Initialize allocation parameters */
	memset(&alloc_data, 0, sizeof(alloc_data));
	alloc_data.len = 4096;                /* Allocate 4KB */
	alloc_data.fd_flags = O_RDWR | O_CLOEXEC;  /* Ensure read/write access */
	alloc_data.heap_flags = 0;            /* No additional heap flags */

	/* Allocate the DMA buffer via the dma_heap interface */
	if (ioctl(heap_fd, DMA_HEAP_IOCTL_ALLOC, &alloc_data) < 0) {
		perror("DMA_HEAP_IOCTL_ALLOC failed");
		close(heap_fd);
		exit(EXIT_FAILURE);
	}
	printf("Allocated DMA buffer, fd: %u\n", alloc_data.fd);

	/* Map the DMA buffer to fill it with test data */
	buf = mmap(NULL, alloc_data.len, PROT_READ | PROT_WRITE, MAP_SHARED, alloc_data.fd, 0);
	if (buf == MAP_FAILED) {
		perror("mmap failed");
		close(alloc_data.fd);
		close(heap_fd);
		exit(EXIT_FAILURE);
	}
	snprintf(buf, alloc_data.len, "Test DMA buffer data.");
	printf("Buffer filled with data: %s\n", (char *)buf);

	/* Unmap the buffer as the driver will map it on its side */
	munmap(buf, alloc_data.len);

	/* Open the custom DMA mapping driver device */
	driver_fd = open("/dev/dummy_dma_map_device", O_RDWR);
	if (driver_fd < 0) {
		perror("Failed to open /dev/my_dma_map_device");
		close(alloc_data.fd);
		close(heap_fd);
		exit(EXIT_FAILURE);
	}

	/* Pass the DMA buffer file descriptor to the driver*/
	if (ioctl(driver_fd, MY_DMA_IOCTL_MAP, &alloc_data.fd) < 0) {
		perror("MY_DMA_IOCTL_MAP ioctl failed");
		close(driver_fd);
		close(alloc_data.fd);
		close(heap_fd);
		exit(EXIT_FAILURE);
	}
	printf("DMA buffer mapping request sent to driver.\n");

	/* Clean up */
	close(driver_fd);
	close(alloc_data.fd);
	close(heap_fd);

	return 0;
}
