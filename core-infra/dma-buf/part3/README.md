# Part 03: Memory Mapping for Userspace

## Overview

This example introduces userspace integration by demonstrating how to map DMA-BUF memory into userspace using `mmap()`. It builds on previous examples by adding a misc device interface for userspace communication.

**What you'll learn:**
- Implementing `mmap` support in DMA-BUF exporters
- Creating misc device interfaces for userspace
- Using `remap_pfn_range` for memory mapping
- IOCTL interfaces for file descriptor sharing
- Combining kernel and userspace buffer access

**Files:**
- `exporter-mmap.c` - Exporter with mmap support and misc device
- `mmap_test.c` - Userspace test application
- `Makefile` - Build configuration

**Prerequisites:** Completion of Parts 01-02

## Context and Scope

The primary purpose of this example set is to illustrate DMA-BUF usage with userspace integration:
- **Exporter Role**: Allocates a buffer, exports it as a `struct dma_buf` named `dmabuf_mmap_exported`, and provides a misc device interface to deliver the DMA-BUF file descriptor to userspace via an `ioctl` call. It supports CPU access (`vmap`) and userspace mapping (`mmap`).
- **Importer Role**: Validates kernel-side CPU access to the buffer and informs userspace access capabilities, serving as a bridge to verify the exported buffer’s integrity.

The scope includes:
- **Single Buffer**: A fixed-size (one page) buffer shared via DMA-BUF.
- **CPU Focus**: Both modules emphasize CPU access, with the exporter enabling userspace `mmap` and the importer testing kernel `vmap`.
- **Userspace Extension**: Introduces a misc device (`/dev/exporter`) for userspace interaction, a step beyond kernel-only examples.
- **DMA Potential**: Uses `kmalloc` with `GFP_DMA` for allocation, laying groundwork for future DMA support, though not implemented here.

This setup mirrors scenarios where kernel-allocated buffers need to be accessed by userspace applications (e.g., graphics or video processing), with a foundation for hardware acceleration.

## Foundational Notes on the DMA-BUF API

The DMA-BUF framework unifies buffer sharing using `struct dma_buf` and its operations (`struct dma_buf_ops`). Key concepts for this example include:

### Core Components
- **`struct dma_buf`**:
  - Represents a shared buffer with metadata (size, private data) and a reference count.
  - Created via `dma_buf_export` and managed with reference counting (`dma_buf_get`, `dma_buf_put`).
- **`struct dma_buf_ops`**:
  - Defines callbacks: `vmap`/`vunmap` for kernel CPU access, `mmap` for userspace mapping, and `release` for cleanup.
  - This set uses `vmap` and `mmap`, with `map_dma_buf` as a placeholder for potential DMA support.

### Key Functions
- **`dma_buf_export`**:
  - Creates the DMA-BUF object from a `struct dma_buf_export_info`.
- **`dma_buf_vmap` and `dma_buf_vunmap`**:
  - Maps/unmaps the buffer for kernel CPU access using `struct iosys_map`.
- **`dma_buf_fd`**:
  - Generates a file descriptor for the DMA-BUF, passed to userspace via `ioctl`.
- **`remap_pfn_range`**:
  - Maps kernel memory into userspace address space, used in `mmap`.

### Modern API (6.8.x)
- **`struct iosys_map`**: Abstracts virtual mappings for CPU access, replacing older `kmap` APIs.
- **Namespace**: Uses `MODULE_IMPORT_NS(DMA_BUF)` for out-of-tree module compatibility.

### Resource Management
- **Reference Counting**: The exporter holds an initial reference, dropped on unload, with userspace potentially holding additional references via the file descriptor.
- **Debugfs Pinning**: With `CONFIG_DMA_SHARED_BUFFER_DEBUG`, debugfs may retain a reference, requiring `rmmod -f`.

## Example-Specific Details

