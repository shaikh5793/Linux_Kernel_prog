#!/bin/bash
# DMA Heap Part 6: Poll-based Completion with dma_fence
# Demonstrates: dma_fence, dma_fence_add_callback, custom .poll, delayed_work
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
TAG="dma_poll"

echo "=== DMA Heap Part 6: Poll-based Completion with dma_fence ==="
echo ""

echo "[build] Building module and userspace test..."
make -C "$DIR" -s
echo "  importer-poll.ko and heap-poll built OK"
echo ""

dmesg -C 2>/dev/null || true

echo "[load] Loading poll driver..."
sudo insmod "$DIR/importer-poll.ko"
sleep 0.5

echo "[run] Submitting buffer, waiting via poll() on driver fd (~1 second)..."
echo "-----------------------------------------------"
sudo "$DIR/heap-poll" || true
echo "-----------------------------------------------"

echo ""
echo "[dmesg] Kernel messages (fence lifecycle):"
echo "-----------------------------------------------"
sudo dmesg | grep "$TAG" | tail -15
echo "-----------------------------------------------"

echo ""
echo "[cleanup] Unloading module..."
sudo rmmod importer_poll 2>/dev/null || true
echo ""
echo "Done. Key points:"
echo "  - poll() is on the driver fd (custom .poll with wait queue)"
echo "  - dma_fence_add_callback() wakes poll waiters when fence signals"
echo "  - Truly async: ioctl returns immediately, delayed_work processes later"
echo "  - Buffer shows ' [processed]' suffix after fence signals"
echo "  - This pattern is used by GPU/media drivers for completion notification"
