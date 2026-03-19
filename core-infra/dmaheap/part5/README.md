# Part 05: Asynchronous Processing with SIGIO

## Overview

This example demonstrates an end-to-end asynchronous DMA buffer processing pipeline. The application writes frame data into a shared buffer and submits it to the driver, which processes it asynchronously and notifies the application via SIGIO when complete.

- Asynchronous notification via `fasync` / `kill_fasync` / `SIGIO`
- Multi-frame processing loop (5 frames)
- Signal handler setup with `sigaction`
- Simulated DMA transfer with `msleep()` delay

**Files:**
- `importer-async.c` — Kernel driver with async processing and SIGIO notification
- `heap-async.c` — Userspace test with signal handler and multi-frame loop
- `Makefile` — Build configuration
- `run-demo.sh` — All-in-one demo script

**Prerequisites:** Understanding of Parts 1-4 (import, array, sync, multi-SG).

## Context and Scope

In real media pipelines, processing is asynchronous — the application submits a buffer and continues doing other work until notified that processing is complete. This example introduces:

- **SIGIO signals**: The traditional Linux mechanism for asynchronous I/O notification. The driver signals the application when it finishes processing a buffer.
- **Frame loop**: The test application processes 5 "frames" in sequence, each time writing new data, submitting, and waiting for the signal.
- **fasync infrastructure**: The driver implements `fasync` file operations to register the application for SIGIO delivery.

### Async Pipeline Flow
```
User writes "frame1" → ioctl → Driver:
  1. begin_cpu_access → vmap → read "frame1" → vunmap → end_cpu_access
  2. attach → map → get DMA addr → simulate_dma_transfer(100ms)
  3. kill_fasync(SIGIO) → User receives signal
User writes "frame2" → ioctl → ...repeat...
```

## Key APIs

### Driver-side Async
- `fasync_helper(fd, filp, on, &async_queue)` — Register/unregister for async notification
- `kill_fasync(&async_queue, SIGIO, POLL_IN)` — Send SIGIO to registered applications
- `my_dma_fasync()` — File operation callback for fasync setup

### Userspace Async
- `sigaction(SIGIO, &sa, NULL)` — Install signal handler
- `fcntl(fd, F_SETFL, O_ASYNC)` — Enable async mode on file descriptor
- `fcntl(fd, F_SETOWN, getpid())` — Set signal recipient to current process

### DMA Access
- Same synchronized CPU + DMA pattern as Part 3
- `simulate_dma_transfer()` with 100ms `msleep()` delay

### IOCTL
- `MY_DMA_IOCTL_PROCESS_ASYNC _IOW('M', 7, int)`
- **Device**: `/dev/dummy_dma_async_device`

## What Changed from Part 4

| Aspect | Part 4 | Part 5 |
|--------|--------|--------|
| Focus | SG table traversal | Async notification |
| Notification | None (synchronous) | SIGIO signal |
| Processing | Single buffer | 5-frame loop |
| File ops | ioctl only | + open, release, fasync |
| Transfer delay | None | 100ms msleep |
| Device name | `dummy_dma_multisg_device` | `dummy_dma_async_device` |

## Build and Test

### Building
```bash
cd core/dmaheap/part5
make
```

### Testing
```bash
sudo insmod importer-async.ko
sudo ./heap-async
sudo dmesg | grep dma_async | tail -20
sudo rmmod importer_async
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
User wrote: frame1
Received SIGIO: DMA processing complete.
Driver processed frame1 and signaled completion.
...
User wrote: frame5
Received SIGIO: DMA processing complete.
Driver processed frame5 and signaled completion.
```

dmesg:
```
dma_async: Received DMA buf fd: 4
dma_async: Buffer content: frame1
simulate_dma_transfer: Programming DMA transfer:
  DMA Source Address: 0x00000000XXXXXXXX
  Transfer Size: 4096 bytes
simulate_dma_transfer: DMA transfer simulated successfully.
...
```

## Important Notes

1. **SIGIO is process-level**: Only one process can receive SIGIO per fd. For multi-process scenarios, consider `epoll` or `poll` (see Part 6).
2. **Signal safety**: The SIGIO handler only sets a `volatile sig_atomic_t` flag — no complex operations in signal context.
3. **Processing is synchronous in the driver**: Despite the "async" name, `msleep(100)` blocks the ioctl call. A real driver would use workqueues or tasklets. Part 6 demonstrates true async with `delayed_work`.
4. **Cleanup on release**: The driver calls `my_dma_fasync(-1, file, 0)` in `release` to deregister async notification when the fd is closed.

## Troubleshooting

- **No SIGIO received**: Ensure `fcntl(F_SETFL, O_ASYNC)` and `fcntl(F_SETOWN, getpid())` are both called on the driver fd.
- **Test hangs**: The `while (!dma_done)` loop waits for SIGIO. If the driver fails before `kill_fasync()`, the test will spin forever. Check dmesg for errors.
- **Permission denied**: Use `sudo` for module and test operations.

## Next Steps

Proceed to **Part 6** to learn about poll-based completion using `dma_fence` on DMA heap buffers — the modern alternative to SIGIO.

## License

GPL, as specified by `MODULE_LICENSE("GPL")`.

## Author

Raghu Bharadwaj (raghu@techveda.org), TECH VEDA.
