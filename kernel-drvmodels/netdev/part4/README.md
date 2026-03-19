<!-- SPDX-License-Identifier: GPL-2.0 -->
<!-- Copyright (c) 2024 TECH VEDA -->
<!-- Author: Raghu Bharadwaj -->

# Part 4: Interrupts, RX Ring & Simple TX DMA

## What This Adds Over Part 3

Part 3 introduced `net_device_ops` with stub implementations (TX just freed
the skb, no interrupts, no DMA).  Part 4 turns those stubs into real
data-path code:

| Feature            | Part 3           | Part 4                              |
|--------------------|------------------|--------------------------------------|
| TX path            | Count + free skb | Streaming DMA map, register writes, ISR |
| RX path            | None             | Coherent DMA ring, ISR, netif_rx()   |
| Interrupts         | None             | request_irq, ISR with 4 event types  |
| DMA                | None             | Coherent (RX) + streaming (TX)       |
| Bus mastering      | No               | pci_set_master                       |
| Queue flow control | No               | netif_stop/wake_queue (1 TX slot)    |

## DMA Comparison: Coherent vs Streaming

| Property        | Coherent (dma_alloc_coherent)       | Streaming (dma_map_single)             |
|-----------------|-------------------------------------|----------------------------------------|
| Lifetime        | Long-lived (allocated once)         | Transient (per-packet)                 |
| Cache behavior  | Uncached / write-combined           | Normal cached, explicit sync           |
| Who allocates   | DMA API allocates buffer            | Driver allocates, API maps it          |
| Use case        | RX buffers (HW writes unpredictably)| TX packets (driver writes, HW reads)   |
| Allocation      | dma_alloc_coherent()                | dma_map_single()                       |
| Free / unmap    | dma_free_coherent()                 | dma_unmap_single()                     |

## Key Structures

```
vnet_priv
  +-- rx_descs[]  (coherent DMA)    <-- array of vnet_hw_desc descriptors
  |     .addr = rx_bufs_dma[i]           points to coherent RX buffer
  |     .flags = VNET_DESC_OWN           hardware owns the slot
  |
  +-- rx_bufs_va[]                  <-- kernel VA for each RX buffer
  +-- rx_bufs_dma[]                 <-- DMA addr for each RX buffer
  |
  +-- tx_skb                        <-- in-flight skb (streaming DMA)
  +-- tx_dma_addr                   <-- mapped DMA addr of skb->data
  +-- tx_len                        <-- length for unmap
  +-- tx_busy                       <-- true while TX in flight
```

## RX Path Flow

1. **open()**: Allocate RX descriptor ring and buffers with `dma_alloc_coherent`
2. **open()**: Initialize each descriptor: `addr = buf_dma, flags = OWN, len = 1518`
3. **open()**: Register ring with platform via `vnet_hw_set_rx_ring()`
4. **open()**: Write RX ring registers (ADDR, SIZE, HEAD, TAIL)
5. **open()**: Enable interrupts and hardware
6. **Hardware**: Writes packet data into buffer, clears OWN, fires RX interrupt
7. **ISR**: `vnet_rx_process()` walks from `rx_tail`:
   - Skip descriptors with OWN set (still owned by hardware)
   - Allocate skb, copy data from coherent buffer
   - Call `netif_rx(skb)` to deliver to the stack
   - Re-post descriptor: set `flags = OWN` so hardware can reuse slot
8. **stop()**: Clear rings, free all coherent allocations

## TX Path Flow (Simple Register-Based, No Ring)

1. **open()**: Initialize TX state (tx_skb=NULL, tx_busy=false)
2. **open()**: Start platform's simple TX engine via `vnet_hw_start_simple_tx(pdev)`
3. **xmit()**: `dma_map_single(skb->data, len, DMA_TO_DEVICE)` -- streaming map
4. **xmit()**: Write DMA address to `VNET_TX_RING_ADDR` register
5. **xmit()**: Write packet length to `VNET_TX_RING_SIZE` register
6. **xmit()**: `netif_stop_queue()` -- only 1 TX slot, block further sends
7. **xmit()**: Write 1 to `VNET_TX_RING_HEAD` as doorbell
8. **Hardware**: Detects doorbell, "transmits" packet, fires TX_COMPLETE interrupt
9. **ISR**: `vnet_tx_complete()`:
   - `dma_unmap_single()` -- release the streaming mapping
   - `dev_kfree_skb_irq()` -- free the skb
   - `netif_wake_queue()` -- allow next packet
10. **stop()**: Stop simple TX engine, unmap any in-flight TX

No descriptor ring is used for TX. The DMA address and length are written
directly to hardware registers.

## Build and Run

```bash
# Build (requires kernel headers and Part 1 symbols)
cd part4
make

# Load modules
sudo insmod ../vnet-platform/vnet_hw_platform.ko
sudo insmod vnet_irq_dma.ko

# Bring up the interface
sudo ip link set vnet0 up
sudo ip addr add 10.0.0.1/24 dev vnet0

# Watch RX stats increment (platform generates synthetic packets)
watch -n 1 'ip -s link show vnet0'

# Send traffic to exercise the TX path
ping -I vnet0 10.0.0.2 &

# Check stats again
ip -s link show vnet0

# Check kernel log for driver messages
dmesg | grep vnet

# Tear down
sudo ip link set vnet0 down
sudo rmmod vnet_irq_dma
sudo rmmod vnet_hw_platform
```

## Demo Script

A `run-demo.sh` script is provided that automates the above steps:

```bash
chmod +x run-demo.sh
sudo ./run-demo.sh
```

## Key Takeaways

- **Coherent DMA** is for buffers the hardware accesses at any time (RX buffers,
  descriptor rings).  No explicit sync needed but may be slower due to uncached
  access.
- **Streaming DMA** is for one-shot transfers (TX packets).  The driver maps
  before telling HW, unmaps after HW signals completion.  Uses normal cached
  memory for better CPU performance.
- **Interrupt handling**: read INT_STATUS, dispatch, acknowledge.  The ISR must
  be fast.
- **Single TX slot** demonstrates `netif_stop_queue` / `netif_wake_queue` flow
  control.
