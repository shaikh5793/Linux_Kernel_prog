/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 *
 * userfd.c - Userspace DMA-BUF FD mediator (Part 4)
 *
 * Why userspace?  In real systems the exporter (e.g. V4L2 camera) and the
 * importer (e.g. DRM display) are independent subsystems — they cannot
 * EXPORT_SYMBOL to each other because they are maintained by different
 * vendors and compiled separately.  Userspace is the only entity that
 * knows the full pipeline topology: it obtains an fd from the producer
 * and hands it to the consumer, just like a video player or Wayland
 * compositor does in production.  The fd can also cross process
 * boundaries via Unix domain sockets (SCM_RIGHTS).
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/ioctl.h>
#include <errno.h>
#include <string.h>

/* IOCTL commands matching the kernel modules */
#define EXPORTER_SHARE_GET_DMABUF_FD _IOR('E', 1, int)
#define IMPORTER_SHARE_SET_DMABUF_FD _IOW('I', 1, int)

/**
 * main() - Demonstrate FD sharing pipeline
 *
 * Calling Context:
 *   This is the entry point of the userspace test application.
 *
 * Call Chain:
 *   (shell) -> ./userfd
 *
 * Steps:
 *   1. Open the exporter device (`/dev/exporter-share`).
 *   2. Get a DMA-BUF file descriptor from the exporter via IOCTL.
 *   3. Open the importer device (`/dev/importer-share`).
 *   4. Pass the fd to the importer via IOCTL.
 *   5. Clean up by closing all file descriptors.
 *
 * Return: 0 on success, 1 on failure
 */
int main(void)
{
	int exporter_fd, importer_fd, dmabuf_fd;
	int ret = 0;

	printf("=== Part 4: FD Sharing Pipeline ===\n");

	/* Step 1: Open exporter and get DMA-BUF fd */
	exporter_fd = open("/dev/exporter-share", O_RDWR);
	if (exporter_fd < 0) {
		perror("open /dev/exporter-share");
		return 1;
	}

	if (ioctl(exporter_fd, EXPORTER_SHARE_GET_DMABUF_FD, &dmabuf_fd) < 0) {
		perror("ioctl GET_DMABUF_FD");
		close(exporter_fd);
		return 1;
	}
	printf("Got DMA-BUF fd %d from exporter\n", dmabuf_fd);

	/* Step 2: Open importer and pass the fd */
	importer_fd = open("/dev/importer-share", O_RDWR);
	if (importer_fd < 0) {
		perror("open /dev/importer-share");
		ret = 1;
		goto cleanup;
	}

	if (ioctl(importer_fd, IMPORTER_SHARE_SET_DMABUF_FD, &dmabuf_fd) < 0) {
		perror("ioctl SET_DMABUF_FD");
		ret = 1;
		goto cleanup;
	}
	printf("Passed fd to importer — DMA and CPU tests triggered\n");
	printf("Pipeline OK. See dmesg for kernel-side details.\n");

cleanup:
	if (importer_fd >= 0)
		close(importer_fd);
	close(dmabuf_fd);
	close(exporter_fd);
	return ret;
}
