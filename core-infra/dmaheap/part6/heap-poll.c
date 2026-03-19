/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 *
 * heap-poll.c - Poll-based DMA completion test (Part 6)
 *
 * Userspace test program that allocates a DMA buffer, writes test data,
 * submits it to the importer-poll driver for async processing, and then
 * uses poll() on the driver fd to wait for fence-based completion.
 *
 * Key concepts:
 * - poll() on the driver fd (the driver's custom .poll checks fence state)
 * - dma_fence callback wakes the driver's poll wait queue
 * - Modern alternative to SIGIO (Part 5) — epoll-compatible
 */

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <linux/dma-heap.h>

#define MY_DMA_IOCTL_POLL_SUBMIT _IOW('M', 9, int)

int main(void)
{
	int heap_fd, driver_fd, ret;
	struct dma_heap_allocation_data alloc_data;
	void *buf;
	struct pollfd pfd;

	printf("=== DMA Poll-based Completion Test ===\n\n");

	/* Allocate from system heap */
	heap_fd = open("/dev/dma_heap/system", O_RDWR | O_CLOEXEC);
	if (heap_fd < 0) {
		perror("Failed to open /dev/dma_heap/system");
		exit(EXIT_FAILURE);
	}

	memset(&alloc_data, 0, sizeof(alloc_data));
	alloc_data.len = 4096;
	alloc_data.fd_flags = O_RDWR | O_CLOEXEC;
	alloc_data.heap_flags = 0;

	ret = ioctl(heap_fd, DMA_HEAP_IOCTL_ALLOC, &alloc_data);
	if (ret < 0) {
		perror("DMA_HEAP_IOCTL_ALLOC failed");
		close(heap_fd);
		exit(EXIT_FAILURE);
	}
	printf("Allocated DMA buffer, fd: %d\n", alloc_data.fd);
	close(heap_fd);

	/* Map and write test data */
	buf = mmap(NULL, alloc_data.len, PROT_READ | PROT_WRITE,
		   MAP_SHARED, alloc_data.fd, 0);
	if (buf == MAP_FAILED) {
		perror("mmap failed");
		close(alloc_data.fd);
		exit(EXIT_FAILURE);
	}
	snprintf(buf, alloc_data.len, "Poll test data");
	printf("User wrote: '%s'\n", (char *)buf);
	munmap(buf, alloc_data.len);

	/* Submit buffer to driver for async processing */
	driver_fd = open("/dev/dummy_dma_poll_device", O_RDWR);
	if (driver_fd < 0) {
		perror("Failed to open /dev/dummy_dma_poll_device");
		close(alloc_data.fd);
		exit(EXIT_FAILURE);
	}

	ret = ioctl(driver_fd, MY_DMA_IOCTL_POLL_SUBMIT, &alloc_data.fd);
	if (ret < 0) {
		perror("MY_DMA_IOCTL_POLL_SUBMIT failed");
		close(driver_fd);
		close(alloc_data.fd);
		exit(EXIT_FAILURE);
	}
	printf("Submitted to driver for processing.\n");

	/*
	 * Poll on the driver fd — the driver implements a custom .poll
	 * that checks whether the dma_fence has been signaled. The fence
	 * callback (registered via dma_fence_add_callback) wakes the
	 * driver's poll wait queue when the delayed work signals the fence.
	 */
	printf("Waiting for completion via poll() on driver fd...\n");

	pfd.fd = driver_fd;
	pfd.events = POLLIN;
	pfd.revents = 0;

	ret = poll(&pfd, 1, 5000); /* 5 second timeout */
	if (ret > 0) {
		printf("poll() returned: revents=0x%x\n", pfd.revents);
		if (pfd.revents & POLLIN) {
			printf("Buffer ready (fence signaled)!\n");

			/* Re-map and read the processed result */
			buf = mmap(NULL, alloc_data.len, PROT_READ | PROT_WRITE,
				   MAP_SHARED, alloc_data.fd, 0);
			if (buf != MAP_FAILED) {
				printf("Processed buffer: '%s'\n", (char *)buf);
				munmap(buf, alloc_data.len);
			}
		}
	} else if (ret == 0) {
		printf("poll() timed out! Fence not signaled within 5 seconds.\n");
	} else {
		perror("poll() failed");
	}

	close(driver_fd);
	close(alloc_data.fd);

	printf("\nCompare with Part 5 (SIGIO-based):\n");
	printf("  Part 5: Uses fasync/SIGIO signals for async notification\n");
	printf("  Part 6: Uses poll() on driver fd with dma_fence (modern approach)\n");
	return 0;
}
