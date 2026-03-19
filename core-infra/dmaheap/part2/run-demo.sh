#!/bin/bash
# DMA Heap Part 2: Array of DMA Buffers
# Demonstrates: multiple buffer allocation, array ioctl, per-buffer DMA mapping
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
TAG="dummy_dma_array"

echo "=== DMA Heap Part 2: Array of DMA Buffers ==="
echo ""

echo "[build] Building module and userspace test..."
make -C "$DIR" -s
echo "  importer-array.ko and heap-array built OK"
echo ""

dmesg -C 2>/dev/null || true

echo "[load] Loading array driver..."
sudo insmod "$DIR/importer-array.ko"
sleep 0.5

echo "[run] Allocating 4 buffers and passing to driver..."
echo "-----------------------------------------------"
sudo "$DIR/heap-array" || true
echo "-----------------------------------------------"

echo ""
echo "[dmesg] Kernel messages:"
echo "-----------------------------------------------"
sudo dmesg | grep "$TAG" | tail -15
echo "-----------------------------------------------"

echo ""
echo "[cleanup] Unloading module..."
sudo rmmod importer_array 2>/dev/null || true
echo ""
echo "Done. Key points:"
echo "  - 4 separate dma_buf allocations, each with its own DMA address"
echo "  - Driver loops: get/attach/map for each buffer"
echo "  - Analogous to multi-plane video buffers (Y/U/V)"
