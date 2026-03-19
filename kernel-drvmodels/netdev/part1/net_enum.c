/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * SPDX-License-Identifier: MIT OR GPL-2.0-only
 *
 * net_enum.c — Enumerate all network interfaces from /sys/class/net/
 *
 * Shows: interface name, type, MAC address, MTU, operstate, speed,
 * carrier, flags, and driver name.
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

static const char *iftype_name(int type)
{
	switch (type) {
	case 1:   return "Ethernet";
	case 24:  return "Loopback";
	case 772: return "Loopback";
	case 768: return "Tunnel";
	case 776: return "IPv6-in-IPv4";
	case 65534: return "None";
	default:  return "Other";
	}
}

static void show_iface(const char *name)
{
	char path[512], buf[256];
	int type = 0;

	printf("\n  %s:\n", name);

	/* Interface type */
	snprintf(path, sizeof(path), "/sys/class/net/%s/type", name);
	if (read_sysfs(path, buf, sizeof(buf)) == 0) {
		type = atoi(buf);
		printf("    Type:      %d (%s)\n", type, iftype_name(type));
	}

	/* MAC address */
	snprintf(path, sizeof(path), "/sys/class/net/%s/address", name);
	if (read_sysfs(path, buf, sizeof(buf)) == 0)
		printf("    MAC:       %s\n", buf);

	/* MTU */
	snprintf(path, sizeof(path), "/sys/class/net/%s/mtu", name);
	if (read_sysfs(path, buf, sizeof(buf)) == 0)
		printf("    MTU:       %s\n", buf);

	/* Operstate */
	snprintf(path, sizeof(path), "/sys/class/net/%s/operstate", name);
	if (read_sysfs(path, buf, sizeof(buf)) == 0)
		printf("    State:     %s\n", buf);

	/* Carrier */
	snprintf(path, sizeof(path), "/sys/class/net/%s/carrier", name);
	if (read_sysfs(path, buf, sizeof(buf)) == 0)
		printf("    Carrier:   %s\n", buf);

	/* Speed (only for Ethernet) */
	snprintf(path, sizeof(path), "/sys/class/net/%s/speed", name);
	if (read_sysfs(path, buf, sizeof(buf)) == 0 && atoi(buf) > 0)
		printf("    Speed:     %s Mbps\n", buf);

	/* TX/RX queue count */
	snprintf(path, sizeof(path),
		 "/sys/class/net/%s/queues", name);
	if (access(path, F_OK) == 0) {
		DIR *qdir = opendir(path);
		int tx = 0, rx = 0;

		if (qdir) {
			struct dirent *qe;

			while ((qe = readdir(qdir)) != NULL) {
				if (strncmp(qe->d_name, "tx-", 3) == 0)
					tx++;
				if (strncmp(qe->d_name, "rx-", 3) == 0)
					rx++;
			}
			closedir(qdir);
			printf("    Queues:    TX=%d RX=%d\n", tx, rx);
		}
	}

	/* Driver */
	snprintf(path, sizeof(path),
		 "/sys/class/net/%s/device/driver", name);
	if (access(path, F_OK) == 0) {
		char link[512];
		ssize_t len = readlink(path, link, sizeof(link) - 1);

		if (len > 0) {
			link[len] = '\0';
			char *drv = strrchr(link, '/');

			printf("    Driver:    %s\n",
			       drv ? drv + 1 : link);
		}
	}
}

int main(void)
{
	DIR *dir;
	struct dirent *entry;
	int count = 0;

	printf("========================================\n");
	printf("  Network Interface Enumeration\n");
	printf("========================================\n");

	dir = opendir("/sys/class/net");
	if (!dir) {
		printf("\n  [/sys/class/net not found]\n");
		return 1;
	}

	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_name[0] == '.')
			continue;
		show_iface(entry->d_name);
		count++;
	}
	closedir(dir);

	printf("\n  Total interfaces: %d\n\n", count);
	return 0;
}
