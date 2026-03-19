#!/bin/bash
# DMA-BUF Part 3: Userspace Memory Mapping
# Demonstrates: mmap, remap_pfn_range, dma_buf_fd, misc device
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
TAG="Exporter\|exporter\|mmap\|buffer\|Buffer"

echo "=== DMA-BUF Part 3: Userspace Memory Mapping ==="
echo ""

echo "[build] Building module and userspace test..."
make -C "$DIR" -s
gcc -Wall -o "$DIR/mmap_test" "$DIR/mmap_test.c"
echo ""

dmesg -C 2>/dev/null || true

echo "[load] Loading exporter with mmap support..."
sudo insmod "$DIR/exporter-mmap.ko"
sleep 0.5

echo "[check] Misc device created:"
ls -l /dev/exporter 2>/dev/null || echo "  /dev/exporter not found!"
echo ""

echo "[run] Running userspace mmap test..."
echo "-----------------------------------------------"
sudo "$DIR/mmap_test" || true
echo "-----------------------------------------------"

echo ""
echo "[dmesg] Kernel messages:"
echo "-----------------------------------------------"
sudo dmesg | grep -i "$TAG" | tail -10
echo "-----------------------------------------------"

echo ""
echo "[cleanup] Unloading module..."
sudo rmmod exporter_mmap 2>/dev/null || true
echo ""
echo "Done. Key points:"
echo "  - dma_buf_fd() converts kernel dma_buf to userspace fd"
echo "  - remap_pfn_range() maps kernel pages into userspace"
echo "  - Userspace reads 'hello world!' via mmap"
