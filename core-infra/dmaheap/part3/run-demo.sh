#!/bin/bash
# DMA Heap Part 3: Synchronized CPU + DMA Access
# Demonstrates: begin/end_cpu_access, vmap, DMA mapping, simulated transfer
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
TAG="dma_transfer"

echo "=== DMA Heap Part 3: Synchronized CPU + DMA Access ==="
echo ""

echo "[build] Building module and userspace test..."
make -C "$DIR" -s
echo "  importer-sync.ko and heap-sync built OK"
echo ""

dmesg -C 2>/dev/null || true

echo "[load] Loading sync driver..."
sudo insmod "$DIR/importer-sync.ko"
sleep 0.5

echo "[run] Allocating buffer, writing data, submitting to driver..."
echo "-----------------------------------------------"
sudo "$DIR/heap-sync" || true
echo "-----------------------------------------------"

echo ""
echo "[dmesg] Kernel messages:"
echo "-----------------------------------------------"
sudo dmesg | grep "$TAG" | tail -15
echo "-----------------------------------------------"

echo ""
echo "[cleanup] Unloading module..."
sudo rmmod importer_sync 2>/dev/null || true
echo ""
echo "Done. Key points:"
echo "  - Phase 1: begin_cpu_access -> vmap -> read -> vunmap -> end_cpu_access"
echo "  - Phase 2: attach -> map_attachment -> get DMA addr -> simulate transfer"
echo "  - Both CPU and DMA access to the same buffer, properly synchronized"
