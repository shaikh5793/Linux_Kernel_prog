#!/bin/bash
# SPDX-License-Identifier: MIT OR GPL-2.0-only
# Copyright (c) 2024 TECH VEDA
#
# Netdev Part 1: Userspace Network Device Discovery
# Demonstrates: /sys/class/net/, statistics, queues, ethtool
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"

echo "=== Netdev Part 1: Userspace Network Discovery ==="
echo ""

echo "[build] Building userspace programs..."
make -C "$DIR" -s
echo "  4 programs built OK"
echo ""

echo "[run] net_enum — Interface enumeration:"
echo "-----------------------------------------------"
"$DIR/net_enum"
echo "-----------------------------------------------"
echo ""

echo "[run] net_stats — Interface statistics:"
echo "-----------------------------------------------"
"$DIR/net_stats" lo
echo "-----------------------------------------------"
echo ""

echo "[run] net_queues — Queue configuration:"
echo "-----------------------------------------------"
"$DIR/net_queues" lo
echo "-----------------------------------------------"
echo ""

echo "[run] net_ethtool — Ethtool information:"
echo "-----------------------------------------------"
sudo "$DIR/net_ethtool" 2>/dev/null || "$DIR/net_ethtool" 2>/dev/null || echo "  (needs root for ethtool ioctl)"
echo "-----------------------------------------------"
echo ""

echo "Done. Key points:"
echo "  - /sys/class/net/ has one dir per interface"
echo "  - statistics/ subdir has per-counter files"
echo "  - queues/ shows TX/RX queue config and CPU maps"
echo "  - SIOCETHTOOL ioctl queries driver/link/features"
