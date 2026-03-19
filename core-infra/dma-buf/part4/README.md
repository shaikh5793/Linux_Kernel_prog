# Part 04: File Descriptor Sharing Pipeline

## Overview

This example demonstrates a complete file descriptor sharing pipeline between kernel modules through userspace. It shows how DMA-BUF file descriptors can be passed between different kernel modules via a userspace intermediary.

**What you'll learn:**
- Complete FD sharing workflow: exporter -> userspace -> importer
- `dma_buf_fd()` to create a userspace-visible file descriptor
- `dma_buf_get(fd)` to convert fd back to a dma_buf in the importer
- Importer exercising both DMA (attach/map SG) and CPU (vmap) paths
- Misc device IOCTL interfaces for kernel-user communication

**Files:**
- `exporter-fd.c` - Exports buffer with DMA-capable sg_table, provides FD via misc device
- `importer-fd.c` - Receives FD, tests both DMA (attach/map SG) and CPU (vmap) access
- `userfd.c` - Userspace application mediating FD transfer between modules
- `hw/` - Hardware abstraction examples (optional)
- `Makefile` - Build configuration

**Prerequisites:** Completion of Parts 01-03

## Why FD Sharing? Why Not Direct Kernel Symbol Export?

In Parts 1–2 the importer accessed the exporter's buffer via `EXPORT_SYMBOL_GPL` — a
hard compile-time dependency. That only works when both modules **know about each
other at build time** and live in the same kernel tree.

Real hardware pipelines look different:

```
Camera (V4L2)  →  GPU (mesa/render)  →  Display (DRM/KMS)
Video decoder  →  GPU texture upload  →  Wayland compositor
```

The camera driver doesn't know which GPU will process its frames. The GPU driver
doesn't know which display controller will show the result. These are **independent
subsystems maintained by different teams and vendors**. They cannot export symbols
to each other.

**Userspace is the orchestrator** — it is the only entity that knows the full
pipeline topology:

1. App opens `/dev/video0` (V4L2 camera), allocates capture buffers.
2. App obtains DMA-BUF fd via `VIDIOC_EXPBUF`.
3. App passes that fd to the GPU context (`EGL_EXT_image_dma_buf_import`).
4. GPU renders; app obtains a new fd for the output buffer.
5. App passes that fd to the display via `drmModeAddFB2`.

No kernel module in this chain knows about any other. The fd is just a handle
that travels through userspace, connecting arbitrary producers to consumers.

Additional benefits of FD-based sharing:

- **Cross-process**: The fd can be sent over Unix domain sockets (`SCM_RIGHTS`)
  to a different process — e.g. a Wayland client sends its rendered buffer fd
  to the compositor.
- **Security**: Userspace controls which processes get access to which buffers.
- **Flexibility**: The same fd can be imported by multiple consumers
  simultaneously.

> **Analogy**: `EXPORT_SYMBOL` is like hardwiring two chips on a PCB.
> FD sharing is like USB — plug any producer into any consumer, with
> userspace as the cable.

## Context and Scope

The primary purpose of this example is to demonstrate a complete DMA-BUF sharing pipeline:
- **Exporter Role**: Creates a buffer, exports it as a DMA-BUF, and exposes its file descriptor through a misc device interface.
- **Importer Role**: Accepts a file descriptor from userspace, converts it to a DMA-BUF object, and exercises both DMA (attach/map SG/read addresses/unmap/detach) and CPU (vmap/read content/vunmap) access paths.
- **Userspace Role**: Mediates the transfer of the file descriptor between the exporter and importer — just as a real application (compositor, video player) would orchestrate the pipeline.

The scope includes:
- **Single Buffer**: A fixed-size (one page) buffer shared via DMA-BUF.
- **Dual Access**: Importer exercises both DMA mapping (via platform device) and CPU access (vmap).
- **Full Pipeline**: Integrates kernel modules and userspace to showcase FD sharing.
- **DMA Support**: Exporter provides a proper sg_table via `map_dma_buf`, importer attaches and reads DMA addresses.

