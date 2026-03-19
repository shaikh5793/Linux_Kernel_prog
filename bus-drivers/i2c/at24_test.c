/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "at24_ioctl.h"

int main() {
    int fd = open("/dev/eeprom0", O_RDWR);
    if (fd < 0) {
        perror("Failed to open the device");
        return 1;
    }

    unsigned int page_offset = 1;
    if (ioctl(fd, EEPROM_IOCTL_SET_PAGE_OFFSET, &page_offset) < 0) {
        perror("Failed to set page offset");
        close(fd);
        return 1;
    }

    char read_buf[32];
    if (ioctl(fd, EEPROM_IOCTL_PAGE_READ, read_buf) < 0) {
        perror("Failed to read page");
        close(fd);
        return 1;
    }

    printf("Page data: %s\n", read_buf);

    char write_buf[32] = "hello techveda";
    if (ioctl(fd, EEPROM_IOCTL_PAGE_WRITE, write_buf) < 0) {
        perror("Failed to write page");
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}
