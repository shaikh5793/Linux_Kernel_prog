#!/bin/bash
# DMA Heap Part 4: Multi-SG Entry Traversal
# Demonstrates: for_each_sgtable_dma_sg, large allocation, orig_nents vs nents
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
TAG="dma_multisg"

echo "=== DMA Heap Part 4: Multi-SG Entry Traversal ==="
echo ""

echo "[build] Building module and userspace test..."
make -C "$DIR" -s
echo "  importer-multisg.ko and heap-multisg built OK"
echo ""

dmesg -C 2>/dev/null || true

echo "[load] Loading multi-SG driver..."
sudo insmod "$DIR/importer-multisg.ko"
sleep 0.5

echo "[run] Allocating 4MB from system heap (4 x 1MB pages = 4 SG entries)..."
echo "-----------------------------------------------"
sudo "$DIR/heap-multisg" || true
echo "-----------------------------------------------"

echo ""
echo "[dmesg] Kernel messages (SG table details):"
echo "-----------------------------------------------"
sudo dmesg | grep "$TAG" | tail -25
echo "-----------------------------------------------"

echo ""
echo "[cleanup] Unloading module..."
sudo rmmod importer_multisg 2>/dev/null || true
echo ""
echo "Done. Key points:"
echo "  - System heap max order=8 (1MB), so 4MB = 4 SG entries"
echo "  - for_each_sgtable_dma_sg() iterates post-DMA-map entries"
echo "  - orig_nents (pre-map) vs nents (post-map, IOMMU may coalesce)"
