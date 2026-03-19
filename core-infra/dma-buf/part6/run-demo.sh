#!/bin/bash
# DMA-BUF Part 6: DMA Fence Synchronization (Exporter → App → Importer)
# Demonstrates: dma_fence, dma_resv, fence signaling via delayed_work,
#               FD sharing pipeline with fence-aware importer
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
TAG="fence_exporter\|fence_importer"

echo "=== DMA-BUF Part 6: DMA Fence Synchronization ==="
echo ""

echo "[build] Building modules and userspace test..."
make -C "$DIR" -s
echo ""

dmesg -C 2>/dev/null || true

echo "[load] Loading fence exporter (creates buffer + fence)..."
sudo insmod "$DIR/exporter-fence.ko"
sleep 0.3

echo "[load] Loading fence importer (provides /dev/importer-fence)..."
sudo insmod "$DIR/importer-fence.ko"
sleep 0.3

echo ""
echo "[run] Running test-fence (gets fd, passes to importer)..."
echo "  Exporter starts simulated hardware work when fd is delivered."
echo "  Importer waits for fence to signal before accessing buffer."
echo ""
sudo "$DIR/test-fence"

sleep 2  # Wait for fence to signal and importer to finish

echo ""
echo "[dmesg] Kernel messages (full fence lifecycle):"
echo "-----------------------------------------------"
sudo dmesg | grep "$TAG" | tail -25
echo "-----------------------------------------------"

echo ""
echo "[cleanup] Unloading modules..."
sudo rmmod importer_fence 2>/dev/null || true
sudo rmmod exporter_fence 2>/dev/null || true
echo ""
echo "Done. Key points:"
echo "  - Exporter created fence, attached to dma_buf->resv"
echo "  - Userspace app mediated fd between exporter and importer"
echo "  - IOCTL delivery started simulated hardware work (fence in ~1s)"
echo "  - Importer called dma_resv_wait_timeout() — blocked until signaled"
echo "  - Fence signaled by delayed_work (simulates hardware completion)"
echo "  - Importer then exercised DMA (sg_table) and synced CPU (begin/vmap/end)"
echo "  - This is how GPU/media drivers synchronize rendering and display"
