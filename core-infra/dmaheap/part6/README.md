# Part 06: Poll-based Completion with dma_fence

## Overview

This example demonstrates poll-based asynchronous completion using `dma_fence` on DMA heap buffers. Instead of using signals (SIGIO) like Part 5, this example uses `poll()` on the driver file descriptor with a custom `.poll` implementation backed by a `dma_fence` callback and wait queue.

- Creating and signaling a `dma_fence`
- `dma_fence_add_callback()` to wire fence signaling to a poll wait queue
- Custom `.poll` fop on the driver device (not relying on `dma_buf_poll()`)
- Truly asynchronous processing via `delayed_work`
- Userspace `poll()` on the driver fd — epoll-compatible

**Files:**
- `importer-poll.c` — Kernel driver with dma_fence, fence callback, custom poll, and delayed_work
- `heap-poll.c` — Userspace test with poll()-based completion wait
- `Makefile` — Build configuration
- `run-demo.sh` — All-in-one demo script

**Prerequisites:** Understanding of Parts 1-5 (import, sync, multi-SG, async notification).

## Context and Scope

The `dma_fence` mechanism is the foundation of synchronization in the DMA subsystem. It's used extensively by:
- **GPU drivers**: To track command buffer completion
- **Media pipelines**: To synchronize encode/decode stages
- **Compositors**: To know when a client buffer is ready for display

### How poll() Works in This Driver

The driver implements a custom `.poll` file operation backed by a wait queue. A `dma_fence_add_callback()` wires fence signaling directly to poll wakeup:

```
User calls poll(driver_fd, POLLIN)
    -> my_dma_poll() registers on poll_wq, checks poll_ready
    -> poll_ready is false: poll() blocks
    -> delayed_work fires after ~1 second
    -> Work function processes buffer, appends " [processed]"
    -> dma_fence_signal() fires the fence callback
    -> fence_signaled_cb() sets poll_ready=true, wakes poll_wq
    -> poll() returns POLLIN
    -> User re-mmaps and reads processed buffer
```

### Why Custom Poll Instead of dma_buf_poll()

The kernel's built-in `dma_buf_poll()` checks the reservation object for unsignaled fences, but its wakeup mechanism is fragile across kernel versions. Real drivers implement their own poll using `dma_fence_add_callback()` to wire fence completion directly to their wait queues — this is both more reliable and more educational.

### True Async vs Part 5

Part 5's `msleep(100)` blocked the ioctl call — the "async" was only in the notification mechanism. This example uses `schedule_delayed_work()` for truly asynchronous processing: the ioctl returns immediately, and processing happens in a separate kernel thread.

## Key APIs

### Fence Lifecycle
1. `dma_fence_context_alloc(1)` — Allocate a unique fence context (once, in init)
2. `dma_fence_init(fence, ops, lock, ctx, seqno)` — Initialize a fence
3. `dma_fence_add_callback(fence, cb, fn)` — Register callback for fence signal
4. `dma_fence_signal(fence)` — Signal completion (triggers callback → wakes poll)
5. `dma_fence_put(fence)` — Release fence reference

### Fence Operations
```c
static const struct dma_fence_ops poll_fence_ops = {
    .get_driver_name = poll_fence_get_driver_name,
    .get_timeline_name = poll_fence_get_timeline_name,
};
```

### Custom Poll
```c
static __poll_t my_dma_poll(struct file *file, poll_table *wait)
{
    poll_wait(file, &poll_wq, wait);
    if (poll_ready)
        return EPOLLIN;
    return 0;
}
```

### Fence Callback
```c
static void fence_signaled_cb(struct dma_fence *fence, struct dma_fence_cb *cb)
{
    poll_ready = true;
    wake_up_interruptible(&poll_wq);
}
```

### Async Work
- `INIT_DELAYED_WORK(&ctx->dwork, work_fn)` — Initialize delayed work
- `schedule_delayed_work(&ctx->dwork, msecs_to_jiffies(1000))` — Schedule ~1s delay
- `cancel_delayed_work_sync()` — Cancel on module unload

