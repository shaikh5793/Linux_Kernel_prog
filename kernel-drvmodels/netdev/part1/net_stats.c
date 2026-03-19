/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * SPDX-License-Identifier: MIT OR GPL-2.0-only
 *
 * net_stats.c — Show network interface statistics
 *
 * Reads per-interface statistics from /sys/class/net/<iface>/statistics/
 * and /proc/net/dev for a side-by-side comparison of both methods.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

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

static void show_sysfs_stats(const char *iface)
{
	char path[512], buf[64];
	static const char *stats[] = {
		"rx_bytes", "rx_packets", "rx_errors", "rx_dropped",
		"tx_bytes", "tx_packets", "tx_errors", "tx_dropped",
		"collisions", "multicast",
	};
	int i;

	printf("\n  %s (sysfs statistics/):\n", iface);
	printf("    %-20s %s\n", "Statistic", "Value");
	printf("    %-20s %s\n", "--------------------", "----------");

	for (i = 0; i < (int)(sizeof(stats) / sizeof(stats[0])); i++) {
		snprintf(path, sizeof(path),
			 "/sys/class/net/%s/statistics/%s", iface, stats[i]);
		if (read_sysfs(path, buf, sizeof(buf)) == 0)
			printf("    %-20s %s\n", stats[i], buf);
	}
}

static void show_proc_stats(void)
{
	FILE *fp;
	char line[512];
	int header = 0;

	fp = fopen("/proc/net/dev", "r");
	if (!fp)
		return;

	printf("\n  /proc/net/dev:\n");
	while (fgets(line, sizeof(line), fp)) {
		if (header < 2) {
			header++;
			printf("    %s", line);
			continue;
		}
		printf("    %s", line);
	}
	fclose(fp);
}

int main(int argc, char *argv[])
{
	DIR *dir;
	struct dirent *entry;

	printf("========================================\n");
	printf("  Network Interface Statistics\n");
	printf("========================================\n");

	if (argc > 1) {
		/* Show stats for specific interface */
		show_sysfs_stats(argv[1]);
	} else {
		/* Show stats for all interfaces */
		dir = opendir("/sys/class/net");
		if (!dir) {
			printf("\n  [/sys/class/net not found]\n");
			return 1;
		}

		while ((entry = readdir(dir)) != NULL) {
			if (entry->d_name[0] == '.')
				continue;
			show_sysfs_stats(entry->d_name);
		}
		closedir(dir);
	}

	show_proc_stats();

	return 0;
}
