/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#define UIO_DEVICE "/dev/uio0"

int main()
{
    int fd;
    unsigned int irq_count;

    fd = open(UIO_DEVICE, O_RDONLY);
    if (fd < 0)
    {
        perror("Failed to open UIO device");
        return -1;
    }

    while (1)
    {
        read(fd, &irq_count, sizeof(irq_count));
        printf("Interrupt received: %u\n", irq_count);
    }

    close(fd);
    return 0;
}