### Userspace
- `poll(driver_fd, POLLIN, timeout)` — Wait for fence signal on the driver fd

### IOCTL
- `MY_DMA_IOCTL_POLL_SUBMIT _IOW('M', 9, int)`
- **Device**: `/dev/dummy_dma_poll_device`

## Comparison with Part 5 (SIGIO)

| Feature | Part 5 (SIGIO) | Part 6 (poll) |
|---------|----------------|---------------|
| Notification | Signal-based (SIGIO) | poll() on driver fd |
| Mechanism | fasync + kill_fasync | dma_fence + callback + wait queue |
| Processing | Synchronous (msleep) | Truly async (delayed_work) |
| Standard | POSIX signals | Modern DMA subsystem |
| Multiplexing | One signal per process | epoll-compatible |
| Use case | Legacy async I/O | GPU/media pipelines |

## Build and Test

### Building
```bash
cd core/dmaheap/part6
make
```

### Testing
```bash
sudo insmod importer-poll.ko
sudo ./heap-poll
sudo dmesg | grep dma_poll | tail -15
sudo rmmod importer_poll
make clean
```

Or use the all-in-one script:
```bash
./run-demo.sh
```

### Expected Output

Userspace:
```
=== DMA Poll-based Completion Test ===

Allocated DMA buffer, fd: 4
User wrote: 'Poll test data'
Submitted to driver for processing.
Waiting for completion via poll() on driver fd...
poll() returned: revents=0x1
Buffer ready (fence signaled)!
Processed buffer: 'Poll test data [processed]'

Compare with Part 5 (SIGIO-based):
  Part 5: Uses fasync/SIGIO signals for async notification
  Part 6: Uses poll() on driver fd with dma_fence (modern approach)
```

dmesg:
```
dma_poll: Received DMA buf fd: 4
dma_poll: Imported dma_buf, size: 4096
dma_poll: Created fence (context=<N>, seqno=1)
dma_poll: Fence callback registered for poll wakeup
dma_poll: Scheduled processing, fence signals in ~1s
dma_poll: Processing work started
dma_poll: Processing buffer: 'Poll test data'
dma_poll: Fence signaled, poll() should return for userspace
```

## Important Notes

1. **Poll on driver fd, not dma_buf fd**: The driver implements its own `.poll` backed by a wait queue and fence callback. This avoids reliance on the kernel's `dma_buf_poll()` which has known issues with fence wakeup in recent kernels.
2. **Reference counting**: The work context holds extra references to both the `dma_buf` and `dma_fence` via `get_dma_buf()` and `dma_fence_get()`. These are released after the work function completes.
3. **Fence context**: Each driver should allocate its own fence context via `dma_fence_context_alloc()` to ensure unique context IDs across the system.
4. **Module unload safety**: `cancel_delayed_work_sync()` in the exit function ensures no work is pending. If work is cancelled before running, the exit function manually signals the fence and releases references.

## Troubleshooting

- **poll() times out**: Check dmesg — the delayed_work may have failed. Ensure the 5-second timeout is sufficient for the 1-second processing delay.
- **Module unload hangs**: Ensure the userspace test has exited (closing the driver fd) before running `rmmod`.
- **Permission denied**: Use `sudo` for module and test operations.

## Next Steps

This is the final example in the DMA heap series. For related concepts:
- **Kernel-to-kernel fences**: See `core/dma-buf/kern-prodcon/part6/` for fence synchronization between exporter and importer kernel modules.
- **V4L2 + DMA heap**: See `kernel-drvmodels/v4l2/part6/` for importing DMA heap buffers into a V4L2 OUTPUT device.
- **DRM + DMA heap**: See `kernel-drvmodels/drm/part7/` for importing DMA heap buffers into DRM via PRIME.

## License

GPL, as specified by `MODULE_LICENSE("GPL")`.

## Author

Raghu Bharadwaj (raghu@techveda.org), TECH VEDA.
