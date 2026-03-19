/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

//#define DEV_NAME    "/dev/vDev-0"
#define BUFF_SIZE   2048

char tranfer_buff[BUFF_SIZE];
char receive_buff[BUFF_SIZE];
char DEV_NAME[512];
int user_write(void)
{
	int fd, ret;

	fd = open(DEV_NAME, O_WRONLY);
	if (fd < 0) {
		perror("Failed to open the device\n");
		close(fd);
		return errno;
	}

	printf("Type in a short string to write to device:\n");
	scanf(" %[^\n]%*c", tranfer_buff);

	ret = write(fd, tranfer_buff, BUFF_SIZE);
	if (ret < 0) {
		printf("Failed to write the message to the device\n");
		close(fd);
		return errno;
	}

	close(fd);
	return 0;
}

int user_read(int size)
{
	int fd, ret;
	int total_read = 0, tmp = 0;

	fd = open(DEV_NAME, O_RDONLY);
	if (fd < 0) {
		perror("Failed to open the device\n");
		close(fd);
		return errno;
	}

	while (size) {
		tmp = read(fd, &receive_buff[total_read], size);

		if (!tmp) {
			printf("End of file \n");
			close(fd);
			break;
		} else if (tmp <= size) {
			printf("read %d bytes of data \n", tmp);
			
			total_read += tmp;
			/* We read some data, so decrement 'remaining' */
			size -= tmp;
		} else if (tmp < 0) {
			printf("something went wrong\n");
			close(fd);
			break;
		}
	}

	/* Dump buffer */
	for (int i = 0; i < total_read; i++)
		printf("%c", receive_buff[i]);

	printf("\n");
	close(fd);
	return 0;
}

int main(int argc, char *argv[])
{
	int size;

	if (argc < 3 || strcmp(argv[1], "--help") == 0) {
		printf("Usage: %s <devnode> [read/write] <read size> \n", argv[0]);
		printf("E.g. %s /dev/vDev-1 read 1024\n", argv[0]);
		return 0;
	}

	strcpy(DEV_NAME, argv[1]);
	if (strcmp(argv[2], "write") == 0)
		user_write();

	if (strcmp(argv[2], "read") == 0) {
		size = atoi(argv[3]);
		user_read(size);
	}

	/* Activate this for lseek testing */
#if  0
	ret = lseek(fd, -10, SEEK_SET);
	if (ret < 0) {
		perror("lseek");
		close(fd);
		return ret;
	}
#endif

	return 0;
}
