# Kernel Producer-Consumer DMA-BUF Examples

## Overview

This track demonstrates the **kernel producer-consumer pattern** where kernel modules create buffers (producers) and share them with other kernel modules (consumers). This is the classic DMA-BUF use case that has been the foundation of the framework since its inception.

## Pattern Characteristics

### **Buffer Lifecycle**
1. **Producer Module** allocates memory and creates a DMA-BUF
2. **Consumer Module** accesses the buffer through DMA-BUF APIs
3. **Userspace** (optional) may participate as an intermediary for FD passing

### **Key Concepts**
- **Producer (Exporter)**: Creates and owns the buffer memory
- **Consumer (Importer)**: Processes buffer data without owning memory
- **Inter-module Communication**: Direct kernel-to-kernel sharing
- **Reference Counting**: Automatic cleanup when all users are done

### **part1** (Basic Producer/Consumer)
- **Foundation concepts**: Basic producer/consumer workflow
- **CPU-only access**: Uses vmap for kernel virtual memory
- **Simple pattern**: Direct inter-module buffer sharing

### **part2** (Scatter-Gather Support)
- **DMA capabilities**: Adds scatter-gather table support
- **Hardware readiness**: Prepares for real DMA operations
- **Device attachment**: Shows proper device association

### **part3** (Userspace Memory Mapping)
- **Userspace extension**: Maps kernel buffers to userspace
- **Misc device interface**: /dev nodes for communication
- **Memory mapping**: remap_pfn_range implementation

### **part4** (FD Sharing Pipeline)
- **Complete workflow**: producer→userspace→consumer pipeline
- **File descriptor passing**: Standard UNIX IPC mechanism
- **Multiple participants**: Shows complex sharing scenarios

### **part5** (Buffer Synchronization)
- **Safe access patterns**: begin_cpu_access/end_cpu_access
- **Cache coherency**: Ensures data consistency
- **Concurrent access**: Coordinates multiple accessors

### **part6** (DMA Fence Synchronization)
- **Software fences**: dma_fence creation, signaling, and waiting
- **Reservation objects**: Attaching fences to dma_buf via dma_resv
- **Asynchronous completion**: Delayed work simulating hardware completion

## When to Use This Pattern

✅ **Good for:**
- Producer-consumer communication between kernel modules
- Hardware-specific buffer formats
- Zero-copy operations between kernel modules
- Graphics/video pipeline components
- Custom hardware with specific memory requirements

❌ **Not ideal for:**
- Simple userspace-driven applications
- Standard system memory allocation
- High-frequency small allocations
- Applications without hardware-specific needs

## Real-World Examples

- **Graphics**: GPU driver (producer) exports framebuffer, display driver (consumer) imports
- **Camera**: ISP driver (producer) exports processed frames, encoder (consumer) imports
- **Media**: Decoder (producer) exports frames, renderer (consumer) imports for display
- **Networking**: DMA engine (producer) exports packet buffers, protocol stack (consumer) imports
