/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

/*
 * w25q32_test.c - Test application for the W25Q32 SPI flash char driver
 *
 * This application uses read, write, and ioctl calls to test the functionality
 * of the flash driver. It supports the following commands:
 *
 *   read <size>                - Read <size> bytes from flash (from current offset)
 *   write <string>             - Write the given string to flash (at current offset)
 *   set_offset <b>:<s>:<p>     - Set flash offset using block, sector, and page values
 *   get_offset                 - Get and print the current flash offset
 *   erase <b>:<s>              - Erase flash sector (using block and sector values)
 *
 * Compile with:
 *     gcc -Wall -o w25q32_test w25q32_test.c
 *
 * Usage:
 *     ./w25q32_test <command> [options]
 *
 * Example:
 *     ./w25q32_test set_offset 1:0:0
 *     ./w25q32_test write "Hello World!"
 *     ./w25q32_test get_offset
 *     ./w25q32_test read 16
 *     ./w25q32_test erase 1:0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>

/* 
 * IOCTL command definitions.
 * These macros must match those defined in the kernel driver.
 */
#define W25_IOC_MAGIC       'W'
#define W25_IOC_SET_OFFSET  _IOW(W25_IOC_MAGIC, 1, struct w25_offset)
#define W25_IOC_GET_OFFSET  _IOR(W25_IOC_MAGIC, 2, unsigned int)
#define W25_IOC_ERASE       _IOW(W25_IOC_MAGIC, 3, struct w25_erase)

/*
 * Structure definitions used for the IOCTL calls.
 * These structures must exactly match the definitions in the driver.
 */
struct w25_offset {
    unsigned int block;
    unsigned int sector;
    unsigned int page;
};

struct w25_erase {
    unsigned int block;
    unsigned int sector;
};

/* 
 * Define the device node path for the flash driver.
 */
#define DEVICE_PATH "/dev/w25q32"

/*
 * usage() - Prints usage information for this test application.
 *
 * @prog: The name of the executable.
 */
static void usage(const char *prog)
{
    printf("Usage: %s <command> [options]\n", prog);
    printf("Commands:\n");
    printf("  read <size>                - Read <size> bytes from current offset and display in hex\n");
    printf("  write <string>             - Write the provided string to flash at current offset\n");
    printf("  set_offset <b>:<s>:<p>     - Set flash offset (block, sector, page)\n");
    printf("  get_offset                 - Get current flash offset\n");
    printf("  erase <b>:<s>              - Erase flash sector at given block and sector\n");
}

int main(int argc, char *argv[])
{
    int fd, ret;
    
    /* Verify that at least one command-line argument is provided */
    if (argc < 2) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    
    /* Open the flash device for read/write access */
    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("open");
        return EXIT_FAILURE;
    }
    
    /*
     * Process commands based on the first argument.
     * The supported commands are: read, write, set_offset, get_offset, and erase.
     */
    if (strcmp(argv[1], "read") == 0) {
        /* --- Read Command ---
         * Reads a specified number of bytes from the flash device and prints
         * the bytes in hexadecimal format.
         */
        if (argc != 3) {
            usage(argv[0]);
            close(fd);
            return EXIT_FAILURE;
        }
        size_t size = atoi(argv[2]);
        unsigned char *buffer = malloc(size);
        if (!buffer) {
            perror("malloc");
            close(fd);
            return EXIT_FAILURE;
        }
        ret = read(fd, buffer, size);
        if (ret < 0) {
            perror("read");
        } else {
            printf("Read %d bytes:\n", ret);
            for (int i = 0; i < ret; i++) {
                printf("%02x ", buffer[i]);
                if ((i + 1) % 16 == 0)
                    printf("\n");
            }
            if (ret % 16)
                printf("\n");
        }
        free(buffer);
    } else if (strcmp(argv[1], "write") == 0) {
        /* --- Write Command ---
         * Writes the provided string to the flash device at the current offset.
         */
        if (argc != 3) {
            usage(argv[0]);
            close(fd);
            return EXIT_FAILURE;
        }
        size_t len = strlen(argv[2]);
        ret = write(fd, argv[2], len);
        if (ret < 0) {
            perror("write");
        } else {
            printf("Wrote %d bytes to flash\n", ret);
        }
    } else if (strcmp(argv[1], "set_offset") == 0) {
        /* --- Set Offset Command ---
         * Sets the flash offset by parsing a string in the format <block>:<sector>:<page>.
         */
        if (argc != 3) {
            usage(argv[0]);
            close(fd);
            return EXIT_FAILURE;
        }
        struct w25_offset off;
        if (sscanf(argv[2], "%u:%u:%u", &off.block, &off.sector, &off.page) != 3) {
            fprintf(stderr, "Invalid offset format. Expected <block>:<sector>:<page>\n");
            close(fd);
            return EXIT_FAILURE;
        }
        ret = ioctl(fd, W25_IOC_SET_OFFSET, &off);
        if (ret < 0) {
            perror("ioctl set_offset");
        } else {
            printf("Flash offset set successfully\n");
        }
    } else if (strcmp(argv[1], "get_offset") == 0) {
        /* --- Get Offset Command ---
         * Retrieves and prints the current flash offset from the driver.
         */
        unsigned int offset;
        ret = ioctl(fd, W25_IOC_GET_OFFSET, &offset);
        if (ret < 0) {
            perror("ioctl get_offset");
        } else {
            printf("Current flash offset: 0x%06x\n", offset);
        }
    } else if (strcmp(argv[1], "erase") == 0) {
        /* --- Erase Command ---
         * Erases a flash sector specified by block and sector values.
         */
        if (argc != 3) {
            usage(argv[0]);
            close(fd);
            return EXIT_FAILURE;
        }
        struct w25_erase er;
        if (sscanf(argv[2], "%u:%u", &er.block, &er.sector) != 2) {
            fprintf(stderr, "Invalid erase format. Expected <block>:<sector>\n");
            close(fd);
            return EXIT_FAILURE;
        }
        ret = ioctl(fd, W25_IOC_ERASE, &er);
        if (ret < 0) {
            perror("ioctl erase");
        } else {
            printf("Flash sector erased successfully\n");
        }
    } else {
        /* Unrecognized command */
        usage(argv[0]);
    }
    
    /* Close the device file */
    close(fd);
    return EXIT_SUCCESS;
}

