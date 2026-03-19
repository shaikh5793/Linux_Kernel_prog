# Part 06: DMA Fence Synchronization

## Overview

This example demonstrates DMA fence synchronization using the exporter → app → importer pipeline established in Part 4. The exporter creates a buffer with an attached `dma_fence` that simulates pending hardware work, and the importer waits for the fence to signal before exercising DMA and synchronized CPU access.

**What you'll learn:**
- Creating and initializing DMA fences (`dma_fence_init`)
- Allocating fence contexts (`dma_fence_context_alloc`)
- Attaching fences to DMA-BUF reservation objects (`dma_resv`)
- Waiting on fences from the importer side (`dma_resv_wait_timeout`)
- Iterating fences with `dma_resv_for_each_fence`
- Signaling fences from delayed work (simulated hardware completion)
- Full FD sharing pipeline with fence-aware synchronization

**Files:**
- `exporter-fence.c` — Exporter with DMA fence attached to the buffer (misc device, IOCTL)
- `importer-fence.c` — Importer that waits on the fence, then does DMA + synced CPU access (misc device, IOCTL)
- `test-fence.c` — Userspace mediator that passes the DMA-BUF fd between modules
- `Makefile` — Build configuration
- `run-demo.sh` — Automated demo script

**Prerequisites:** Completion of Parts 01-05

## Context and Scope

This example introduces the synchronization primitives used in real GPU and media drivers, building on the FD sharing pipeline from Part 4 and the sync wrappers from Part 5:

- **Exporter Role**: Creates a buffer, attaches a `dma_fence` to its reservation object (`dma_buf->resv`), and provides the buffer's fd via IOCTL on `/dev/exporter-fence`. When the fd is delivered, the exporter schedules a `delayed_work` that signals the fence after ~1 second (simulating hardware starting work when the buffer is shared).
- **Userspace App Role**: Obtains the fd from the exporter (which triggers the simulated hardware work), optionally mmaps the buffer for direct access, then passes the fd to the importer.
- **Importer Role**: Receives the fd via IOCTL on `/dev/importer-fence`, inspects attached fences, waits for all write fences to signal via `dma_resv_wait_timeout`, then exercises both DMA (sg_table) and synchronized CPU (begin/vmap/end) access paths.

The scope includes:
- **Software Fences**: No real hardware is involved; the fence is signaled from a workqueue to simulate hardware completion.
- **Reservation Objects**: The `dma_resv` embedded in every `dma_buf` is used to attach and query fences, matching how GPU drivers coordinate access.
- **FD Sharing Pipeline**: Exporter and importer are independent misc devices, connected only through userspace — the same architecture used in production V4L2/DRM/Wayland pipelines.

## Key APIs

### Fence Creation (Exporter)
- `dma_fence_context_alloc(1)` — Allocates a unique fence context
- `dma_fence_init(fence, ops, lock, context, seqno)` — Initializes a fence
- `dma_fence_signal(fence)` — Marks the fence as complete

### Reservation Object (Exporter)
- `dma_resv_lock(resv, NULL)` — Locks the reservation object
- `dma_resv_reserve_fences(resv, 1)` — Pre-allocates space for fences
- `dma_resv_add_fence(resv, fence, DMA_RESV_USAGE_WRITE)` — Attaches fence
- `dma_resv_unlock(resv)` — Unlocks the reservation object

### Fence Waiting (Importer)
- `dma_resv_wait_timeout(resv, usage, intr, timeout)` — Blocks until all fences signal
- `dma_resv_for_each_fence(cursor, resv, usage, fence)` — Iterates attached fences

### DMA Access (Importer)
- `dma_buf_attach()` / `dma_buf_map_attachment()` — Get sg_table for DMA
- `dma_buf_unmap_attachment()` / `dma_buf_detach()` — Clean up DMA mapping

### Synchronized CPU Access (Importer)
- `dma_buf_begin_cpu_access()` / `dma_buf_end_cpu_access()` — Cache coherency sync
- `dma_buf_vmap()` / `dma_buf_vunmap()` — Kernel virtual mapping

