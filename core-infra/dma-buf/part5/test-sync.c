/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 *
 * test-sync.c - Userspace DMA-BUF FD mediator with sync (Part 5)
 *
 * Same pipeline as Part 4 (exporter → app → importer), but the
 * importer now wraps CPU access with begin/end_cpu_access for
 * cache coherency.  This app also mmaps the buffer to demonstrate
 * direct userspace access alongside the kernel-side tests.
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
#define EXPORTER_SYNC_GET_DMABUF_FD _IOR('E', 2, int)
#define IMPORTER_SYNC_SET_DMABUF_FD _IOW('I', 2, int)

#define BUFFER_SIZE 4096

/**
 * main() - Demonstrate synchronized buffer sharing pipeline
 *
 * Calling Context:
 *   This is the entry point of the userspace test application.
 *
 * Call Chain:
 *   (shell) -> ./test-sync
 *
 * Steps:
 *   1. Open the exporter device (`/dev/exporter-sync`).
 *   2. Get a DMA-BUF file descriptor from the exporter via IOCTL.
 *   3. mmap the buffer to test direct userspace access.
 *   4. Open the importer device (`/dev/importer-sync`).
 *   5. Pass the fd to the importer via IOCTL (triggers kernel-side
 *      DMA and synchronized CPU tests).
 *   6. Clean up all resources.
 *
 * Return: 0 on success, 1 on failure
 */
int main(void)
{
	int exporter_fd, importer_fd, dmabuf_fd;
	void *mapped = NULL;
	int ret = 0;

	printf("=== Part 5: Sync Pipeline ===\n");

	/* Step 1: Open exporter and get DMA-BUF fd */
	exporter_fd = open("/dev/exporter-sync", O_RDWR);
	if (exporter_fd < 0) {
		perror("open /dev/exporter-sync");
		return 1;
	}

	if (ioctl(exporter_fd, EXPORTER_SYNC_GET_DMABUF_FD, &dmabuf_fd) < 0) {
		perror("ioctl GET_DMABUF_FD");
		close(exporter_fd);
		return 1;
	}
	printf("Got DMA-BUF fd %d from exporter\n", dmabuf_fd);

	/* Step 2: mmap for direct userspace access */
	mapped = mmap(NULL, BUFFER_SIZE, PROT_READ, MAP_SHARED, dmabuf_fd, 0);
	if (mapped == MAP_FAILED) {
		perror("mmap");
		mapped = NULL;
	} else {
		printf("mmap read: \"%s\"\n", (char *)mapped);
	}

	/* Step 3: Open importer and pass the fd */
	importer_fd = open("/dev/importer-sync", O_RDWR);
	if (importer_fd < 0) {
		perror("open /dev/importer-sync");
		ret = 1;
		goto cleanup;
	}

	if (ioctl(importer_fd, IMPORTER_SYNC_SET_DMABUF_FD, &dmabuf_fd) < 0) {
		perror("ioctl SET_DMABUF_FD");
		ret = 1;
		goto cleanup;
	}
	printf("Passed fd to importer — DMA and synced CPU tests triggered\n");
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
