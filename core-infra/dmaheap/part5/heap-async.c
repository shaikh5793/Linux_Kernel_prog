/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 *
 * heap-async.c - Multi-frame async DMA processing test (Part 5)
 *
 * Userspace test program that allocates a DMA buffer, writes 5 "frames"
 * sequentially, submits each to the importer-async driver, and waits
 * for SIGIO notification of completion before sending the next frame.
 *
 * Key concepts:
 * - sigaction(SIGIO) to install async signal handler
 * - fcntl(F_SETFL, O_ASYNC) + fcntl(F_SETOWN, pid) for async I/O setup
 * - Multi-frame loop with signal-based synchronization
 */

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <linux/dma-heap.h>

#define MY_DMA_IOCTL_PROCESS_ASYNC _IOW('M', 7, int)

volatile sig_atomic_t dma_done = 0;

void sigio_handler(int sig)
{
	dma_done = 1;
	printf("Received SIGIO: DMA processing complete.\n");
}

int main(void)
{
	int heap_fd, driver_fd, ret;
	struct dma_heap_allocation_data alloc_data;
	void *buf;
	struct sigaction sa;

	/* Set up SIGIO handler */
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sigio_handler;
	if (sigaction(SIGIO, &sa, NULL) < 0) {
		perror("sigaction");
		exit(EXIT_FAILURE);
	}

	/* Open the DMA heap device */
	heap_fd = open("/dev/dma_heap/system", O_RDWR | O_CLOEXEC);
	if (heap_fd < 0) {
		perror("Failed to open /dev/dma_heap/system");
		exit(EXIT_FAILURE);
	}

	memset(&alloc_data, 0, sizeof(alloc_data));
	alloc_data.len = 4096;  /* Allocate 4KB */
	alloc_data.fd_flags = O_RDWR | O_CLOEXEC;
	alloc_data.heap_flags = 0;
	ret = ioctl(heap_fd, DMA_HEAP_IOCTL_ALLOC, &alloc_data);
	if (ret < 0) {
		perror("DMA_HEAP_IOCTL_ALLOC failed");
		close(heap_fd);
		exit(EXIT_FAILURE);
	}
	printf("Allocated DMA buffer, fd: %d\n", alloc_data.fd);
	close(heap_fd);  /* FD remains valid */

	/* Map the buffer */
	buf = mmap(NULL, alloc_data.len, PROT_READ | PROT_WRITE, MAP_SHARED, alloc_data.fd, 0);
	if (buf == MAP_FAILED) {
		perror("mmap failed");
		close(alloc_data.fd);
		exit(EXIT_FAILURE);
	}

	/* Open the driver device */
	driver_fd = open("/dev/dummy_dma_async_device", O_RDWR);
	if (driver_fd < 0) {
		perror("Failed to open /dev/dummy_dma_async_device");
		munmap(buf, alloc_data.len);
		close(alloc_data.fd);
		exit(EXIT_FAILURE);
	}

	/* Set asynchronous mode on the driver fd */
	ret = fcntl(driver_fd, F_SETFL, O_ASYNC);
	if (ret < 0)
		perror("fcntl F_SETFL failed");
	ret = fcntl(driver_fd, F_SETOWN, getpid());
	if (ret < 0)
		perror("fcntl F_SETOWN failed");

	/* Loop: write frame strings, signal driver, wait for processing, then write next frame */
	for (int i = 1; i <= 5; i++) {
		char frame_str[64];
		snprintf(frame_str, sizeof(frame_str), "frame%d", i);
		strncpy((char *)buf, frame_str, alloc_data.len);
		printf("User wrote: %s\n", (char *)buf);

		dma_done = 0;
		ret = ioctl(driver_fd, MY_DMA_IOCTL_PROCESS_ASYNC, &alloc_data.fd);
		if (ret < 0) {
			perror("MY_DMA_IOCTL_PROCESS_ASYNC ioctl failed");
		}

		/* Wait for the SIGIO notification from driver */
		while (!dma_done) {
			usleep(10000);  /* sleep 10ms */
		}
		printf("Driver processed frame%d and signaled completion.\n", i);
		printf("Preparing to write next frame...\n");
		sleep(1);
	}

	munmap(buf, alloc_data.len);
	close(driver_fd);
	close(alloc_data.fd);
	return 0;
}
