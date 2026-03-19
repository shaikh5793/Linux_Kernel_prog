# Part 01: Basic Exporter and Importer

## Overview

This is the first example in the DMA-BUF learning series. It demonstrates the fundamental concepts of DMA-BUF buffer sharing between kernel modules using CPU-accessible memory.

- Basic DMA-BUF exporter/importer pattern
- Buffer allocation and export
- Virtual memory mapping (vmap)
- Reference counting basics

**Files:**
- `exporter-kmap.c` - Creates and exports a DMA buffer
- `importer-kmap.c` - Imports and reads the buffer
- `Makefile` - Build configuration

**Prerequisites:** None - this is the starting point!

## Context and Scope

The primary purpose of these examples is to illustrate the core concepts of the DMA-BUF framework in a simplified, CPU-only scenario:
- **Exporter Role**: The exporter creates a buffer, wraps it in a `struct dma_buf`, and defines how it can be accessed (e.g., via virtual mapping). It represents a producer of shared memory, akin to a driver allocating a frame buffer or texture.
- **Importer Role**: The importer accesses the shared `dma_buf`, maps it, and consumes its data, simulating a consumer like a display driver or renderer.

The scope is intentionally limited to:
- **Single Buffer**: The exporter manages one `dma_buf` with a fixed size (one page).
- **CPU Access**: The buffer is accessed via kernel virtual mappings (`vmap`), not DMA, to keep the example straightforward.
- **Kernel Space**: The interaction occurs entirely within the kernel, without userspace involvement (e.g., no `mmap` support).

This setup mirrors real-world scenarios where drivers need to share data efficiently, such as passing a video frame from a camera driver to a processing unit, but abstracts away hardware-specific complexities for clarity.

## Foundational Notes on the DMA-BUF API

The DMA-BUF framework, introduced in Linux to unify buffer sharing across drivers, is built around the `struct dma_buf` and its associated operations (`struct dma_buf_ops`). Here are key concepts relevant to these examples:

### Core Components
- **`struct dma_buf`**:
  - A kernel object representing a shared buffer, containing metadata (e.g., size, private data) and a reference count.
  - Created by an exporter via `dma_buf_export()` and managed using reference counting (e.g., `dma_buf_get()`, `dma_buf_put()`).
- **`struct dma_buf_ops`**:
  - Defines callbacks for buffer operations, such as mapping (`vmap`, `map_dma_buf`), unmapping (`vunmap`, `unmap_dma_buf`), and cleanup (`release`).
  - In these examples, only `vmap` and `release` are implemented meaningfully, as the buffer is CPU-only.

### Key Functions
- **`dma_buf_export()`**:
  - Takes a `struct dma_buf_export_info` (populated with operations, size, and private data) and returns a `struct dma_buf`.
  - Used in the exporter to create the shared buffer.
- **`dma_buf_vmap()` and `dma_buf_vunmap()`**:
  - Map and unmap the buffer into kernel virtual address space using the `iosys_map` abstraction (introduced in later kernels like 6.8.x).
  - Replace older `dma_buf_kmap()` APIs, providing a unified mapping interface.
- **`dma_buf_put()`**:
  - Decrements the reference count of a `dma_buf`. When it reaches zero, the `release` callback frees associated resources.

### Modern API (6.8.x)
- **iosys_map**: A structure (`struct iosys_map`) abstracts buffer mappings, supporting both virtual addresses and I/O memory. In these examples, it’s used with `vmap` to provide CPU access.
- **Deprecation**: Older APIs like `dma_buf_kmap()` and `dma_buf_ops.map` are removed by 6.8.x, reflecting a shift to `vmap`/`vunmap` for consistency.

### Resource Management
- **Reference Counting**: The `dma_buf` uses a reference count to track users. The exporter holds an initial reference, and importers increment it. Resources are freed only when the count drops to zero.
- **Private Data**: The `priv` field in `struct dma_buf` allows exporters to attach custom data (e.g., the allocated buffer), managed and freed via the `release` callback.

## Example-Specific Details

### Exporter (`exporter-kmap.c`)
- **Functionality**: Allocates a page-sized buffer, writes "hello world!", and exports it as `dmabuf_exported`. It supports virtual mapping via `vmap` but not DMA or userspace access.
- **Key Implementation**:
  - `exporter_alloc_page()`: Sets up the buffer and exports it.
  - `exporter_release()`: Frees the buffer when no references remain.
  - `exporter_exit()`: Drops the module’s reference, triggering cleanup if no importers are active.

### Importer (`importer-kmap.c`)
- **Functionality**: Accesses `dmabuf_exported`, maps it with `dma_buf_vmap()`, and reads the string.
- **Key Implementation**:
  - `importer_test()`: Demonstrates buffer access using the `iosys_map` API.
  - Minimal cleanup, as it doesn’t hold a persistent reference.

## Important Notes

1. **Dependency**: The importer relies on the exporter initializing `dmabuf_exported`. Without the exporter, the importer fails.
2. **CPU-Only Limitation**: These examples don’t implement DMA support (`map_dma_buf` returns `NULL`), making them unsuitable for hardware acceleration without modification.
3. **Reference Counting Behavior**: If the importer is active when the exporter unloads, the buffer memory persists until the importer releases its reference, a standard DMA-BUF feature.
4. **Kernel Version**: Tailored for 6.8.x, using `iosys_map` and modern `vmap` APIs. Adaptation for older kernels requires reverting to deprecated APIs.
5. **Namespace**: Both modules import the `DMA_BUF` namespace, necessary for out-of-tree modules accessing DMA-BUF symbols.

## Build and Test

### Building
```bash
# Navigate to the directory
cd core/dma-buf/kern-prodcon/part1

# Build the modules
make

# Verify the build
ls *.ko
```

### Testing
```bash
# Load the exporter first (creates the buffer)
sudo insmod exporter-kmap.ko

# Load the importer (reads the buffer)
sudo insmod importer-kmap.ko

# Check kernel messages
dmesg | tail -10

# Unload modules (order matters - importer first)
sudo rmmod importer-kmap
sudo rmmod exporter-kmap

# Clean build artifacts
make clean
```

### Expected Output
You should see messages like:
```
[  123.456] DMA-BUF Exporter initialized
[  124.567] DMA-BUF Importer: Found dmabuf_exported
[  124.568] DMA-BUF Importer: Buffer content: hello world!
```

## Next Steps

After completing this example, proceed to:
- **Part 02**: Basic driver structure with dummy devices
- Learn about platform devices and proper driver architecture

## Troubleshooting

- **Module loading fails**: Ensure kernel headers are installed
- **"Unknown symbol" errors**: Check `dmesg` for missing dependencies
- **Permission denied**: Use `sudo` for module operations

## License

GPL v2, as specified by `MODULE_LICENSE("GPL v2")`.

## Author

Raghu Bharadwaj (raghu@techveda.org) updated for kernel 6.8.x.