This setup mirrors real-world workflows where independent kernel drivers share
buffers (e.g. a GPU buffer passed to a display driver), with userspace wiring
them together.

## Foundational Notes on the DMA-BUF API

The DMA-BUF framework unifies buffer sharing with `struct dma_buf` and its operations (`struct dma_buf_ops`). Key concepts for this pipeline include:

### Core Components
- **`struct dma_buf`**:
  - Represents a shared buffer with metadata (size, private data) and a reference count.
  - Managed with `dma_buf_export`, `dma_buf_get`, and `dma_buf_put`.
- **`struct dma_buf_ops`**:
  - Defines callbacks: `vmap`/`vunmap` for kernel CPU access, `release` for cleanup, and placeholders for DMA (`map_dma_buf`).
  - This set uses `vmap` exclusively for CPU access.

### Key Functions
- **`dma_buf_export`**:
  - Creates the DMA-BUF object in the exporter.
- **`dma_buf_fd`**:
  - Converts a DMA-BUF to a file descriptor for userspace sharing.
- **`dma_buf_get`**:
  - Converts a file descriptor back to a DMA-BUF object in the importer.
- **`dma_buf_vmap` and `dma_buf_vunmap`**:
  - Maps/unmaps the buffer for kernel CPU access using `struct iosys_map`.

### Modern API (6.8.x)
- **`struct iosys_map`**: Provides virtual mapping abstraction, replacing deprecated `kmap` APIs.
- **Namespace**: Uses `MODULE_IMPORT_NS(DMA_BUF)` for out-of-tree compatibility.

### Resource Management
- **Reference Counting**: The exporter holds an initial reference, userspace holds a reference via the FD, and the importer adds a temporary reference during validation.
- **Debugfs Pinning**: With `CONFIG_DMA_SHARED_BUFFER_DEBUG`, debugfs may retain a reference, requiring `rmmod -f`.

## Workflow Details

The pipeline demonstrates FD sharing across kernel and userspace components:

1. **Exporter Initialization**:
   - The `exporter-share` module allocates a page-sized buffer with `kmalloc`, writes "hello world!" into it, and exports it as a DMA-BUF named `dmabuf_share_exported`.
   - Registers a misc device (`/dev/exporter-share`) to provide the FD via an `ioctl` command (`EXPORTER_SHARE_GET_DMABUF_FD`).

2. **Importer Initialization**:
   - The `importer-share` module registers a misc device (`/dev/importer-share`) to receive an FD via an `ioctl` command (`IMPORTER_SHARE_SET_DMABUF_FD`).

3. **Userspace FD Retrieval**:
   - The `test-share` application opens `/dev/exporter-share` in read-only mode.
   - Calls `ioctl` with `EXPORTER_SHARE_GET_DMABUF_FD` to retrieve the DMA-BUF file descriptor.

4. **Userspace FD Sharing**:
   - `test-share` opens `/dev/importer-share` in read-only mode.
   - Passes the retrieved FD to the importer via `ioctl` with `IMPORTER_SHARE_SET_DMABUF_FD`.

5. **Importer Validation**:
   - The importer receives the FD, converts it to a `struct dma_buf` using `dma_buf_get`.
   - Maps the buffer with `dma_buf_vmap`, reads its contents ("hello world!"), and logs it for verification.
   - Releases the temporary reference with `dma_buf_put`.

6. **Cleanup**:
   - `test-share` closes all file descriptors.
   - The kernel modules can be unloaded, with the exporter dropping its reference (`dma_buf_put`), potentially triggering `release` if no other references remain.

This workflow validates the FD sharing mechanism, ensuring the buffer’s contents are accessible across the pipeline.

## Example-Specific Details

### Exporter (`exporter-share.c`)
- **Functionality**: Allocates a buffer, exports it as `dmabuf_share_exported`, and provides its FD via `/dev/exporter-share`.
- **Key Implementation**:
  - `exporter_alloc_page`: Creates and exports the buffer.
  - `exporter_ioctl`: Delivers the FD to userspace.

