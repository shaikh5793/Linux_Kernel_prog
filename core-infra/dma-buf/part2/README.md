# Part 02: Exporter with DMA Scatter-Gather Support

## Overview

This example builds upon the basic exporter/importer pattern by adding proper DMA support through scatter-gather (SG) tables. It demonstrates how exporters can support both CPU and DMA access to the same buffer.

**What you'll learn:**
- Implementing DMA-capable exporters
- Scatter-gather table creation and management
- DMA mapping and address retrieval
- Supporting both CPU (vmap) and DMA access modes
- Device attachment/detachment callbacks
- Why sg_table is required (not just dma_addr_t) — demonstrated via scattered pages

**Files (Track A — single contiguous page):**
- `exporter-sg.c` - Single-page exporter, 1-entry sg_table via dma_map_single
- `importer-sg.c` - Importer exercising both DMA (attach/map SG) and CPU (vmap) paths

**Files (Track B — scattered multi-page):**
- `exporter-sg-multi.c` - 4 individually-allocated pages, 4-entry sg_table via dma_map_sgtable
- `importer-sg-multi.c` - Iterates all SG entries, verifies disjoint DMA addresses

**Files (Alternative driver interface):**
- `exporter-drv.c` - Alternative exporter with driver interface
- `importer-drv.c` - Alternative importer with driver interface
- `Makefile` - Build configuration

**Prerequisites:** Completion of Part 01

## Context and Scope

The primary purpose of these examples is to illustrate DMA-BUF sharing with a mix of capabilities:
- **Exporter Role**: Creates a buffer, wraps it in a `struct dma_buf`, and defines operations for both CPU access (via `vmap`) and DMA access (via `map_dma_buf`). It represents a producer that could support hardware-accelerated data transfers.
- **Importer Role**: Accesses the shared `dma_buf` using both DMA (attach → map_attachment → read SG addresses → unmap → detach) and CPU (vmap) paths, demonstrating the full importer workflow.

The scope is defined as:
- **Single Buffer**: The exporter manages one `dma_buf` with a fixed size (one page).
- **Dual Access in Importer**: The importer exercises both DMA mapping (via a platform device) and CPU access (`vmap`), showing the complete import workflow.
- **DMA Support in Exporter**: The exporter provides DMA capabilities via `map_dma_buf`, which the importer now fully exercises.
- **Kernel Space**: Interaction occurs entirely within the kernel, without userspace involvement (e.g., no `mmap` support).

This setup mirrors scenarios where drivers share data (e.g., a GPU buffer passed to a display driver), but simplifies the importer for educational clarity.

## Foundational Notes on the DMA-BUF API

The DMA-BUF framework unifies buffer sharing across drivers using `struct dma_buf` and its operations (`struct dma_buf_ops`). Key concepts for this example include:

### Core Components
- **`struct dma_buf`**:
  - Represents a shared buffer with metadata (size, private data) and a reference count.
  - Created by the exporter via `dma_buf_export` and managed with reference counting (`dma_buf_get`, `dma_buf_put`).
- **`struct dma_buf_ops`**:
  - Defines callbacks for buffer operations: `vmap`/`vunmap` for CPU access, `map_dma_buf`/`unmap_dma_buf` for DMA, and `release` for cleanup.
  - This example uses `vmap` in the importer and supports both `vmap` and `map_dma_buf` in the exporter.

### Key Functions
- **`dma_buf_export`**:
  - Creates a `struct dma_buf` from a `struct dma_buf_export_info`, used by the exporter to share the buffer.
- **`dma_buf_vmap` and `dma_buf_vunmap`**:
  - Maps/unmaps the buffer into kernel virtual address space using `struct iosys_map`, used by the importer for CPU access.
- **`dma_buf_map_attachment` and `dma_buf_unmap_attachment`**:
  - Maps/unmaps the buffer for DMA using a scatter-gather table, used by the importer to retrieve DMA addresses from the exporter's SG table.
- **`dma_buf_put`**:
  - Decrements the reference count, triggering `release` when it reaches zero.

### Modern API (6.8.x)
- **`struct iosys_map`**: Abstracts buffer mappings for CPU access, used in `vmap`/`vunmap`.
- **Deprecation**: Older APIs like `dma_buf_kmap` are removed, with `vmap` being the standard for CPU access.

