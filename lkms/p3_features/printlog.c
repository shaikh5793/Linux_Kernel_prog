/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

#include <linux/kernel.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/klog.h>
#include <stdlib.h>
#include <stdio.h>


#define KMSG_PATH "/dev/kmsg"
#define KLOG_BUF_SIZE (16 * 1024)

/*
 * read_from_kmsg - Reads kernel logs from the modern /dev/kmsg interface.
 *
 * This is the modern, preferred method for accessing kernel logs. It provides
 * structured messages, where metadata such as log level, timestamp, and
 * sequence number are provided separately from the message itself. This makes
 * it easy to parse and filter messages programmatically. The /dev/kmsg
 * interface provides a continuous stream of logs as they are generated.
 * This function demonstrates how to parse this structured data.
 */
static void read_from_kmsg(void)
{
	int fd;
	char buffer[2048];
	ssize_t nbytes;

	/*
	 * Open the kernel log device. O_NONBLOCK can be used for non-blocking reads.
	 */
	fd = open(KMSG_PATH, O_RDONLY);
	if (fd < 0) {
		perror("Failed to open " KMSG_PATH);
		exit(EXIT_FAILURE);
	}

	printf("Reading and parsing kernel messages from %s. Press Ctrl+C to exit.\n",
	       KMSG_PATH);

	while ((nbytes = read(fd, buffer, sizeof(buffer) - 1)) > 0) {
		char *msg, *p;
		unsigned long level, seq;
		long long timestamp_us;

		buffer[nbytes] = '\0';

		/*
		 * The format from /dev/kmsg is a comma-separated string:
		 * "level,sequence,timestamp_us,flags;message\n"
		 * We parse this to extract the metadata.
		 */
		p = buffer;
		msg = strchr(p, ';');
		if (msg) {
			*msg = '\0'; /* Terminate the prefix part */
			msg++; /* The actual message starts after the semicolon */

			if (sscanf(p, "%lu,%lu,%lld,", &level, &seq, &timestamp_us) == 3) {
				/* Trim trailing newline from the message part */
				char *newline = strchr(msg, '\n');
				if (newline)
					*newline = '\0';

				printf("[L:%lu S:%lu T:%lld us] %s\n",
				       level, seq, timestamp_us, msg);
			} else {
				/* If parsing fails, print the raw line for debugging */
				printf("RAW: %s;%s", p, msg);
			}
		} else {
			/* Fallback for messages without a prefix */
			printf("RAW: %s", buffer);
		}
	}

	if (nbytes < 0)
		perror("Failed to read from " KMSG_PATH);

	close(fd);
}

/*
 * read_with_klogctl - Reads kernel logs using the legacy klogctl() syscall.
 *
 * This is the older, legacy method. It provides a raw, unstructured dump of
 * the kernel's ring buffer. Log metadata (like level and timestamp) is
 * embedded within the message string itself (e.g., "<6>..."), requiring
 * manual, and often brittle, string parsing. This function demonstrates how
 * to take a single snapshot and parse it, similar to dmesg.
 */
static void read_with_klogctl(void)
{
	char *buffer, *line, *p, *saveptr;
	int nbytes;
	unsigned long level;
	double timestamp;

	buffer = malloc(KLOG_BUF_SIZE);
	if (!buffer) {
		perror("Failed to allocate memory for klog buffer");
		exit(EXIT_FAILURE);
	}

	printf("Taking a snapshot of kernel logs using klogctl:\n");

	/* SYSLOG_ACTION_READ_ALL: Read all messages since last read */
	nbytes = klogctl(3, buffer, KLOG_BUF_SIZE - 1);
	if (nbytes < 0) {
		perror("klogctl failed");
		free(buffer);
		exit(EXIT_FAILURE);
	}
	buffer[nbytes] = '\0';

	/*
	 * Parse the buffer line by line. The format is typically:
	 * <level>[ timestamp] message
	 * We use strtok_r for safe, re-entrant tokenizing.
	 */
	for (p = buffer; ; p = NULL) {
		line = strtok_r(p, "\n", &saveptr);
		if (line == NULL)
			break;

		/* Skip empty lines */
		if (*line == '\0')
			continue;

		/*
		 * Attempt to parse the log level and timestamp.
		 * sscanf is simple, but can be tricky. We check its return value.
		 * The format string looks for '<level>[ timestamp]'.
		 * The '%*c' consumes the space after the timestamp.
		 */
		if (sscanf(line, "<%lu>[%lf]%*c", &level, &timestamp) == 2) {
			char *msg = strchr(line, ']');
			if (msg && *(++msg) == ' ') {
				printf("[%10.6f] %s\n", timestamp, msg + 1);
			} else {
				/* Fallback for lines that parse but don't have the expected structure */
				printf("%s\n", line);
			}
		} else {
			/* If parsing fails, print the raw line */
			printf("%s\n", line);
		}
	}

	free(buffer);
}

int main(int argc, char *argv[])
{
	if (argc > 1 && strcmp(argv[1], "--klogctl") == 0) {
		read_with_klogctl();
	} else {
		read_from_kmsg();
	}

	return EXIT_SUCCESS;
}
