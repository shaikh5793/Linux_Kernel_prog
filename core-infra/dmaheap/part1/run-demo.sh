#!/bin/bash
# DMA Heap Part 1: Basic Import and Mapping
# Demonstrates: DMA_HEAP_IOCTL_ALLOC, dma_buf_get, attach, map_attachment
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
TAG="dummy_dma"

echo "=== DMA Heap Part 1: Basic Import and Mapping ==="
echo ""

echo "[build] Building module and userspace test..."
make -C "$DIR" -s
echo "  importer-map.ko and heap-alloc built OK"
echo ""

dmesg -C 2>/dev/null || true

echo "[load] Loading driver (creates /dev/dummy_dma_map_device)..."
sudo insmod "$DIR/importer-map.ko"
sleep 0.5

echo "[run] Running userspace test (allocates from heap, passes to driver)..."
echo "-----------------------------------------------"
sudo "$DIR/heap-alloc" || true
echo "-----------------------------------------------"

echo ""
echo "[dmesg] Kernel messages:"
echo "-----------------------------------------------"
sudo dmesg | grep "$TAG" | tail -10
echo "-----------------------------------------------"

echo ""
echo "[cleanup] Unloading module..."
sudo rmmod importer_map 2>/dev/null || true
echo ""
echo "Done. Key points:"
echo "  - Userspace allocated from /dev/dma_heap/system"
echo "  - Driver: dma_buf_get -> attach -> map_attachment -> sg_dma_address"
echo "  - This is the standard 6-step import pattern"
