/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 *
 * heap-array.c - Multi-buffer DMA heap allocation test (Part 2)
 *
 * Userspace test program that allocates 4 DMA buffers from the system heap,
 * fills each with test data, and passes all file descriptors to the
 * importer-array kernel driver in a single ioctl call.
 *
 * Key concepts:
 * - Multiple DMA_HEAP_IOCTL_ALLOC calls for independent buffers
 * - Custom ioctl struct bundling an array of fds
 * - Analogous to multi-plane video buffers (Y/U/V)
 */

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <linux/types.h>
#include <linux/dma-heap.h>

#define MAX_BUFFERS 4
#define MY_DMA_IOCTL_MAP_ARRAY _IOW('M', 3, struct dma_buffer_array)

/* Structure to pass an array of DMA buffer file descriptors */
struct dma_buffer_array {
	__u32 count;
	int fds[MAX_BUFFERS];
};

int main(void)
{
	int heap_fd, driver_fd, i, ret;
	struct dma_buffer_array buf_array;
	struct dma_heap_allocation_data alloc_data;
	void *buf;

	/* Open the DMA heap device (system heap) */
	heap_fd = open("/dev/dma_heap/system", O_RDWR | O_CLOEXEC);
	if (heap_fd < 0) {
		perror("Failed to open /dev/dma_heap/system");
		exit(EXIT_FAILURE);
	}

	buf_array.count = MAX_BUFFERS;
	for (i = 0; i < MAX_BUFFERS; i++) {
		memset(&alloc_data, 0, sizeof(alloc_data));
		alloc_data.len = 4096; /* Allocate 4KB per buffer */
		alloc_data.fd_flags = O_RDWR | O_CLOEXEC;
		alloc_data.heap_flags = 0;

		ret = ioctl(heap_fd, DMA_HEAP_IOCTL_ALLOC, &alloc_data);
		if (ret < 0) {
			perror("DMA_HEAP_IOCTL_ALLOC failed");
			/* Clean up previously allocated fds */
			for (int j = 0; j < i; j++)
				close(buf_array.fds[j]);
			close(heap_fd);
			exit(EXIT_FAILURE);
		}
		buf_array.fds[i] = alloc_data.fd;
		printf("Allocated DMA buffer %d, fd: %d\n", i, alloc_data.fd);

		/* Optionally map and fill the buffer with test data */
		buf = mmap(NULL, alloc_data.len, PROT_READ | PROT_WRITE, MAP_SHARED, alloc_data.fd, 0);
		if (buf == MAP_FAILED) {
			perror("mmap failed");
			for (int j = 0; j < i; j++)
				close(buf_array.fds[j]);
			close(alloc_data.fd);
			close(heap_fd);
			exit(EXIT_FAILURE);
		}
		char text[64];
		snprintf(text, sizeof(text), "Buffer %d data", i);
		strncpy(buf, text, alloc_data.len - 1);
		((char *)buf)[alloc_data.len - 1] = '\0';
		printf("Buffer %d filled with data: %s\n", i, (char *)buf);
		munmap(buf, alloc_data.len);
	}

	close(heap_fd);

	/* Open the driver device */
	driver_fd = open("/dev/dummy_dma_array_device", O_RDWR);
	if (driver_fd < 0) {
		perror("Failed to open /dev/dummy_dma_array_device");
		for (i = 0; i < MAX_BUFFERS; i++)
			close(buf_array.fds[i]);
		exit(EXIT_FAILURE);
	}

	/* Pass the array of DMA buffer fds to the driver via ioctl */
	ret = ioctl(driver_fd, MY_DMA_IOCTL_MAP_ARRAY, &buf_array);
	if (ret < 0) {
		perror("MY_DMA_IOCTL_MAP_ARRAY ioctl failed");
	} else {
		printf("DMA buffer array mapping request sent to driver.\n");
	}

	/* Cleanup: close all fds and the driver fd */
	for (i = 0; i < MAX_BUFFERS; i++)
		close(buf_array.fds[i]);
	close(driver_fd);

	return 0;
}