### Resource Management
- **Reference Counting**: The `dma_buf` tracks users via references. The exporter holds an initial reference, dropped on unload.
- **Debugfs Pinning**: With `CONFIG_DMA_SHARED_BUFFER_DEBUG` enabled, debugfs may hold an additional reference, requiring `rmmod -f` for unloading.

## Example-Specific Details

### Exporter (`exporter-sg.c`)
- **Functionality**: Allocates a page-sized buffer, exports it as `dmabuf_sg_exported`, and supports CPU access (`vmap`) and DMA access (`map_dma_buf` with scatter-gather).
- **Key Implementation**:
  - `exporter_alloc_page`: Creates and exports the buffer.
  - `exporter_map_dma_buf`: Provides a single-entry scatter-gather table for DMA.
  - `exporter_release`: Frees the buffer when references drop to zero.

### Importer (`importer-sg.c`)
- **Functionality**: Accesses `dmabuf_sg_exported` via both DMA and CPU paths.
- **Key Implementation**:
  - Creates a platform device (`sg_importer`) with `dma_set_mask_and_coherent(dev, DMA_BIT_MASK(64))` to provide the `struct device *` needed for DMA attach.
  - `importer_test_dma`: `dma_buf_attach` → `dma_buf_map_attachment(DMA_FROM_DEVICE)` → iterates `for_each_sgtable_dma_sg` reading `sg_dma_address`/`sg_dma_len` → `dma_buf_unmap_attachment` → `dma_buf_detach`.
  - `importer_test_cpu`: `dma_buf_vmap` → reads buffer content via `iosys_map` → `dma_buf_vunmap`.

## Important Notes

1. **Symbol Naming**: Uses `dmabuf_sg_exported` to avoid conflicts with other examples (e.g., `dmabuf_exported` from `exporter-kmap.c`).
2. **Full DMA + CPU Importer**: The importer exercises both paths — DMA attach/map (reading SG addresses) and CPU vmap (reading content).
3. **Reference Counting**: The exporter drops its reference on unload, but debugfs may pin the module (count=1 in `/sys/kernel/debug/dma_buf/bufinfo`), requiring `rmmod -f`.
4. **Kernel Version**: Tailored for 6.x, using `iosys_map` and modern `vmap` APIs.
5. **Namespace**: Both modules import the `DMA_BUF` namespace for out-of-tree compatibility.

## Build and Test

### Building
```bash
# Navigate to the directory
cd core/dma-buf/kern-prodcon/part2

# Build all modules
make

# Verify the build
ls *.ko
```

### Testing Basic SG Version
```bash
# Load the scatter-gather exporter
sudo insmod exporter-sg.ko

# Load the importer (uses CPU access)
sudo insmod importer-sg.ko

# Check kernel messages
dmesg | tail -15

# Unload modules
sudo rmmod importer-sg
sudo rmmod exporter-sg
```

### Testing Driver Version
```bash
# Load the driver-based versions
sudo insmod exporter-drv.ko
sudo insmod importer-drv.ko

# Check messages
dmesg | tail -15

# Unload
sudo rmmod importer-drv
sudo rmmod exporter-drv
```

### Expected Output
You should see messages indicating:
- Device attachment/detachment events
- DMA mapping: SG table entry count, DMA addresses and lengths
- CPU access: buffer content verification ("hello world!")
- Both DMA and CPU access tests passed

## Key Learning Points

1. **Scatter-Gather Tables**: Essential for DMA operations
2. **Device Attachment**: Proper device association for DMA
3. **Dual Access Modes**: Supporting both CPU and DMA access
4. **DMA Address Mapping**: How to retrieve DMA addresses
5. **Symbol Isolation**: Using unique names to avoid conflicts

## Next Steps

After completing this example, proceed to:
- **Part 03**: Memory mapping for userspace integration
- Learn about extending DMA-BUF to userspace via mmap

## Troubleshooting

- **DMA mapping fails**: Check if platform supports DMA
- **Symbol conflicts**: Ensure unique symbol names between examples
- **Reference counting issues**: May need `rmmod -f` if debugfs is enabled

## Potential Extensions

- Add `mmap` support for userspace access
- Manage multiple buffers dynamically
- Multi-page SG tables with non-contiguous allocations

## License

GPL v2, as specified by `MODULE_LICENSE("GPL v2")`.

## Author

Raghu Bharadwaj (raghu@techveda.org), updated for kernel 6.x.
