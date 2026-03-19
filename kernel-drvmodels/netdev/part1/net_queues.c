/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * SPDX-License-Identifier: MIT OR GPL-2.0-only
 *
 * net_queues.c — Explore TX/RX queue configuration
 *
 * Shows per-queue details: XPS/RPS CPU affinity, byte/packet counts,
 * queue lengths, and ring buffer parameters from sysfs.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>

static int read_sysfs(const char *path, char *buf, int buflen)
{
	FILE *fp = fopen(path, "r");

	if (!fp)
		return -1;
	if (!fgets(buf, buflen, fp)) {
		fclose(fp);
		return -1;
	}
	fclose(fp);
	buf[strcspn(buf, "\n")] = '\0';
	return 0;
}

static void show_queues(const char *iface)
{
	char basepath[512], path[1024], buf[256];
	DIR *dir;
	struct dirent *entry;
	int tx_count = 0, rx_count = 0;

	printf("\n  %s:\n", iface);

	/* TX queues */
	snprintf(basepath, sizeof(basepath),
		 "/sys/class/net/%s/queues", iface);
	dir = opendir(basepath);
	if (!dir) {
		printf("    (no queues directory)\n");
		return;
	}

	printf("    TX Queues:\n");
	while ((entry = readdir(dir)) != NULL) {
		if (strncmp(entry->d_name, "tx-", 3) != 0)
			continue;

		printf("      %s:", entry->d_name);

		/* XPS CPU map */
		snprintf(path, sizeof(path), "%s/%s/xps_cpus",
			 basepath, entry->d_name);
		if (read_sysfs(path, buf, sizeof(buf)) == 0)
			printf("  xps_cpus=%s", buf);

		/* TX bytes */
		snprintf(path, sizeof(path), "%s/%s/tx_bytes",
			 basepath, entry->d_name);
		if (read_sysfs(path, buf, sizeof(buf)) == 0)
			printf("  bytes=%s", buf);

		/* TX packets */
		snprintf(path, sizeof(path), "%s/%s/tx_packets",
			 basepath, entry->d_name);
		if (read_sysfs(path, buf, sizeof(buf)) == 0)
			printf("  pkts=%s", buf);

		printf("\n");
		tx_count++;
	}
	closedir(dir);

	/* RX queues */
	dir = opendir(basepath);
	if (!dir)
		return;

	printf("    RX Queues:\n");
	while ((entry = readdir(dir)) != NULL) {
		if (strncmp(entry->d_name, "rx-", 3) != 0)
			continue;

		printf("      %s:", entry->d_name);

		/* RPS CPU map */
		snprintf(path, sizeof(path), "%s/%s/rps_cpus",
			 basepath, entry->d_name);
		if (read_sysfs(path, buf, sizeof(buf)) == 0)
			printf("  rps_cpus=%s", buf);

		/* RPS flow count */
		snprintf(path, sizeof(path), "%s/%s/rps_flow_cnt",
			 basepath, entry->d_name);
		if (read_sysfs(path, buf, sizeof(buf)) == 0)
			printf("  flows=%s", buf);

		printf("\n");
		rx_count++;
	}
	closedir(dir);

	printf("    Summary: %d TX queues, %d RX queues\n",
	       tx_count, rx_count);

	/* TX queue length */
	snprintf(path, sizeof(path),
		 "/sys/class/net/%s/tx_queue_len", iface);
	if (read_sysfs(path, buf, sizeof(buf)) == 0)
		printf("    TX queue length: %s\n", buf);
}

int main(int argc, char *argv[])
{
	DIR *dir;
	struct dirent *entry;

	printf("========================================\n");
	printf("  Network Queue Configuration\n");
	printf("========================================\n");

	if (argc > 1) {
		show_queues(argv[1]);
	} else {
		dir = opendir("/sys/class/net");
		if (!dir) {
			printf("\n  [/sys/class/net not found]\n");
			return 1;
		}

		while ((entry = readdir(dir)) != NULL) {
			if (entry->d_name[0] == '.')
				continue;
			show_queues(entry->d_name);
		}
		closedir(dir);
	}

	return 0;
}