### Importer (`importer-fd.c`)
- **Functionality**: Receives an FD via `/dev/importer-share`, converts it to a DMA-BUF, and exercises both DMA and CPU access.
- **Key Implementation**:
  - Creates a platform device (`fd_importer`) with `dma_set_mask_and_coherent(dev, DMA_BIT_MASK(64))` for DMA attach.
  - `importer_ioctl`: Imports the FD and runs both tests.
  - `importer_test_dma`: `dma_buf_attach` → `dma_buf_map_attachment(DMA_FROM_DEVICE)` → iterates `for_each_sgtable_dma_sg` reading DMA addresses → `dma_buf_unmap_attachment` → `dma_buf_detach`.
  - `importer_test_cpu`: `dma_buf_vmap` → reads buffer content → `dma_buf_vunmap`.

### Userspace App (`userfd.c`)
- **Functionality**: Retrieves the FD from the exporter and passes it to the importer.
- **Key Implementation**:
  - Uses `ioctl` calls to mediate FD transfer between devices.

## Important Notes

1. **Full DMA + CPU Importer**: The importer exercises both paths — DMA attach/map (reading SG addresses) and CPU vmap (reading content).
2. **Misc Devices**: `/dev/exporter-share` and `/dev/importer-share` enable FD sharing via IOCTL.
3. **Reference Counting**: Debugfs may pin the exporter module, requiring `rmmod -f`.
4. **Kernel Version**: Tailored for 6.x with modern APIs (`iosys_map`, `vmap`).
5. **Namespace**: Both modules import the `DMA_BUF` namespace for out-of-tree compatibility.

## Build and Test

### Building
```bash
# Navigate to the directory
cd core/dma-buf/kern-prodcon/part4

# Build all modules
make

# Verify the build
ls *.ko
```

### Testing

1. **Load the exporter module:**
```bash
sudo insmod exporter-fd.ko
```

2. **Load the importer module:**
```bash
sudo insmod importer-fd.ko
```

3. **Run the userspace app:**
```bash
./userfd
```

4. **Check kernel messages:**
```bash
dmesg | tail -20
```

5. **Unload modules:**
```bash
sudo rmmod importer-fd
sudo rmmod exporter-fd
```

### Expected Output

You should see messages indicating:
- Exporter creates DMA-BUF with test data ("hello world!")
- Userspace retrieves fd from exporter, passes to importer
- DMA test: attachment, SG table with DMA addresses, detachment
- CPU test: vmap reads buffer content ("hello world!")
- Both DMA and CPU tests passed

## Key Learning Points

1. **FD Sharing Pipeline**: `dma_buf_fd()` → userspace → `dma_buf_get(fd)` for cross-module sharing
2. **Userspace Intermediary**: Demonstrates userspace role in mediating buffer FDs
3. **Dual Access Modes**: Importer exercises both DMA (SG addresses) and CPU (vmap content)
4. **Misc Device IOCTL**: Kernel-user communication for FD exchange
5. **Reference Management**: Proper ref counting across exporter, userspace, and importer

## Next Steps

After completing this example, proceed to:
- **Part 05**: Buffer access synchronization
- Learn about begin_cpu_access/end_cpu_access for safe concurrent access

## Troubleshooting

- **Import/Export failures**: Ensure correct order of module loading
- **Permission Issues**: Run userspace app as sudo if needed
- **Reference Counting Issues**: Might need `rmmod -f` if debugfs is enabled

## Potential Extensions

- Enable userspace `mmap` for direct buffer access
- Support multiple buffers or dynamic sizing
- Multi-page scattered buffers (see Part 2 Track B)

## License

GPL v2, as specified by `MODULE_LICENSE("GPL v2")`.

## Author

Raghu Bharadwaj (raghu@techveda.org), updated for kernel 6.8.x.
