#!/bin/bash
# DMA-BUF Part 1: Basic Exporter/Importer
# Demonstrates: dma_buf_export, vmap, kernel-to-kernel sharing
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
TAG="exporter\|importer\|dmabuf"

echo "=== DMA-BUF Part 1: Basic Exporter/Importer ==="
echo ""

# Build
echo "[build] Building modules..."
make -C "$DIR" -s
echo ""

# Clear dmesg marker
dmesg -C 2>/dev/null || true

# Load
echo "[load] Loading exporter (creates buffer with 'hello world!')..."
sudo insmod "$DIR/exporter-kmap.ko"
sleep 0.5

echo "[load] Loading importer (reads the buffer via vmap)..."
sudo insmod "$DIR/importer-kmap.ko"
sleep 0.5

# Show results
echo ""
echo "[dmesg] Kernel messages:"
echo "-----------------------------------------------"
sudo dmesg | grep -i "$TAG" | tail -15
echo "-----------------------------------------------"

# Cleanup
echo ""
echo "[cleanup] Unloading modules..."
sudo rmmod importer_kmap 2>/dev/null || true
sudo rmmod exporter_kmap 2>/dev/null || true
echo ""
echo "Done. Key points:"
echo "  - Exporter allocated buffer and exported as dma_buf"
echo "  - Importer read buffer via dma_buf_vmap()"
echo "  - No hardware, no DMA addresses -- pure CPU access"
