/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 *
 * heap-multisg.c - Multi-SG DMA heap allocation test (Part 4)
 *
 * Userspace test program that allocates a 4MB buffer from the system
 * DMA heap and passes it to the importer-multisg kernel driver to
 * demonstrate scatter-gather table traversal with multiple entries.
 *
 * Key concepts:
 * - System heap page pool max order is 8 (1MB compound pages)
 * - 4MB allocation requires 4 separate order-8 pages = 4 SG entries
 * - for_each_sgtable_dma_sg() iterates all DMA-mapped SG entries
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

#define MY_DMA_IOCTL_MULTISG _IOW('M', 8, int)
/*
 * The system heap's page pool uses order 8 (1MB) as its largest page
 * order. A 4MB allocation requires 4 separate order-8 pages, each
 * becoming its own SG entry. The driver uses DMA_BIT_MASK(64) to
 * bypass SWIOTLB, which can't bounce-buffer segments > 256KB.
 */
#define ALLOC_SIZE (4 * 1024 * 1024)

int main(void)
{
	int heap_fd, driver_fd, ret;
	struct dma_heap_allocation_data alloc_data;
	void *buf;

	printf("=== DMA Multi-SG Test ===\n\n");

	/* Open the system heap — always available */
	heap_fd = open("/dev/dma_heap/system", O_RDWR | O_CLOEXEC);
	if (heap_fd < 0) {
		perror("Failed to open /dev/dma_heap/system");
		exit(EXIT_FAILURE);
	}
	printf("Using heap: system\n");

	/* Allocate 4MB buffer — needs 4 order-8 pages = 4 SG entries */
	memset(&alloc_data, 0, sizeof(alloc_data));
	alloc_data.len = ALLOC_SIZE;
	alloc_data.fd_flags = O_RDWR | O_CLOEXEC;
	alloc_data.heap_flags = 0;

	ret = ioctl(heap_fd, DMA_HEAP_IOCTL_ALLOC, &alloc_data);
	if (ret < 0) {
		perror("DMA_HEAP_IOCTL_ALLOC failed");
		close(heap_fd);
		exit(EXIT_FAILURE);
	}
	printf("Allocated %d bytes (%d MB), fd: %d\n",
	       ALLOC_SIZE, ALLOC_SIZE / (1024 * 1024), alloc_data.fd);
	close(heap_fd);

	/* Map and write test data */
	buf = mmap(NULL, alloc_data.len, PROT_READ | PROT_WRITE,
		   MAP_SHARED, alloc_data.fd, 0);
	if (buf == MAP_FAILED) {
		perror("mmap failed");
		close(alloc_data.fd);
		exit(EXIT_FAILURE);
	}
	snprintf(buf, alloc_data.len, "Multi-SG test, size=%d", ALLOC_SIZE);
	printf("Buffer filled: %s\n", (char *)buf);
	munmap(buf, alloc_data.len);

	/* Pass to kernel driver */
	driver_fd = open("/dev/dummy_dma_multisg_device", O_RDWR);
	if (driver_fd < 0) {
		perror("Failed to open /dev/dummy_dma_multisg_device");
		close(alloc_data.fd);
		exit(EXIT_FAILURE);
	}

	ret = ioctl(driver_fd, MY_DMA_IOCTL_MULTISG, &alloc_data.fd);
	if (ret < 0) {
		perror("MY_DMA_IOCTL_MULTISG failed");
		close(driver_fd);
		close(alloc_data.fd);
		exit(EXIT_FAILURE);
	}

	printf("\nCheck dmesg for SG table details:\n");
	printf("  sudo dmesg | grep dma_multisg\n");
	printf("\nSystem heap page pool max order = 8 (1MB compound pages).\n");
	printf("4MB requires 4 separate pages, each becoming an SG entry.\n");

	close(driver_fd);
	close(alloc_data.fd);
	return 0;
}
