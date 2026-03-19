/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * SPDX-License-Identifier: MIT OR GPL-2.0-only
 *
 * net_ethtool.c — Show ethtool-like information via sysfs + ioctl
 *
 * Demonstrates reading network interface details similar to ethtool:
 * driver info, link status, features (offloads), and ring parameters.
 * Uses SIOCETHTOOL ioctl for some data, sysfs for the rest.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>

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

static void show_driver_info(const char *iface)
{
	int fd;
	struct ifreq ifr;
	struct ethtool_drvinfo drvinfo;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0)
		return;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);

	memset(&drvinfo, 0, sizeof(drvinfo));
	drvinfo.cmd = ETHTOOL_GDRVINFO;
	ifr.ifr_data = (void *)&drvinfo;

	if (ioctl(fd, SIOCETHTOOL, &ifr) == 0) {
		printf("    Driver:     %s\n", drvinfo.driver);
		printf("    Version:    %s\n", drvinfo.version);
		printf("    FW version: %s\n", drvinfo.fw_version);
		printf("    Bus info:   %s\n", drvinfo.bus_info);
	} else {
		printf("    (ETHTOOL_GDRVINFO not supported)\n");
	}

	close(fd);
}

static void show_link_settings(const char *iface)
{
	char path[512], buf[64];

	snprintf(path, sizeof(path), "/sys/class/net/%s/speed", iface);
	if (read_sysfs(path, buf, sizeof(buf)) == 0 && atoi(buf) >= 0)
		printf("    Speed:      %s Mbps\n", buf);

	snprintf(path, sizeof(path), "/sys/class/net/%s/duplex", iface);
	if (read_sysfs(path, buf, sizeof(buf)) == 0)
		printf("    Duplex:     %s\n", buf);

	snprintf(path, sizeof(path), "/sys/class/net/%s/carrier", iface);
	if (read_sysfs(path, buf, sizeof(buf)) == 0)
		printf("    Link:       %s\n",
		       atoi(buf) ? "yes" : "no");
}

static void show_features(const char *iface)
{
	char path[512], buf[64];
	static const char *features[] = {
		"tx-checksum-ipv4",
		"tx-checksum-ipv6",
		"scatter-gather",
		"tcp-segmentation-offload",
		"generic-segmentation-offload",
		"generic-receive-offload",
		"rx-checksumming",
	};
	int i;

	printf("    Features (offloads):\n");
	for (i = 0; i < (int)(sizeof(features) / sizeof(features[0])); i++) {
		snprintf(path, sizeof(path),
			 "/sys/class/net/%s/features/%s",
			 iface, features[i]);
		/* features may not be in sysfs for all drivers */
		printf("      %-35s ", features[i]);
		if (read_sysfs(path, buf, sizeof(buf)) == 0)
			printf("%s\n", buf);
		else
			printf("(N/A)\n");
	}
}

static void show_iface(const char *iface)
{
	printf("\n  %s:\n", iface);
	show_driver_info(iface);
	show_link_settings(iface);
	show_features(iface);
}

int main(int argc, char *argv[])
{
	printf("========================================\n");
	printf("  Network Ethtool Information\n");
	printf("========================================\n");

	if (argc > 1) {
		show_iface(argv[1]);
	} else {
		/* Show for common interfaces */
		const char *common[] = {"lo", "eth0", "enp2s0", "wlan0"};
		int i;
		char path[256];

		for (i = 0; i < 4; i++) {
			snprintf(path, sizeof(path),
				 "/sys/class/net/%s", common[i]);
			if (access(path, F_OK) == 0)
				show_iface(common[i]);
		}
	}

	return 0;
}
