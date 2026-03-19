# Part 01: Basic DMA Heap Import and Mapping

## Overview

This is the first example in the DMA Heap learning series. It demonstrates the fundamental workflow of importing a DMA heap buffer into a kernel driver using the `dma_buf` framework.

- DMA heap allocation from userspace
- Importing a `dma_buf` via file descriptor
- Attaching and mapping for DMA access
- Retrieving the DMA address from the scatter-gather table

**Files:**
- `importer-map.c` — Kernel driver that imports and maps a DMA heap buffer
- `heap-alloc.c` — Userspace test that allocates, fills, and passes a buffer
- `Makefile` — Build configuration
- `run-demo.sh` — All-in-one demo script

**Prerequisites:** None — this is the starting point!

## Context and Scope

The primary purpose of this example is to illustrate the standard DMA heap import pattern:

- **Userspace role**: Allocates a buffer from the DMA heap (`/dev/dma_heap/system`), fills it with test data via `mmap()`, and passes the file descriptor to a kernel driver via `ioctl()`.
- **Kernel role**: Receives the fd, imports the `dma_buf`, attaches it to a device, maps it for DMA access, and retrieves the physical DMA address.

The scope is intentionally limited to:
- **Single buffer**: One 4KB allocation from the system heap.
- **DMA mapping only**: The driver retrieves the DMA address but doesn't perform an actual transfer — that comes in Part 3.
- **Dummy platform device**: A `platform_device_register_simple()` device provides the required `struct device` with a DMA mask.

This setup mirrors real-world scenarios where a hardware driver receives externally-allocated buffers (e.g., from a camera or codec) and needs to map them for DMA access.

## Key APIs

### Userspace
- `DMA_HEAP_IOCTL_ALLOC` — Allocate a buffer from the DMA heap
- `mmap()` — Map the DMA buffer to userspace for read/write access
- `ioctl()` — Pass the buffer fd to the kernel driver

### Kernel (6-step import pattern)
1. `dma_buf_get(fd)` — Convert file descriptor to `struct dma_buf *`
2. `dma_buf_attach(dmabuf, dev)` — Attach the buffer to a device
3. `dma_buf_map_attachment(attachment, dir)` — Map for DMA, returns `sg_table`
4. `sg_dma_address(sgt->sgl)` — Get the DMA address from the first SG entry
5. `dma_buf_unmap_attachment()` — Unmap the DMA attachment
6. `dma_buf_detach()` / `dma_buf_put()` — Detach and release

### IOCTL
- `MY_DMA_IOCTL_MAP _IOW('M', 2, int)` — Pass a DMA buffer fd to the driver
- **Device**: `/dev/dummy_dma_map_device`

## Build and Test

### Building
```bash
cd core/dmaheap/part1
make
```

### Testing
```bash
# Load the driver
sudo insmod importer-map.ko

# Run the test
sudo ./heap-alloc

# Check kernel messages
sudo dmesg | grep dummy_dma | tail -10

# Unload
sudo rmmod importer_map

# Clean
make clean
```

Or use the all-in-one script:
```bash
./run-demo.sh
```

### Expected Output

Userspace:
```
Allocated DMA buffer, fd: 4
Buffer filled with data: Test DMA buffer data.
DMA buffer mapping request sent to driver.
```

dmesg:
```
dummy_dma: Device registered as /dev/dummy_dma_map_device
dummy_dma: Received DMA buf fd: 4
dummy_dma: Imported dma_buf at <ptr>, size: 4096
dummy_dma: DMA address of buffer: 0x00000000XXXXXXXX
```

## Important Notes

1. **Dummy platform device**: The driver creates a dummy platform device because `dma_buf_attach()` requires a valid `struct device *` with a DMA mask. Real drivers use their actual hardware device.
2. **DMA mask**: `dma_set_mask_and_coherent(&dev, DMA_BIT_MASK(32))` is required before DMA mapping will succeed.
3. **No actual transfer**: This example only retrieves the DMA address. Part 3 adds a simulated DMA transfer.
4. **Namespace**: `MODULE_IMPORT_NS("DMA_BUF")` is required for out-of-tree modules accessing DMA-BUF symbols.
5. **Heap availability**: The system heap (`/dev/dma_heap/system`) is always available. CMA heap requires kernel CMA configuration.

## Troubleshooting

- **"Failed to open /dev/dma_heap/system"**: Ensure `CONFIG_DMABUF_HEAPS_SYSTEM` is enabled in your kernel config.
- **"Unknown symbol" on insmod**: Check that `MODULE_IMPORT_NS("DMA_BUF")` is present in the driver.
- **Permission denied**: Use `sudo` for module and test operations.

## Next Steps

Proceed to **Part 2** to learn about passing arrays of DMA buffers to a driver.

## License

GPL, as specified by `MODULE_LICENSE("GPL")`.

## Author

Raghu Bharadwaj (raghu@techveda.org), TECH VEDA.
