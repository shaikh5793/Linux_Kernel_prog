# Part 03: Synchronized CPU and DMA Access

## Overview

This example demonstrates proper synchronization between CPU and DMA access to a DMA heap buffer. The driver uses `dma_buf_begin_cpu_access()` / `dma_buf_end_cpu_access()` for cache coherency, reads buffer contents via `dma_buf_vmap()`, and then simulates programming a DMA transfer.

- CPU access synchronization (cache coherency)
- Kernel virtual mapping via `dma_buf_vmap()` / `iosys_map`
- Simulated DMA transfer programming
- Two-phase access: CPU read then DMA mapping

**Files:**
- `importer-sync.c` — Kernel driver with synchronized CPU + DMA access
- `heap-sync.c` — Userspace test that allocates and fills a buffer
- `Makefile` — Build configuration
- `run-demo.sh` — All-in-one demo script

**Prerequisites:** Understanding of Part 1 (basic import pattern).

## Context and Scope

In real hardware drivers, a buffer may need both CPU access (to inspect or modify data) and DMA access (to transfer data to/from hardware). These accesses must be synchronized to ensure cache coherency:

- **CPU access**: `begin_cpu_access()` flushes/invalidates caches so the CPU sees the latest data. `end_cpu_access()` ensures any CPU writes are visible to DMA.
- **DMA mapping**: `dma_buf_map_attachment()` returns a scatter-gather table with bus addresses for DMA engine programming.

This two-phase pattern is fundamental to real media drivers that need to inspect buffer headers (CPU) before submitting to hardware (DMA).

### Simulated DMA Transfer

The `program_dma_transfer()` function contains commented pseudo-code showing what real hardware register writes would look like:
```c
write_register(DMA_SRC_ADDR_REG, dma_addr);
write_register(DMA_DST_ADDR_REG, destination_address);
write_register(DMA_TRANSFER_SIZE_REG, size);
write_register(DMA_CONTROL_REG, DMA_START | other_flags);
```

## Key APIs

### CPU Access Synchronization
- `dma_buf_begin_cpu_access(dmabuf, DMA_BIDIRECTIONAL)` — Prepare for CPU access (cache flush/invalidate)
- `dma_buf_end_cpu_access(dmabuf, DMA_BIDIRECTIONAL)` — Finish CPU access (writeback caches)

### Kernel Virtual Mapping
- `dma_buf_vmap(dmabuf, &map)` — Map buffer into kernel virtual address space
- `dma_buf_vunmap(dmabuf, &map)` — Unmap
- `struct iosys_map` — Abstraction for virtual or I/O memory mappings

### DMA Mapping
- Same attach/map/sg_dma_address pattern as Part 1

### IOCTL
- `MY_DMA_IOCTL_TRANSFER _IOW('M', 5, int)`
- **Device**: `/dev/dummy_dma_transfer_device`

## What Changed from Part 1

| Aspect | Part 1 | Part 3 |
|--------|--------|--------|
| CPU access | None | `begin_cpu_access` + `vmap` + `end_cpu_access` |
| Buffer read | Not read | Prints buffer contents in kernel |
| DMA transfer | Address only | Simulated transfer programming |
| Headers | Basic | + `iosys-map.h` for `struct iosys_map` |
| Device name | `dummy_dma_map_device` | `dummy_dma_transfer_device` |

## Build and Test

### Building
```bash
cd core/dmaheap/part3
make
```

### Testing
```bash
sudo insmod importer-sync.ko
sudo ./heap-sync
sudo dmesg | grep dma_transfer | tail -15
sudo rmmod importer_sync
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
Buffer filled with data: Test data for DMA transfer
DMA transfer programmed successfully.
```

dmesg:
```
dma_transfer: Received DMA buf fd: 4
dma_transfer: Imported dma_buf at <ptr>, size: 4096
dma_transfer: Buffer content (CPU access): Test data for DMA transfer
dma_transfer: Retrieved DMA address: 0x00000000XXXXXXXX
program_dma_transfer: Programming DMA transfer from address 0x00000000XXXXXXXX, size 4096
dma_transfer: DMA transfer programmed successfully.
```

## Important Notes

1. **Cache coherency**: On architectures with non-coherent DMA (ARM, MIPS), skipping `begin_cpu_access` / `end_cpu_access` can lead to stale data. On x86 with coherent DMA, these are typically no-ops but should always be called for portability.
2. **iosys_map**: The modern `struct iosys_map` (replacing older `struct dma_buf_map`) supports both virtual addresses and I/O memory, making the code compatible with both system and I/O-mapped buffers.
3. **Access ordering**: CPU access (read) happens before DMA mapping (transfer) — this is the correct order for a "prepare and submit" workflow.
4. **goto cleanup**: The driver uses `goto unmap_detach` for error cleanup, ensuring resources are always freed properly.

## Troubleshooting

- **"dma_buf_vmap failed"**: The exporter may not support `vmap`. System heap buffers always support it.
- **Empty buffer content**: Ensure the userspace test writes data before passing the fd to the driver.
- **Permission denied**: Use `sudo` for module and test operations.

## Next Steps

Proceed to **Part 4** to learn about multi-entry scatter-gather table traversal — iterating all SG entries when buffers span multiple physical page chunks.

## License

GPL, as specified by `MODULE_LICENSE("GPL")`.

## Author

Raghu Bharadwaj (raghu@techveda.org), TECH VEDA.
