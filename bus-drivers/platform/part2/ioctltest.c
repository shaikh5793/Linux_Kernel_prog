/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "rtc.h"

int main(void)
{
        int value;
	int fd;

	fd = open("/dev/myrtc", O_RDWR);
	if (fd < 0) {
		perror("open call");
		return -1;
	}

	ioctl(fd, SET_MONTH, 10);
	close(fd);
	return 0;
}
