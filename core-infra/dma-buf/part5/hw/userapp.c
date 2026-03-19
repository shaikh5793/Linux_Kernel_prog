/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/dma-buf.h>
#include "dmabuf_example.h"

#define BUFFER_SIZE 4096

/**
 * main() - Userspace application to facilitate DMA-BUF sharing with synchronization.
 *
 * Calling Context:
 *   This is the entry point of the userspace test application.
 *
 * Call Chain:
 *   (shell) -> ./userapp
 *
 * Steps to be handled:
 *   1. Open the exporter device (`/dev/exporter`).
 *   2. Get a DMA-BUF file descriptor from the exporter via an IOCTL call.
 *   3. Map the DMA-BUF into userspace using `mmap()`.
 *   4. Perform `DMA_BUF_IOCTL_SYNC` with `DMA_BUF_SYNC_START | DMA_BUF_SYNC_WRITE`
 *      to synchronize before CPU write access.
 *   5. Write data to the mapped buffer.
 *   6. Perform `DMA_BUF_IOCTL_SYNC` with `DMA_BUF_SYNC_END | DMA_BUF_SYNC_WRITE`
 *      to synchronize after CPU write access.
 *   7. Open the importer device (`/dev/importer`).
 *   8. Pass the obtained file descriptor to the importer via another IOCTL call.
 *   9. Clean up by unmapping memory and closing all file descriptors.
 *
 * Return: 0 on success, 1 on failure.
 */int main(void)
{
    /* Open the exporter device */
    int exporter_fd = open("/dev/exporter", O_RDWR);
    if (exporter_fd < 0) {
        perror("Failed to open /dev/exporter");
        return 1;
    }

    /* Get the DMA-BUF fd from the exporter */
    int dmabuf_fd = ioctl(exporter_fd, EXPORTER_IOC_GET_DMABUF, 0);
    if (dmabuf_fd < 0) {
        perror("Failed to get DMA-BUF fd from exporter");
        close(exporter_fd);
        return 1;
    }
    printf("Received DMA-BUF fd: %d\n", dmabuf_fd);

    /* Map the DMA-BUF into user-space */
    void *ptr = mmap(NULL, BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, dmabuf_fd, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap failed");
        close(dmabuf_fd);
        close(exporter_fd);
        return 1;
    }

    /* Synchronize before CPU write access */
    struct dma_buf_sync sync_start = { .flags = DMA_BUF_SYNC_START | DMA_BUF_SYNC_WRITE };
    if (ioctl(dmabuf_fd, DMA_BUF_IOCTL_SYNC, &sync_start) < 0) {
        perror("DMA_BUF_IOCTL_SYNC start failed");
        munmap(ptr, BUFFER_SIZE);
        close(dmabuf_fd);
        close(exporter_fd);
        return 1;
    }

    /* Write data to the buffer */
    memset(ptr, 0xAA, BUFFER_SIZE); /* Example: fill with 0xAA */
    printf("User-space: Wrote 0xAA to the buffer\n");

    /* Synchronize after CPU write access */
    struct dma_buf_sync sync_end = { .flags = DMA_BUF_SYNC_END | DMA_BUF_SYNC_WRITE };
    if (ioctl(dmabuf_fd, DMA_BUF_IOCTL_SYNC, &sync_end) < 0) {
        perror("DMA_BUF_IOCTL_SYNC end failed");
        munmap(ptr, BUFFER_SIZE);
        close(dmabuf_fd);
        close(exporter_fd);
        return 1;
    }

    /* Open the importer device */
    int importer_fd = open("/dev/importer", O_RDWR);
    if (importer_fd < 0) {
        perror("Failed to open /dev/importer");
        munmap(ptr, BUFFER_SIZE);
        close(dmabuf_fd);
        close(exporter_fd);
        return 1;
    }

    /* Pass the DMA-BUF fd to the importer */
    int ret = ioctl(importer_fd, IMPORTER_IOC_SET_DMABUF, dmabuf_fd);
    if (ret < 0) {
        perror("Failed to set DMA-BUF in importer");
    } else {
        printf("Successfully passed DMA-BUF to importer\n");
    }

    /* Cleanup */
    munmap(ptr, BUFFER_SIZE);
    close(importer_fd);
    close(dmabuf_fd);
    close(exporter_fd);
    return ret < 0 ? 1 : 0;
}
