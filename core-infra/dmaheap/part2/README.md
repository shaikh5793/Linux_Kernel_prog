# Part 02: Array of DMA Heap Buffers

## Overview

This example extends Part 1 to demonstrate sharing multiple DMA heap buffers with a kernel driver in a single ioctl call. The driver iterates over an array of file descriptors, importing and mapping each buffer independently.

- Allocating multiple DMA heap buffers
- Passing an array of buffer file descriptors via ioctl
- Per-buffer import, attach, map, and DMA address retrieval
- Graceful error handling (continue processing on individual failures)

**Files:**
- `importer-array.c` — Kernel driver that processes an array of DMA buffer fds
- `heap-array.c` — Userspace test that allocates 4 buffers and passes them as an array
- `Makefile` — Build configuration
- `run-demo.sh` — All-in-one demo script

**Prerequisites:** Understanding of Part 1 (basic import pattern).

## Context and Scope

Real hardware often works with multiple buffers simultaneously:
- **Multi-plane video**: Y/U/V planes as separate DMA buffers
- **Scatter-gather DMA**: Multiple source/destination buffers for a single transfer
- **Ring buffers**: A pool of buffers for streaming I/O

This example introduces a custom ioctl structure that bundles multiple file descriptors into a single array, demonstrating how drivers can efficiently process buffer pools.

The scope covers:
- **4 buffers**: Each 4KB, allocated independently from the system heap.
- **Array ioctl**: A single `MY_DMA_IOCTL_MAP_ARRAY` call passes all fds at once.
- **Independent mapping**: Each buffer is attached and mapped separately, allowing different DMA addresses.

## Key APIs

### Custom Structures
```c
#define MAX_BUFFERS 4
struct dma_buffer_array {
    __u32 count;
    int fds[MAX_BUFFERS];
};
```

### Userspace
- `DMA_HEAP_IOCTL_ALLOC` — Allocate each buffer individually
- `mmap()` / `munmap()` — Fill each buffer with test data
- `ioctl(MY_DMA_IOCTL_MAP_ARRAY)` — Pass the array to the driver

### Kernel
- Same 6-step import pattern as Part 1, applied in a loop
- Error handling: `continue` on per-buffer failures instead of aborting

### IOCTL
- `MY_DMA_IOCTL_MAP_ARRAY _IOW('M', 3, struct dma_buffer_array)`
- **Device**: `/dev/dummy_dma_array_device`

## What Changed from Part 1

| Aspect | Part 1 | Part 2 |
|--------|--------|--------|
| Buffers | Single fd | Array of 4 fds |
| IOCTL | `_IOW('M', 2, int)` | `_IOW('M', 3, struct dma_buffer_array)` |
| Error handling | Abort on error | Continue to next buffer |
| Device name | `dummy_dma_map_device` | `dummy_dma_array_device` |

## Build and Test

### Building
```bash
cd core/dmaheap/part2
make
```

### Testing
```bash
sudo insmod importer-array.ko
sudo ./heap-array
sudo dmesg | grep dummy_dma_array | tail -15
sudo rmmod importer_array
make clean
```

Or use the all-in-one script:
```bash
./run-demo.sh
```

### Expected Output

Userspace:
```
Allocated DMA buffer 0, fd: 4
Buffer 0 filled with data: Buffer 0 data
...
Allocated DMA buffer 3, fd: 7
Buffer 3 filled with data: Buffer 3 data
DMA buffer array mapping request sent to driver.
```

dmesg:
```
dummy_dma_array: Received 4 DMA buffer fds
dummy_dma_array: Processing buffer 0, fd 4
dummy_dma_array: Imported dma_buf <ptr>, size: 4096
dummy_dma_array: Buffer 0 DMA address: 0x00000000XXXXXXXX
...
dummy_dma_array: Buffer 3 DMA address: 0x00000000YYYYYYYY
```

## Important Notes

1. **Independent allocations**: Each buffer has a separate DMA address. They are not contiguous in physical memory.
2. **Error resilience**: The driver uses `continue` rather than `goto` so that one failed buffer doesn't prevent processing of the others.
3. **MAX_BUFFERS constant**: Must match between userspace and kernel. In production, consider using a header shared by both.
4. **Copy semantics**: The entire `struct dma_buffer_array` is copied from userspace in one `copy_from_user()` call.

## Troubleshooting

- **"array count exceeds MAX_BUFFERS"**: The userspace sent a count larger than 4. Check `buf_array.count`.
- **Some buffers fail**: Each buffer is processed independently — check per-buffer error messages in dmesg.
- **Permission denied**: Use `sudo` for module and test operations.

## Next Steps

Proceed to **Part 3** to learn about synchronized CPU and DMA access with `begin_cpu_access` / `end_cpu_access`.

## License

GPL, as specified by `MODULE_LICENSE("GPL")`.

## Author

Raghu Bharadwaj (raghu@techveda.org), TECH VEDA.
