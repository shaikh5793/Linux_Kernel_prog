# Part 04: Multi-SG Entry Traversal

## Overview

This example demonstrates how to iterate over all scatter-gather entries when importing a DMA heap buffer. A 4MB allocation from the system heap produces multiple SG entries because the heap's page pool caps individual allocations at order 8 (1MB compound pages).

- Full scatter-gather table traversal using `for_each_sgtable_dma_sg()`
- Comparing `orig_nents` (pre-DMA-map) vs `nents` (post-DMA-map)
- System heap page pool orders: `[8 (1MB), 4 (64KB), 0 (4KB)]`
- Per-entry `sg_dma_address()` / `sg_dma_len()` inspection

**Files:**
- `importer-multisg.c` — Kernel driver that traverses all SG entries
- `heap-multisg.c` — Userspace test allocating 4MB from system heap
- `Makefile` — Build configuration
- `run-demo.sh` — All-in-one demo script

**Prerequisites:** Understanding of Parts 1-3 (import pattern, CPU/DMA access).

## Context and Scope

Previous examples only looked at the first SG entry (`sgt->sgl`). In reality, DMA buffers often have multiple SG entries. Drivers that program scatter-gather DMA engines must iterate all entries.

### Why 4MB Produces Multiple Entries

The system heap allocates pages from a pool with three page orders:

| Order | Size | Usage |
|-------|------|-------|
| 8 | 1MB | Largest available, tried first |
| 4 | 64KB | Medium fallback |
| 0 | 4KB | Smallest fallback |

For a 4MB allocation, the heap calls `alloc_pages(order=8)` four times, producing 4 separate 1MB compound pages. Each page becomes its own SG entry in the buffer's scatter-gather table. The attach callback (`dup_sg_table`) copies this table directly — no merging occurs.

Small allocations (up to 1MB) fit in a single order-8 page and produce just 1 SG entry.

### DMA Mask: 64-bit

This example uses `DMA_BIT_MASK(64)` instead of the 32-bit mask used in Parts 1-3. On systems without a hardware IOMMU, a 32-bit mask forces SWIOTLB bounce buffering. SWIOTLB's max single allocation is ~256KB, which fails for 1MB SG segments. A 64-bit mask bypasses SWIOTLB entirely.

### IOMMU Effects

On systems with an IOMMU, `nents` (after DMA mapping) may be smaller than `orig_nents` (before DMA mapping) because the IOMMU can make discontiguous physical pages appear contiguous from the device's perspective.

## Key APIs

### SG Table Traversal
- `for_each_sgtable_dma_sg(sgt, sg, i)` — Iterate DMA-mapped SG entries (uses `nents`)
- `sg_dma_address(sg)` — Get the DMA address of an SG entry
- `sg_dma_len(sg)` — Get the length of an SG entry

### SG Entry Counts
- `sgt->orig_nents` — Number of SG entries before DMA mapping (CPU-side view)
- `sgt->nents` — Number of SG entries after DMA mapping (device-side view, may be coalesced by IOMMU)

### IOCTL
- `MY_DMA_IOCTL_MULTISG _IOW('M', 8, int)`
- **Device**: `/dev/dummy_dma_multisg_device`

## What Changed from Part 3

| Aspect | Part 3 | Part 4 |
|--------|--------|--------|
| SG traversal | First entry only | All entries via `for_each_sgtable_dma_sg` |
| Buffer size | 4KB | 4MB (4 x 1MB SG entries) |
| DMA mask | 32-bit | 64-bit (avoids SWIOTLB limits) |
| Focus | CPU/DMA sync | Physical memory layout |
| Device name | `dummy_dma_transfer_device` | `dummy_dma_multisg_device` |

## Build and Test

### Building
```bash
cd core/dmaheap/part4
make
```

### Testing
```bash
sudo insmod importer-multisg.ko
sudo ./heap-multisg
sudo dmesg | grep dma_multisg
sudo rmmod importer_multisg
make clean
```

Or use the all-in-one script:
```bash
./run-demo.sh
```

### Expected Output

Userspace:
```
=== DMA Multi-SG Test ===

Using heap: system
Allocated 4194304 bytes (4 MB), fd: 4
Buffer filled: Multi-SG test, size=4194304

Check dmesg for SG table details:
  sudo dmesg | grep dma_multisg

System heap page pool max order = 8 (1MB compound pages).
4MB requires 4 separate pages, each becoming an SG entry.
```

dmesg:
```
dma_multisg: Imported dma_buf at <ptr>, size: 4194304
dma_multisg: Buffer content: Multi-SG test, size=4194304
dma_multisg: === Scatter-Gather Table ===
dma_multisg: orig_nents (before DMA map): 4
dma_multisg: nents (after DMA map):       4
dma_multisg: Buffer is scattered across 4 segments
dma_multisg: SG[0]: dma_addr=0x00000001XXXXXXXX, length=1048576 bytes
dma_multisg: SG[1]: dma_addr=0x00000001YYYYYYYY, length=1048576 bytes
dma_multisg: SG[2]: dma_addr=0x00000001ZZZZZZZZ, length=1048576 bytes
dma_multisg: SG[3]: dma_addr=0x00000001WWWWWWWW, length=1048576 bytes
dma_multisg: Total mapped size: 4194304 bytes
dma_multisg: === End SG Table ===
```

## Important Notes

1. **Allocation size determines SG entry count**: The system heap's largest page order is 8 (1MB). Allocate > 1MB to guarantee multiple entries.
2. **IOMMU coalescing**: With an IOMMU, `nents` may be smaller than `orig_nents` because the IOMMU creates contiguous DMA mappings.
3. **SWIOTLB limitation**: On systems without IOMMU, a 32-bit DMA mask triggers SWIOTLB bounce buffering, which can't handle segments > ~256KB. Use 64-bit mask for large buffers.
4. **CMA heap**: The CMA heap (`/dev/dma_heap/linux,cma`) allocates from a reserved contiguous region — always 1 SG entry regardless of size.
5. **Real-world SG usage**: GPU and media drivers that use scatter-gather DMA must iterate all entries — they cannot assume a single contiguous buffer.

## Troubleshooting

- **Only 1 SG entry**: Allocation is <= 1MB (fits in one order-8 page). Increase to > 1MB.
- **"swiotlb buffer is full"**: DMA mask is 32-bit and SG segments exceed SWIOTLB capacity. Switch to `DMA_BIT_MASK(64)`.
- **Permission denied**: Use `sudo` for module and test operations.

## Next Steps

Proceed to **Part 5** to learn about asynchronous processing with SIGIO signal notifications.

## License

GPL, as specified by `MODULE_LICENSE("GPL")`.

## Author

Raghu Bharadwaj (raghu@techveda.org), TECH VEDA.
