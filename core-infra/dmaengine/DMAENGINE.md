<!--
Copyright (c) 2024 TECH VEDA
Author: Raghu Bharadwaj
This code is dual-licensed under the MIT License and GPL v2
-->

This document explains how our DMA engine API calls relate to the underlying EDMA hardware on the BeagleBone Black.

### Hardware Foundation

The examples utilize:
- **EDMA3 Channel Controller (TPCC)** at `0x49000000`
- **Transfer Controllers (TPTC0-2)** for actual data movement
- **64 DMA channels** with dedicated memory-copy channels (20, 21)
- **3 Transfer Controllers** with different priorities (7, 5, 0)


### 1. Simple Memory-to-Memory DMA 

**Hardware Mapping:**
```c
// Requests DMA_MEMCPY capability
dma_cap_set(DMA_MEMCPY, dma_mask);
dma_chan = dma_request_channel(dma_mask, NULL, NULL);
```

**What happens in hardware:**
- Allocates one of the dedicated memcpy channels (20 or 21)
- Configures EDMA PaRAM set with source/destination addresses
- Uses TPTC with highest available priority
- Triggers interrupt on completion (edma3_ccint)

**Device Tree Connection:**
```dts
ti,edma-memcpy-channels = <20 21>;  // Dedicated channels for our example
```

### 2. Peripheral-to-Memory DMA

**Hardware Mapping:**
```c
// Requests DMA_SLAVE capability for peripheral transfers
dma_cap_set(DMA_SLAVE, dma_mask);
dma_chan = dma_request_channel(dma_mask, NULL, NULL);
```

**What happens in hardware:**
- Allocates a slave channel (0-19, 22-63)
- Configures EDMA for peripheral-triggered transfers
- Sets up event synchronization with peripheral
- Uses appropriate TPTC based on system load

**Device Tree Connection:**
```dts
dma-requests = <64>;  // Total channels available
interrupts = <12 13 14>;  // Completion, memory error, channel error
```

## EDMA Channel Allocation

```
Channel Layout (64 total):
┌─────────────────────────────────────────────────────────────┐
│ 0-19  │ 20-21      │ 22-63                                 │
│ Slave │ MemCpy     │ Slave                                 │
│ DMA   │ Dedicated  │ DMA                                   │
└─────────────────────────────────────────────────────────────┘
       ↑              ↑
  Used by          Used by
  peripheral_dma   simple_dma_m2m
```

## Transfer Controllers Priority

```
TPTC Priority Assignment:
┌──────────┬──────────┬─────────────────────────────────┐
│ TPTC     │ Priority │ Typical Usage                   │
├──────────┼──────────┼─────────────────────────────────┤
│ TPTC0    │ 7 (High) │ High-priority/real-time data    │
│ TPTC1    │ 5 (Med)  │ General purpose transfers       │
│ TPTC2    │ 0 (Low)  │ Background/bulk transfers       │
└──────────┴──────────┴─────────────────────────────────┘
```

## Memory Mapping and DMA Coherency

handle DMA coherency correctly:

```c
// Map memory for DMA access
dma_src_addr = dma_map_single(dma_chan->device->dev, src_buffer, 
                              DMA_BUFF_SIZE, DMA_TO_DEVICE);

// Unmap after transfer to make data CPU-accessible
dma_unmap_single(dma_chan->device->dev, dma_src_addr, 
                 DMA_BUFF_SIZE, DMA_TO_DEVICE);
```

**Hardware Implication:**
- Ensures cache coherency between CPU and EDMA
- Manages memory barriers for ARM Cortex-A8
- Handles L1/L2 cache synchronization

## Interrupt Handling

use completion callbacks that map to EDMA interrupts:

```c
static void dma_transfer_callback(void *completion)
{
    pr_info("DMA transfer completed successfully\n");
    complete((struct completion *)completion);
}
```

**Hardware Connection:**
- Maps to IRQ 12 (edma3_ccint) from device tree
- Triggered by EDMA3CC when transfer completes
- Handled by DMA engine framework in kernel

## Performance Considerations

### Buffer Alignment
```c
#define DMA_BUFF_SIZE 1024  // Power-of-2 for optimal performance
src_buffer = kzalloc(DMA_BUFF_SIZE, GFP_KERNEL);  // Kernel allocator ensures alignment
```

### Transfer Size
- EDMA optimizes for burst transfers
- Larger transfers generally more efficient
- 1KB-2KB buffers good balance for educational examples

## Testing on BeagleBone Black

1. **Module Load**: `insmod sdma_m2m.ko`
   - Driver requests EDMA channel via DMA engine
   - Channel allocated from available pool
   - PaRAM set configured in EDMA memory

2. **Transfer Execution**:
   - CPU writes to EDMA registers via memory mapping
   - EDMA performs transfer without CPU intervention
   - Interrupt generated on completion

3. **Verification**: 
   - CPU reads transferred data after cache sync
   - Memory comparison validates transfer success

## Real-World Applications

These patterns extend to:
- **SPI/UART DMA**: High-speed serial communication
- **Audio/Video**: Streaming data to/from peripherals
- **Network**: Packet buffer management
- **Storage**: Block device I/O acceleration

## Debugging Tips

Monitor EDMA activity:
```bash
# Check DMA engine statistics
cat /sys/kernel/debug/dmaengine/summary

# Monitor interrupts
cat /proc/interrupts | grep edma

# Check for DMA mapping errors
dmesg | grep -i dma
```

