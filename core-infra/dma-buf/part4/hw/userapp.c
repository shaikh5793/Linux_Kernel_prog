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
#include "dmabuf_example.h"

/**
 * main() - Userspace application to facilitate DMA-BUF sharing.
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
 *   3. Open the importer device (`/dev/importer`).
 *   4. Pass the obtained file descriptor to the importer via another IOCTL call.
 *   5. Clean up by closing all file descriptors.
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

    /* Open the importer device */
    int importer_fd = open("/dev/importer", O_RDWR);
    if (importer_fd < 0) {
        perror("Failed to open /dev/importer");
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
    close(importer_fd);
    close(dmabuf_fd);
    close(exporter_fd);
    return ret < 0 ? 1 : 0;
}
