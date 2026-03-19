#!/bin/bash
# DMA Heap Part 5: Async Processing with SIGIO
# Demonstrates: fasync, kill_fasync, SIGIO signal, multi-frame loop
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
TAG="dma_async"

echo "=== DMA Heap Part 5: Async Processing with SIGIO ==="
echo ""

echo "[build] Building module and userspace test..."
make -C "$DIR" -s
echo "  importer-async.ko and heap-async built OK"
echo ""

dmesg -C 2>/dev/null || true

echo "[load] Loading async driver..."
sudo insmod "$DIR/importer-async.ko"
sleep 0.5

echo "[run] Processing 5 frames asynchronously (watch for SIGIO)..."
echo "-----------------------------------------------"
sudo "$DIR/heap-async" || true
echo "-----------------------------------------------"

echo ""
echo "[dmesg] Kernel messages:"
echo "-----------------------------------------------"
sudo dmesg | grep "$TAG" | tail -20
echo "-----------------------------------------------"

echo ""
echo "[cleanup] Unloading module..."
sudo rmmod importer_async 2>/dev/null || true
echo ""
echo "Done"