### Exporter (`exporter-mmap.c`)
- **Functionality**: Allocates a page-sized buffer with `kmalloc(GFP_DMA)`, exports it as `dmabuf_mmap_exported`, and provides a misc device (`/dev/exporter`) with an `ioctl` to deliver the DMA-BUF file descriptor.
- **Key Implementation**:
  - `exporter_alloc_page`: Creates and exports the buffer.
  - `exporter_mmap`: Enables userspace mapping via `remap_pfn_range`.
  - `exporter_ioctl`: Supplies the file descriptor for userspace `mmap`.

### Importer (`importer-mmap.c`)
- **Functionality**: Verifies kernel CPU access to `dmabuf_mmap_exported` using `vmap`, preparing for userspace validation.
- **Key Implementation**:
  - `importer_test`: Maps the buffer in kernel space and logs its contents, indicating userspace readiness.

## Important Notes

1. **Symbol Naming**: Uses `dmabuf_mmap_exported` to avoid conflicts with other examples (e.g., `dmabuf_sg_exported`).
2. **CPU Focus**: Prioritizes CPU access and userspace mapping; DMA support is potential but unimplemented.
3. **Misc Device**: `/dev/exporter` enables userspace interaction, requiring an `ioctl` call to obtain the DMA-BUF file descriptor.
4. **Reference Counting**: Debugfs may pin the module, necessitating `rmmod -f` if unloading fails.
5. **Kernel Version**: Tailored for 6.8.x with modern APIs (`iosys_map`, `vmap`).

## Build and Test

### Building
```bash
# Navigate to the directory
cd core/dma-buf/kern-prodcon/part3

# Build the kernel module
make

# Build the userspace test application
gcc -o mmap_test mmap_test.c

# Verify the build
ls *.ko mmap_test
```

### Testing
```bash
# Load the exporter module
sudo insmod exporter-mmap.ko

# Verify the misc device was created
ls -l /dev/exporter

# Run the userspace test
./mmap_test

# Check kernel messages
dmesg | tail -10

# Unload the module
sudo rmmod exporter-mmap
```

### Expected Output

Kernel messages:
```
[  123.456] DMA-BUF Exporter with mmap initialized
[  123.457] Misc device /dev/exporter registered
```

Userspace test output:
```
Opening /dev/exporter...
Getting DMA-BUF file descriptor...
Mapping buffer to userspace...
Buffer content: hello world!
Test completed successfully!
```

## Architecture Flow

```
Userspace Application (mmap_test)
           |
           | open("/dev/exporter")
           |
           v
    Misc Device Interface
           |
           | ioctl(GET_DMABUF_FD)
           |
           v
    DMA-BUF File Descriptor
           |
           | mmap(fd)
           |
           v
    Mapped Memory in Userspace
```

## Key Learning Points

1. **Misc Device**: Bridge between kernel and userspace
2. **IOCTL Interface**: Standard method for kernel-userspace communication
3. **Memory Mapping**: Direct userspace access to kernel buffers
4. **File Descriptors**: Portable handles for buffer sharing
5. **Reference Management**: Proper cleanup when userspace closes FDs

## Next Steps

After completing this example, proceed to:
- **Part 04**: File descriptor sharing pipeline
- Learn about complete FD sharing between kernel modules via userspace

## Troubleshooting

- **Device creation fails**: Check for conflicting device names
- **mmap fails**: Verify buffer allocation and permissions
- **IOCTL fails**: Ensure proper command numbers and structures
- **Permission denied**: Use appropriate file permissions for device access

## Potential Extensions

- Implement `map_dma_buf` for scatter-gather DMA support
- Expand the importer to handle userspace `ioctl` and `mmap` directly
- Support larger or multiple buffers
- Add write support for bidirectional communication

## License

GPL v2, as specified by `MODULE_LICENSE("GPL v2")`.

## Author

Raghu Bharadwaj (raghu@techveda.org), updated for kernel 6.8.x.
