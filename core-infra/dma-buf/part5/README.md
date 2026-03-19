# Part 05: Buffer Access Synchronization

## Overview

This example introduces synchronization mechanisms for safe buffer access in DMA-BUF sharing. It demonstrates how to coordinate CPU access to shared buffers using sync callbacks and proper synchronization patterns.

**What you'll learn:**
- DMA-BUF synchronization callbacks
- CPU access coordination (`begin_cpu_access`/`end_cpu_access`)
- Userspace buffer mapping with sync
- Safe concurrent access patterns
- Synchronization best practices

**Files:**
- `exporter-sync.c` - Exporter with synchronization support
- `importer-sync.c` - Importer with sync-aware access
- `test-sync.c` - Userspace application with sync operations
- `hw/` - Hardware abstraction examples (optional)
- `Makefile` - Build configuration

**Prerequisites:** Completion of Parts 01-04

## Purpose and Scope

The pipeline showcases:
- **Buffer Sharing**: A single, page-sized buffer shared via DMA-BUF between kernel and userspace.
- **Synchronization**: Includes placeholders for `begin_cpu_access` and `end_cpu_access` callbacks to manage CPU access safely.
- **Full Workflow**: Integrates kernel modules and userspace to transfer and validate an FD.
- **Extensibility**: Designed for future enhancements like DMA support.

This setup is ideal for understanding buffer sharing in scenarios involving kernel drivers and potential hardware acceleration.

## DMA-BUF Basics

The DMA-BUF framework enables efficient memory sharing in Linux:
- **`struct dma_buf`**: Represents the shared buffer, created with `dma_buf_export` and managed with reference counting (`dma_buf_get`, `dma_buf_put`).
- **`struct dma_buf_ops`**: Defines operations like `vmap` (kernel mapping), `mmap` (userspace mapping), and sync callbacks.
- **Key APIs**:
  - `dma_buf_fd`: Converts a DMA-BUF to an FD for userspace.
  - `dma_buf_vmap`: Maps the buffer for kernel CPU access.
  - `remap_pfn_range`: Enables userspace mapping via `mmap`.

This example uses modern 6.8.x APIs, including `struct iosys_map` for mappings and `MODULE_IMPORT_NS(DMA_BUF)` for out-of-tree builds.

## Workflow

Here’s how the pipeline works:

1. **Exporter Setup**:
   - Allocates a buffer with `kmalloc`, fills it with "hello world!", and exports it as a DMA-BUF (`dmabuf_sync_exported`).
   - Registers `/dev/exporter-sync` to share the FD via `ioctl`.

2. **Importer Setup**:
   - Registers `/dev/importer-sync` to accept an FD via `ioctl`.

3. **FD Retrieval**:
   - `test-sync` opens `/dev/exporter-sync` and uses `ioctl` to get the FD.

4. **Optional Mapping**:
   - `test-sync` maps the buffer with `mmap` to read its contents in userspace (optional).

5. **FD Transfer**:
   - `test-sync` opens `/dev/importer-sync` and passes the FD via `ioctl`.

6. **Importer Validation**:
   - The importer converts the FD to a DMA-BUF, maps it with `dma_buf_vmap`, and checks its contents, using sync hooks.

7. **Cleanup**:
   - `test-sync` closes all FDs; kernel modules release resources when unloaded.

This sequence validates the FD sharing process and synchronization readiness.

## Component Details

### Exporter (`exporter-sync.c`)
- **Role**: Creates and exports a DMA-BUF.
- **Key Features**:
  - Allocates a buffer and exports it with `dma_buf_export`.
  - Supports `mmap` for userspace access.
  - Includes placeholder sync callbacks (`begin_cpu_access`, `end_cpu_access`).

### Importer (`importer-sync.c`)
- **Role**: Receives and validates a DMA-BUF FD.
- **Key Features**:
  - Uses `dma_buf_get` to access the buffer.
  - Validates contents with `dma_buf_vmap` and sync hooks.

### Userspace (`test-sync.c`)
- **Role**: Manages FD transfer between exporter and importer.
- **Key Features**:
  - Uses `ioctl` to retrieve and pass the FD.
  - Optionally maps the buffer with `mmap`.

## Build and Test

### Building
```bash
# Navigate to the directory
cd core/dma-buf/kern-prodcon/part5

# Build all modules
make

# Build userspace test application
gcc -o test-sync test-sync.c

# Verify the build
ls *.ko test-sync
```

### Testing

1. **Load both modules:**
```bash
sudo insmod exporter-sync.ko
sudo insmod importer-sync.ko
```

2. **Verify device creation:**
```bash
ls -l /dev/exporter-sync /dev/importer-sync
```

3. **Run the test application:**
```bash
./test-sync
```

4. **Check kernel messages:**
```bash
dmesg | tail -20
```

5. **Clean up:**
```bash
sudo rmmod importer-sync
sudo rmmod exporter-sync
```

### Expected Output

Kernel messages:
```
[  123.456] Exporter-sync initialized
[  123.457] Importer-sync initialized
[  124.567] begin_cpu_access called
[  124.568] Buffer content verified: hello world!
[  124.569] end_cpu_access called
```

Userspace test output:
```
Retrieved FD from exporter
Mapped buffer to userspace
Buffer content: hello world!
Passed FD to importer for validation
Synchronization test completed!
```

## Key Synchronization Concepts

1. **begin_cpu_access**: Called before CPU accesses the buffer
2. **end_cpu_access**: Called after CPU finishes with the buffer
3. **Cache Coherency**: Ensures data consistency between CPU and DMA
4. **Access Coordination**: Prevents race conditions between accessors
5. **Userspace Sync**: Coordinating userspace and kernel access

## Next Steps

After completing this example, proceed to:
- **Part 06**: DMA fence synchronization
- Learn about dma_fence, dma_resv, and hardware-style synchronization

## Troubleshooting

- **Sync callbacks not called**: Check DMA-BUF operations structure
- **Race conditions**: Ensure proper sync usage before buffer access
- **Memory corruption**: Verify sync callbacks implement cache operations

## License

GPL v2, as specified by `MODULE_LICENSE("GPL v2")`.

## Author

Raghu Bharadwaj (raghu@techveda.org), updated for kernel 6.8.x.
