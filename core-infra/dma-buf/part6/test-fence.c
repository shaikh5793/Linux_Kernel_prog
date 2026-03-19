/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 *
 * test-fence.c - Userspace DMA-BUF FD mediator with fence (Part 6)
 *
 * Same pipeline as Parts 4-5 (exporter → app → importer), but the
 * exporter starts simulated hardware work when the fd is delivered,
 * and the importer waits for the fence to signal before accessing
 * the buffer.
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/ioctl.h>
#include <errno.h>
#include <string.h>

/* IOCTL commands matching the kernel modules */
#define EXPORTER_FENCE_GET_DMABUF_FD _IOR('E', 3, int)
#define IMPORTER_FENCE_SET_DMABUF_FD _IOW('I', 3, int)

#define BUFFER_SIZE 4096

/**
 * main() - Demonstrate fence-synchronized buffer sharing pipeline
 *
 * Calling Context:
 *   This is the entry point of the userspace test application.
 *
 * Call Chain:
 *   (shell) -> ./test-fence
 *
 * Steps:
 *   1. Open the exporter device (`/dev/exporter-fence`).
 *   2. Get a DMA-BUF fd from the exporter via IOCTL (this also starts
 *      simulated hardware work — the fence will signal in ~1 second).
 *   3. mmap the buffer to test direct userspace access.
 *   4. Open the importer device (`/dev/importer-fence`).
 *   5. Pass the fd to the importer via IOCTL (triggers fence wait,
 *      then DMA and synchronized CPU tests).
 *   6. Clean up all resources.
 *
 * Return: 0 on success, 1 on failure
 */
int main(void)
{
	int exporter_fd, importer_fd, dmabuf_fd;
	void *mapped = NULL;
	int ret = 0;

	printf("=== Part 6: Fence Pipeline ===\n");

	/* Step 1: Open exporter and get DMA-BUF fd (starts hw work) */
	exporter_fd = open("/dev/exporter-fence", O_RDWR);
	if (exporter_fd < 0) {
		perror("open /dev/exporter-fence");
		return 1;
	}

	if (ioctl(exporter_fd, EXPORTER_FENCE_GET_DMABUF_FD, &dmabuf_fd) < 0) {
		perror("ioctl GET_DMABUF_FD");
		close(exporter_fd);
		return 1;
	}
	printf("Got DMA-BUF fd %d — hardware work started (fence in ~1s)\n", dmabuf_fd);

	/* Step 2: mmap for direct userspace access */
	mapped = mmap(NULL, BUFFER_SIZE, PROT_READ, MAP_SHARED, dmabuf_fd, 0);
	if (mapped == MAP_FAILED) {
		perror("mmap");
		mapped = NULL;
	} else {
		printf("mmap read: \"%s\"\n", (char *)mapped);
	}

	/* Step 3: Open importer and pass the fd (waits for fence) */
	importer_fd = open("/dev/importer-fence", O_RDWR);
	if (importer_fd < 0) {
		perror("open /dev/importer-fence");
		ret = 1;
		goto cleanup;
	}

	printf("Passing fd to importer (will block on fence)...\n");
	if (ioctl(importer_fd, IMPORTER_FENCE_SET_DMABUF_FD, &dmabuf_fd) < 0) {
		perror("ioctl SET_DMABUF_FD");
		ret = 1;
		goto cleanup;
	}
	printf("Importer done — fence wait + DMA + CPU tests passed\n");
	printf("Pipeline OK. See dmesg for kernel-side details.\n");

cleanup:
	if (importer_fd >= 0)
		close(importer_fd);
	if (mapped)
		munmap(mapped, BUFFER_SIZE);
	close(dmabuf_fd);
	close(exporter_fd);
	return ret;
}
