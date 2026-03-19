#!/bin/bash
# DMA-BUF Part 5: Buffer Synchronization
# Demonstrates: begin_cpu_access/end_cpu_access, DMA attach/map SG, mmap
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
TAG="Sync\|sync\|cpu_access\|buffer\|Buffer\|DMA\|SG\|attach\|Importer\|Exporter"

echo "=== DMA-BUF Part 5: Buffer Synchronization ==="
echo ""

echo "[build] Building modules and userspace test..."
make -C "$DIR" -s
gcc -Wall -o "$DIR/test-sync" "$DIR/test-sync.c"
echo ""

dmesg -C 2>/dev/null || true

echo "[load] Loading sync exporter..."
sudo insmod "$DIR/exporter-sync.ko"
sleep 0.5

echo "[load] Loading sync importer..."
sudo insmod "$DIR/importer-sync.ko"
sleep 0.5

echo "[run] Running sync pipeline test..."
echo "-----------------------------------------------"
sudo "$DIR/test-sync" || true
echo "-----------------------------------------------"

echo ""
echo "[dmesg] Kernel messages:"
echo "-----------------------------------------------"
sudo dmesg | grep -i "$TAG" | tail -20
echo "-----------------------------------------------"

echo ""
echo "[cleanup] Unloading modules..."
sudo rmmod importer_sync 2>/dev/null || true
sudo rmmod exporter_sync 2>/dev/null || true
rm -f "$DIR/test-sync"

echo ""
echo "Done. Key points:"
echo "  - DMA test: attach → map_attachment → read SG addresses → unmap → detach"
echo "  - CPU test: begin_cpu_access → vmap → read → vunmap → end_cpu_access"
echo "  - begin/end_cpu_access are the NEW concept (sync hooks for cache coherency)"
echo "  - Direction (DMA_FROM_DEVICE) tells exporter which cache op to perform"
echo "  - Userspace mmap also available for direct buffer access"
