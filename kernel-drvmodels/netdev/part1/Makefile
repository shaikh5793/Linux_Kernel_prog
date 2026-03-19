# SPDX-License-Identifier: MIT OR GPL-2.0-only
# Copyright (c) 2024 TECH VEDA

CC = gcc
CFLAGS = -Wall -Wextra -D_DEFAULT_SOURCE

PROGS = net_enum net_stats net_queues net_ethtool

all: $(PROGS)

net_enum: net_enum.c
	$(CC) $(CFLAGS) -o $@ $<

net_stats: net_stats.c
	$(CC) $(CFLAGS) -o $@ $<

net_queues: net_queues.c
	$(CC) $(CFLAGS) -o $@ $<

net_ethtool: net_ethtool.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(PROGS)

.PHONY: all clean
