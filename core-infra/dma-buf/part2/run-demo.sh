#!/bin/bash
# DMA-BUF Part 2: Scatter-Gather DMA Support
# Demonstrates: map_dma_buf, sg_table, attach/detach, DMA addresses
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
TAG="Exporter\|Importer\|dmabuf\|sg\|SG\|attach\|DMA\|Multi"

echo "=== DMA-BUF Part 2: Scatter-Gather DMA Support ==="
echo ""

echo "[build] Building modules..."
make -C "$DIR" -s
echo ""

dmesg -C 2>/dev/null || true

# --- Track A: Single contiguous page (1-entry sg_table) ---
echo "====== Track A: Single-page SG exporter/importer ======"
echo ""

echo "[load] Loading SG exporter (single contiguous page)..."
sudo insmod "$DIR/exporter-sg.ko"
sleep 0.5

echo "[load] Loading SG importer (DMA attach/map + CPU vmap)..."
sudo insmod "$DIR/importer-sg.ko"
sleep 0.5

echo ""
echo "[dmesg] Kernel messages (single-page):"
echo "-----------------------------------------------"
sudo dmesg | grep -i "$TAG" | tail -15
echo "-----------------------------------------------"

echo ""
echo "[cleanup] Unloading single-page modules..."
sudo rmmod importer_sg 2>/dev/null || true
sudo rmmod exporter_sg 2>/dev/null || true

dmesg -C 2>/dev/null || true

# --- Track B: Scattered multi-page (N-entry sg_table) ---
echo ""
echo "====== Track B: Multi-page scattered SG exporter/importer ======"
echo ""

echo "[load] Loading multi-page exporter (4 scattered pages)..."
sudo insmod "$DIR/exporter-sg-multi.ko"
sleep 0.5

echo "[load] Loading multi-page importer (iterates all SG entries)..."
sudo insmod "$DIR/importer-sg-multi.ko"
sleep 0.5

echo ""
echo "[dmesg] Kernel messages (multi-page scattered):"
echo "-----------------------------------------------"
sudo dmesg | grep -i "$TAG" | tail -20
echo "-----------------------------------------------"

echo ""
echo "[cleanup] Unloading multi-page modules..."
sudo rmmod importer_sg_multi 2>/dev/null || true
sudo rmmod exporter_sg_multi 2>/dev/null || true

echo ""
echo "Done. Key points:"
echo "  - Track A: Single kzalloc page → 1-entry sg_table, dma_map_single()"
echo "  - Track B: 4 alloc_page() pages → 4-entry sg_table, dma_map_sgtable()"
echo "  - Track B pages have different physical addresses (non-contiguous)"
echo "  - for_each_sgtable_dma_sg() iterates all entries uniformly"
echo "  - vmap() gives contiguous CPU view over scattered physical pages"
echo "  - IOMMU may coalesce entries: compare orig_nents vs nents"