## Workflow

1. **Load exporter**: Allocates buffer, writes "hello from fence exporter!", creates fence, attaches to `dma_buf->resv`, registers `/dev/exporter-fence`
2. **Load importer**: Registers `/dev/importer-fence` with platform device for DMA
3. **Run test-fence**: Gets fd from exporter via IOCTL (this starts simulated hardware work — fence signals in ~1s), optionally mmaps buffer, passes fd to importer via IOCTL
4. **Importer receives fd**: Inspects fences, calls `dma_resv_wait_timeout(2s)` which blocks ~1s until the fence signals
5. **Fence signals**: The delayed work fires and calls `dma_fence_signal()`
6. **Importer accesses buffer**: DMA test (attach/map SG/unmap/detach) + synced CPU test (begin/vmap/read/vunmap/end)
7. **Unload**: Importer then exporter modules are removed

## Build and Test

### Quick Start
```bash
chmod +x run-demo.sh
sudo ./run-demo.sh
```

### Manual Testing
```bash
# Build
cd core/dma-buf/kern-prodcon/part6
make

# Load modules
sudo insmod exporter-fence.ko
sudo insmod importer-fence.ko

# Run userspace mediator (gets fd, passes to importer)
sudo ./test-fence

# Check kernel messages
dmesg | tail -25

# Unload
sudo rmmod importer_fence
sudo rmmod exporter_fence
```

### Expected Output
```
fence_exporter: Fence created (context=<N>, seqno=1)
fence_exporter: Fence attached to dma_buf reservation object
fence_exporter: Initialized — /dev/exporter-fence ready
fence_importer: Initialized — /dev/importer-fence ready
fence_exporter: Delivered fd <N>, hardware work started (fence in ~1s)
fence_importer: Received fd <N> from userspace
fence_importer: Imported DMA-BUF (size=4096) from fd <N>
fence_importer: Inspecting fences in dma_buf reservation object
fence_importer: Found fence — driver: dmabuf-fence-exporter, timeline: exporter-timeline, signaled: no
fence_importer: Waiting for fence (timeout=2s)...
fence_exporter: Simulated transfer complete, signaling fence
fence_exporter: Fence signaled (seqno=1)
fence_importer: Fence signaled! Remaining jiffies: <N>
fence_importer: DMA test — attaching to buffer
fence_exporter: Device fence_importer.0 attached
fence_importer: SG table received (nents=1)
fence_importer:   SG[0] dma_addr=<addr> len=4096
fence_importer: DMA test passed — detached
fence_importer: CPU test — synchronized access (size: 4096)
fence_importer: Buffer content after fence: "hello from fence exporter!"
fence_importer: CPU test passed
fence_importer: All tests passed (fence wait + DMA + CPU)
```

## Key Learning Points

1. **Fence Context**: Each driver allocates a unique context to identify its fences
2. **Reservation Objects**: Every `dma_buf` has an embedded `dma_resv` for fence tracking
3. **Usage Types**: `DMA_RESV_USAGE_WRITE` vs `DMA_RESV_USAGE_READ` controls fence semantics
4. **Deferred Signaling**: Real drivers signal fences from interrupt handlers; this example uses `delayed_work`
5. **Fence Ops**: The `get_driver_name` and `get_timeline_name` callbacks identify fences in debug output
6. **Pipeline Consistency**: Uses the same exporter → app → importer pattern from Part 4, ensuring the curriculum builds progressively without regression
7. **Full Stack**: Combines fences (Part 6) with DMA mapping (Part 2) and sync wrappers (Part 5) in a single importer

## Troubleshooting

- **"Permission denied"**: Run test-fence with `sudo` (needs access to /dev/ nodes)
- **Wait times out**: The fence should signal within ~1s; check dmesg for errors
- **"No such device"**: Ensure both modules are loaded before running test-fence

## License

GPL v2, as specified by `MODULE_LICENSE("GPL v2")`.

## Author

Raghu Bharadwaj (raghu@techveda.org), updated for kernel 6.x.
