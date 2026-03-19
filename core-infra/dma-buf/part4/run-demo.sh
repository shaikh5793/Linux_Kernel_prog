#!/bin/bash
# DMA-BUF Part 4: FD Sharing Pipeline
# Demonstrates: dma_buf_fd(), dma_buf_get(fd), DMA attach/map SG, CPU vmap
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
TAG="Exporter\|Importer\|exporter\|importer\|FD\|fd\|share\|buffer\|DMA\|SG\|CPU\|attach"

echo "=== DMA-BUF Part 4: FD Sharing Pipeline ==="
echo ""

echo "[build] Building modules and userspace app..."
make -C "$DIR" -s
gcc -Wall -o "$DIR/userfd" "$DIR/userfd.c"
echo ""

dmesg -C 2>/dev/null || true

echo "[load] Loading exporter (creates /dev/exporter-share)..."
sudo insmod "$DIR/exporter-fd.ko"
sleep 0.5

echo "[load] Loading importer (creates /dev/importer-share)..."
sudo insmod "$DIR/importer-fd.ko"
sleep 0.5

echo "[run] Running userspace fd mediator..."
echo "-----------------------------------------------"
sudo "$DIR/userfd" || true
echo "-----------------------------------------------"

echo ""
echo "[dmesg] Kernel messages:"
echo "-----------------------------------------------"
sudo dmesg | grep -i "$TAG" | tail -20
echo "-----------------------------------------------"

echo ""
echo "[cleanup] Unloading modules..."
sudo rmmod importer_fd 2>/dev/null || true
sudo rmmod exporter_fd 2>/dev/null || true
rm -f "$DIR/userfd"

echo ""
echo "Done. Key points:"
echo "  - Exporter: dma_buf_fd() creates a userspace-visible file descriptor"
echo "  - Userspace: mediates fd from exporter to importer via ioctl"
echo "  - Importer: dma_buf_get(fd) converts fd back to dma_buf"
echo "  - DMA test: attach → map_attachment → read SG addresses → unmap → detach"
echo "  - CPU test: vmap → read buffer content → vunmap"
echo "  - Zero-copy: same physical page accessed by all three components"
