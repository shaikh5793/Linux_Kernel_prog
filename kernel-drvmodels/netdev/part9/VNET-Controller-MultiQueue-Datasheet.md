<!-- SPDX-License-Identifier: GPL-2.0 -->
<!-- Copyright (c) 2024 TECH VEDA -->
<!-- Author: Raghu Bharadwaj -->

# VNET Controller Multi-Queue Datasheet

## 1. Overview: Why Multi-Queue?

A single-queue NIC forces all packet processing through one CPU and one
descriptor ring. This creates two bottlenecks:

**Head-of-line blocking:** A single slow flow stalls every packet behind it
in the shared ring.

**Single-CPU saturation:** One CPU handles all interrupts, NAPI polling,
and protocol processing while other CPUs sit idle.

```
  SINGLE-QUEUE BOTTLENECK
  ========================

  CPU 0 (100% busy)        CPU 1 (idle)       CPU 2 (idle)       CPU 3 (idle)
    |
    v
  +------------------------------------------------------------------+
  | Single TX Ring         Single RX Ring                             |
  | [pkt][pkt][pkt][pkt]   [pkt][pkt][pkt][pkt]                     |
  +------------------------------------------------------------------+
    |                        ^
    v                        |
  +------------------------------------------------------------------+
  |                        NIC Hardware                               |
  |                      (1 IRQ vector)                               |
  +------------------------------------------------------------------+

  All traffic funnels through CPU 0. If flow A is slow, flows B/C/D wait.
```

Multi-queue solves this by giving each CPU its own independent queue with
its own ring buffer, NAPI instance, and interrupt vector:

```
  MULTI-QUEUE ARCHITECTURE
  ========================

  CPU 0            CPU 1            CPU 2            CPU 3
    |                |                |                |
    v                v                v                v
  +------------+  +------------+  +------------+  +------------+
  | Queue 0    |  | Queue 1    |  | Queue 2    |  | Queue 3    |
  | TX Ring 0  |  | TX Ring 1  |  | TX Ring 2  |  | TX Ring 3  |
  | RX Ring 0  |  | RX Ring 1  |  | RX Ring 2  |  | RX Ring 3  |
  | NAPI 0     |  | NAPI 1     |  | NAPI 2     |  | NAPI 3     |
  | IRQ vec 0  |  | IRQ vec 1  |  | IRQ vec 2  |  | IRQ vec 3  |
  +------------+  +------------+  +------------+  +------------+
        |                |                |                |
        v                v                v                v
  +------------------------------------------------------------------+
  |                      NIC Hardware                                 |
  |              4 independent DMA engines                            |
  |              4 interrupt vectors (MSI-X)                          |
  +------------------------------------------------------------------+
```

## 2. Multi-Queue Architecture Detail

Each queue is a fully independent data path. No locks are shared between
queues during normal operation.

```
  QUEUE N INTERNALS
  =================

  +---------------------------------------------+
  |  struct vnet_queue (per-queue state)         |
  |                                              |
  |  TX Descriptor Ring          RX Descriptor   |
  |  +----+----+----+----+      Ring             |
  |  | d0 | d1 | d2 | d3 |      +----+----+--+  |
  |  +----+----+----+----+      | d0 | d1 |..|  |
  |    ^              ^          +----+----+--+  |
  |    |              |            ^          ^   |
  |  tail           head         tail       head  |
  |  (HW writes)   (SW writes)  (SW reads) (HW)  |
  |                                              |
  |  struct napi_struct napi;                    |
  |  int irq_vector;                             |
  |  spinlock_t tx_lock;                         |
  +---------------------------------------------+

  Head/Tail Pointer Convention:
  ----------------------------
  TX: Software advances HEAD (new packets to send)
      Hardware advances TAIL (completed transmissions)

  RX: Hardware advances HEAD (new packets received)
      Software advances TAIL (processed by NAPI poll)
```

## 3. Per-Queue Register Map

All queue registers sit in the BAR0 MMIO region. Each queue occupies a
16-byte TX block and a 16-byte RX block at fixed offsets.

### Global Registers

```
  +--------+------+-----+-----------------------------------------------+
  | Offset | Size | R/W | Description                                   |
  +--------+------+-----+-----------------------------------------------+
  | 0x000  |  4   | R/W | DEVICE_CTRL   -- global enable, reset         |
  | 0x004  |  4   | R   | DEVICE_STATUS -- link, speed                  |
  | 0x008  |  4   | R/W | INT_MASK      -- global interrupt mask        |
  | 0x00C  |  4   | R/W | INT_STATUS    -- global interrupt status      |
  | 0x040  |  4   | R/W | NUM_QUEUES    -- number of active queues      |
  | 0x044  |  4   | R/W | QUEUE_CTRL    -- per-queue enable bitmask     |
  |        |      |     |   bit 0 = queue 0, bit 1 = queue 1, etc.      |
  | 0x048  |  4   | R/W | QUEUE_INT_MAP -- queue-to-IRQ vector mapping  |
  +--------+------+-----+-----------------------------------------------+
```

### QUEUE_CTRL Register (0x044) Bit Layout

```
  31                              4  3    2    1    0
  +----------------------------------+----+----+----+----+
  |           Reserved               | Q3 | Q2 | Q1 | Q0 |
  +----------------------------------+----+----+----+----+

  Q0..Q3: 1 = queue enabled, 0 = queue disabled

  Example: Write 0x0F to enable all 4 queues
           Write 0x03 to enable only queues 0 and 1
```

### Per-Queue TX Registers

```
  Base formula: TX_BASE(n) = 0x100 + (n * 0x10)

  +--------+------+-----+-----------------------------------------------+
  | Offset | Size | R/W | Description                                   |
  +--------+------+-----+-----------------------------------------------+
  |                     QUEUE 0 TX (base 0x100)                         |
  +--------+------+-----+-----------------------------------------------+
  | 0x100  |  4   | R/W | TX0_RING_BASE_LO  -- ring phys addr [31:0]   |
  | 0x104  |  4   | R/W | TX0_RING_BASE_HI  -- ring phys addr [63:32]  |
  | 0x108  |  4   | R/W | TX0_HEAD           -- software write pointer  |
  | 0x10C  |  4   | R   | TX0_TAIL           -- hardware read pointer   |
  | 0x10E  |  2   | R/W | TX0_RING_SIZE      -- number of descriptors   |
  | 0x10F  |  1   | R/W | TX0_CTRL           -- queue start/stop        |
  +--------+------+-----+-----------------------------------------------+
  |                     QUEUE 1 TX (base 0x110)                         |
  +--------+------+-----+-----------------------------------------------+
  | 0x110  |  4   | R/W | TX1_RING_BASE_LO                             |
  | 0x114  |  4   | R/W | TX1_RING_BASE_HI                             |
  | 0x118  |  4   | R/W | TX1_HEAD                                     |
  | 0x11C  |  4   | R   | TX1_TAIL                                     |
  | 0x11E  |  2   | R/W | TX1_RING_SIZE                                |
  | 0x11F  |  1   | R/W | TX1_CTRL                                     |
  +--------+------+-----+-----------------------------------------------+
  |                     QUEUE 2 TX (base 0x120)                         |
  +--------+------+-----+-----------------------------------------------+
  | 0x120  |  4   | R/W | TX2_RING_BASE_LO                             |
  | 0x124  |  4   | R/W | TX2_RING_BASE_HI                             |
  | 0x128  |  4   | R/W | TX2_HEAD                                     |
  | 0x12C  |  4   | R   | TX2_TAIL                                     |
  | 0x12E  |  2   | R/W | TX2_RING_SIZE                                |
  | 0x12F  |  1   | R/W | TX2_CTRL                                     |
  +--------+------+-----+-----------------------------------------------+
  |                     QUEUE 3 TX (base 0x130)                         |
  +--------+------+-----+-----------------------------------------------+
  | 0x130  |  4   | R/W | TX3_RING_BASE_LO                             |
  | 0x134  |  4   | R/W | TX3_RING_BASE_HI                             |
  | 0x138  |  4   | R/W | TX3_HEAD                                     |
  | 0x13C  |  4   | R   | TX3_TAIL                                     |
  | 0x13E  |  2   | R/W | TX3_RING_SIZE                                |
  | 0x13F  |  1   | R/W | TX3_CTRL                                     |
  +--------+------+-----+-----------------------------------------------+
```

### Per-Queue RX Registers

```
  Base formula: RX_BASE(n) = 0x140 + (n * 0x10)

  +--------+------+-----+-----------------------------------------------+
  | Offset | Size | R/W | Description                                   |
  +--------+------+-----+-----------------------------------------------+
  |                     QUEUE 0 RX (base 0x140)                         |
  +--------+------+-----+-----------------------------------------------+
  | 0x140  |  4   | R/W | RX0_RING_BASE_LO  -- ring phys addr [31:0]   |
  | 0x144  |  4   | R/W | RX0_RING_BASE_HI  -- ring phys addr [63:32]  |
  | 0x148  |  4   | R   | RX0_HEAD           -- hardware write pointer  |
  | 0x14C  |  4   | R/W | RX0_TAIL           -- software read pointer   |
  | 0x14E  |  2   | R/W | RX0_RING_SIZE      -- number of descriptors   |
  | 0x14F  |  1   | R/W | RX0_CTRL           -- queue start/stop        |
  +--------+------+-----+-----------------------------------------------+
  |                     QUEUE 1 RX (base 0x150)                         |
  +--------+------+-----+-----------------------------------------------+
  | 0x150  |  4   | R/W | RX1_RING_BASE_LO                             |
  | 0x154  |  4   | R/W | RX1_RING_BASE_HI                             |
  | 0x158  |  4   | R   | RX1_HEAD                                     |
  | 0x15C  |  4   | R/W | RX1_TAIL                                     |
  | 0x15E  |  2   | R/W | RX1_RING_SIZE                                |
  | 0x15F  |  1   | R/W | RX1_CTRL                                     |
  +--------+------+-----+-----------------------------------------------+
  |                     QUEUE 2 RX (base 0x160)                         |
  +--------+------+-----+-----------------------------------------------+
  | 0x160  |  4   | R/W | RX2_RING_BASE_LO                             |
  | 0x164  |  4   | R/W | RX2_RING_BASE_HI                             |
  | 0x168  |  4   | R   | RX2_HEAD                                     |
  | 0x16C  |  4   | R/W | RX2_TAIL                                     |
  | 0x16E  |  2   | R/W | RX2_RING_SIZE                                |
  | 0x16F  |  1   | R/W | RX2_CTRL                                     |
  +--------+------+-----+-----------------------------------------------+
  |                     QUEUE 3 RX (base 0x170)                         |
  +--------+------+-----+-----------------------------------------------+
  | 0x170  |  4   | R/W | RX3_RING_BASE_LO                             |
  | 0x174  |  4   | R/W | RX3_RING_BASE_HI                             |
  | 0x178  |  4   | R   | RX3_HEAD                                     |
  | 0x17C  |  4   | R/W | RX3_TAIL                                     |
  | 0x17E  |  2   | R/W | RX3_RING_SIZE                                |
  | 0x17F  |  1   | R/W | RX3_CTRL                                     |
  +--------+------+-----+-----------------------------------------------+
```

### Register Summary Map

```
  0x000 +------------------+
        | Global Registers |
  0x040 +------------------+
        | Queue Control    |
  0x04C +------------------+
        |    (reserved)    |
  0x100 +------------------+
        | TX Queue 0 Regs  |
  0x110 +------------------+
        | TX Queue 1 Regs  |
  0x120 +------------------+
        | TX Queue 2 Regs  |
  0x130 +------------------+
        | TX Queue 3 Regs  |
  0x140 +------------------+
        | RX Queue 0 Regs  |
  0x150 +------------------+
        | RX Queue 1 Regs  |
  0x160 +------------------+
        | RX Queue 2 Regs  |
  0x170 +------------------+
        | RX Queue 3 Regs  |
  0x180 +------------------+
```

## 4. TX Queue Selection

When the kernel has a packet to transmit, it must choose one of the 4 TX
queues. Three mechanisms participate, evaluated in this order:

```
  APPLICATION
      |
      v
  +------------------+
  | sk_buff created  |
  +------------------+
      |
      v
  +---------------------------------------+
  | 1. ndo_select_queue() defined?        |
  |    YES --> driver picks queue          |------> Queue N
  |    NO  --> continue                    |
  +---------------------------------------+
      |
      v
  +---------------------------------------+
  | 2. XPS mapping configured?            |
  |    YES --> CPU affinity selects queue  |------> Queue N
  |    NO  --> continue                    |
  +---------------------------------------+
      |
      v
  +---------------------------------------+
  | 3. skb flow hash                      |
  |    hash(src_ip, dst_ip, src_port,     |
  |         dst_port, proto) % num_queues |------> Queue N
  +---------------------------------------+
```

### XPS (Transmit Packet Steering)

XPS maps CPUs to TX queues so that a CPU always uses the same queue,
avoiding cross-CPU contention on the TX ring lock.

```
  XPS Configuration
  =================

  CPU 0 -----> TX Queue 0       /sys/class/net/vnet0/queues/tx-0/xps_cpus = 01
  CPU 1 -----> TX Queue 1       /sys/class/net/vnet0/queues/tx-1/xps_cpus = 02
  CPU 2 -----> TX Queue 2       /sys/class/net/vnet0/queues/tx-2/xps_cpus = 04
  CPU 3 -----> TX Queue 3       /sys/class/net/vnet0/queues/tx-3/xps_cpus = 08

  The xps_cpus value is a hex CPU bitmask.
  01 = CPU 0, 02 = CPU 1, 04 = CPU 2, 08 = CPU 3.
```

### ndo_select_queue

The driver can override queue selection entirely:

```c
static u16 vnet_select_queue(struct net_device *ndev,
                             struct sk_buff *skb,
                             struct net_device *sb_dev)
{
    /* Example: steer by protocol */
    if (skb->protocol == htons(ETH_P_ARP))
        return 0;                          /* ARP always on queue 0 */

    /* Otherwise use flow hash */
    return skb_tx_hash(ndev, skb);
}
```

## 5. RX Queue Steering

On the receive side, the hardware hashes incoming packets and steers them
to the appropriate RX queue. Each queue fires its own MSI-X interrupt,
which schedules its own NAPI instance.

```
  PACKET ARRIVES AT NIC
         |
         v
  +-----------------------------+
  | Hardware RSS Hash           |
  | hash(src, dst, ports, proto)|
  +-----------------------------+
         |
         |  hash % 4
         v
  +------+-------+-------+-------+
  | RX Q0| RX Q1 | RX Q2 | RX Q3 |
  +------+-------+-------+-------+
     |       |        |       |
     v       v        v       v
  IRQ 0   IRQ 1    IRQ 2   IRQ 3    (MSI-X vectors)
     |       |        |       |
     v       v        v       v
  NAPI 0  NAPI 1   NAPI 2  NAPI 3   (independent poll loops)
     |       |        |       |
     v       v        v       v
  CPU 0   CPU 1    CPU 2   CPU 3     (parallel processing)
```

### RPS (Receive Packet Steering) -- Software Fallback

When hardware RSS is unavailable, RPS provides software-based RX steering.
The kernel hashes the packet in the interrupt handler and enqueues it to
the target CPU's backlog.

```
  RPS Configuration
  =================

  /sys/class/net/vnet0/queues/rx-0/rps_cpus = 01    (CPU 0 handles queue 0)
  /sys/class/net/vnet0/queues/rx-1/rps_cpus = 02    (CPU 1 handles queue 1)
  /sys/class/net/vnet0/queues/rx-2/rps_cpus = 04    (CPU 2 handles queue 2)
  /sys/class/net/vnet0/queues/rx-3/rps_cpus = 08    (CPU 3 handles queue 3)

  To spread a single RX queue across all CPUs:
  /sys/class/net/vnet0/queues/rx-0/rps_cpus = 0f    (CPUs 0-3)
```

## 6. XPS/RPS Configuration Reference

```
  SYSFS LAYOUT FOR MULTI-QUEUE DEVICE
  ====================================

  /sys/class/net/vnet0/queues/
  |
  +-- tx-0/
  |   +-- xps_cpus          CPU bitmask for TX queue 0
  |   +-- tx_timeout         timeout counter
  |   +-- byte_queue_limits/
  |       +-- limit           BQL byte limit
  |       +-- limit_max
  |       +-- limit_min
  |
  +-- tx-1/
  |   +-- xps_cpus
  |   +-- ...
  |
  +-- tx-2/
  |   +-- xps_cpus
  |   +-- ...
  |
  +-- tx-3/
  |   +-- xps_cpus
  |   +-- ...
  |
  +-- rx-0/
  |   +-- rps_cpus           CPU bitmask for RPS
  |   +-- rps_flow_cnt       RFS flow table size
  |
  +-- rx-1/
  |   +-- rps_cpus
  |   +-- ...
  |
  +-- rx-2/
  |   +-- rps_cpus
  |   +-- ...
  |
  +-- rx-3/
      +-- rps_cpus
      +-- ...
```

### Quick Setup Script

```bash
#!/bin/bash
# Assign each TX queue to one CPU (XPS)
echo 1 > /sys/class/net/vnet0/queues/tx-0/xps_cpus
echo 2 > /sys/class/net/vnet0/queues/tx-1/xps_cpus
echo 4 > /sys/class/net/vnet0/queues/tx-2/xps_cpus
echo 8 > /sys/class/net/vnet0/queues/tx-3/xps_cpus

# Assign each RX queue to one CPU (RPS)
echo 1 > /sys/class/net/vnet0/queues/rx-0/rps_cpus
echo 2 > /sys/class/net/vnet0/queues/rx-1/rps_cpus
echo 4 > /sys/class/net/vnet0/queues/rx-2/rps_cpus
echo 8 > /sys/class/net/vnet0/queues/rx-3/rps_cpus
```

## 7. alloc_netdev_mqs vs alloc_netdev

### Single-Queue (Parts 2-7)

```c
ndev = alloc_netdev(sizeof(struct vnet_priv),
                    "vnet%d",
                    NET_NAME_USER,
                    ether_setup);
```

This calls `alloc_netdev_mqs()` internally with `txqs=1, rxqs=1`.

### Multi-Queue (Part 9)

```c
ndev = alloc_netdev_mqs(sizeof(struct vnet_priv),
                        "vnet%d",
                        NET_NAME_USER,
                        ether_setup,
                        4,              /* 4 TX queues */
                        4);             /* 4 RX queues */
```

Then in `ndo_open`:

```c
netif_set_real_num_tx_queues(ndev, 4);
netif_set_real_num_rx_queues(ndev, 4);
```

### Data Structure Impact

```
  alloc_netdev (1 queue)          alloc_netdev_mqs (4+4 queues)
  ========================        ================================

  struct net_device                struct net_device
  +------------------+             +------------------+
  | _tx[0]           |             | _tx[0]           |
  | (netdev_queue)   |             | _tx[1]           |
  +------------------+             | _tx[2]           |
                                   | _tx[3]           |
                                   +------------------+
                                   | (4 netdev_queue  |
                                   |  structs, each   |
                                   |  with own lock)  |
                                   +------------------+

  struct vnet_priv                 struct vnet_priv
  +------------------+             +------------------+
  | napi (1)         |             | napi[4]          |
  | tx_ring (1)      |             | tx_ring[4]       |
  | rx_ring (1)      |             | rx_ring[4]       |
  +------------------+             | irq_vec[4]       |
                                   +------------------+
```

## 8. Per-Queue NAPI

Each queue has its own `napi_struct`. The ISR for queue N schedules only
NAPI instance N. The poll function for instance N processes only queue N's
RX ring.

### Registration (in probe)

```c
for (i = 0; i < NUM_QUEUES; i++) {
    netif_napi_add(ndev, &priv->queue[i].napi,
                   vnet_poll_queue);
}
```

### ISR (per-queue)

```c
static irqreturn_t vnet_irq_queue(int irq, void *data)
{
    struct vnet_queue *q = data;          /* queue-specific context */

    /* Disable interrupts for THIS queue only */
    vnet_queue_irq_disable(q);

    /* Schedule NAPI for THIS queue only */
    napi_schedule(&q->napi);

    return IRQ_HANDLED;
}
```

### Poll (per-queue)

```c
static int vnet_poll_queue(struct napi_struct *napi, int budget)
{
    struct vnet_queue *q = container_of(napi, struct vnet_queue, napi);
    int work_done = 0;

    /* Process only THIS queue's RX ring */
    while (work_done < budget && vnet_rx_pending(q)) {
        struct sk_buff *skb = vnet_rx_one(q);
        skb_record_rx_queue(skb, q->index);   /* tag with queue number */
        napi_gro_receive(napi, skb);
        work_done++;
    }

    /* Also clean THIS queue's TX completions */
    vnet_tx_complete(q);

    if (work_done < budget) {
        napi_complete_done(napi, work_done);
        vnet_queue_irq_enable(q);             /* re-enable for THIS queue */
    }

    return work_done;
}
```

### Flow Diagram

```
  PACKET RECEIVED ON QUEUE 2
  ==========================

  NIC writes descriptor to RX Ring 2
       |
       v
  NIC fires MSI-X vector 2
       |
       v
  vnet_irq_queue(irq=vec2, data=&priv->queue[2])
       |
       +---> Disable IRQ for queue 2 only
       +---> napi_schedule(&priv->queue[2].napi)
                  |
                  v
            Kernel schedules poll on CPU 2
                  |
                  v
            vnet_poll_queue(&priv->queue[2].napi, budget=64)
                  |
                  +---> Read RX Ring 2 descriptors
                  +---> Build skb, tag: skb_record_rx_queue(skb, 2)
                  +---> napi_gro_receive(napi, skb)
                  +---> Clean TX Ring 2 completions
                  +---> If done: napi_complete + re-enable queue 2 IRQ

  Queues 0, 1, 3 are completely unaffected.
```

### Why Per-Queue NAPI Matters

```
  SHARED NAPI (bad)                PER-QUEUE NAPI (good)
  ================                 =====================

  Queue 0 --+                      Queue 0 --> NAPI 0 --> CPU 0
  Queue 1 --+--> NAPI --> CPU 0    Queue 1 --> NAPI 1 --> CPU 1
  Queue 2 --+                      Queue 2 --> NAPI 2 --> CPU 2
  Queue 3 --+                      Queue 3 --> NAPI 3 --> CPU 3

  CPU 0 processes all 4 queues     Each CPU processes 1 queue
  serially. No parallelism.        in parallel. 4x throughput.
```

## 9. Platform Calls

The VNET platform layer provides hardware-abstraction helpers that the
driver calls during `open()` and `stop()` to register and deregister
descriptor rings with the device. Multi-queue TX uses per-queue variants;
RX currently uses the legacy single-queue API (queue 0).

### Per-Queue TX Ring Registration

| Function | Description |
|----------|-------------|
| `vnet_hw_set_tx_ring_queue(pdev, queue, ring_va, count)` | Per-queue TX ring registration. Called in `open()` for each queue. Programs the TX ring base address and size into the hardware registers for the specified queue index. |
| `vnet_hw_clear_tx_ring_queue(pdev, queue)` | Per-queue TX ring deregistration. Called in `stop()` for each queue. Clears the TX ring base and size registers, ensuring the hardware stops DMA on that queue. |

### RX Ring Registration (Legacy API)

| Function | Description |
|----------|-------------|
| `vnet_hw_set_rx_ring(pdev, ring_va, count, bufs_va, buf_size)` | RX ring registration (queue 0 via legacy API). Called in `open()`. Programs the RX ring base address, descriptor count, buffer region base, and per-buffer size into the hardware. |
| `vnet_hw_clear_rx_ring(pdev)` | RX ring deregistration. Called in `stop()`. Clears RX ring registers so the hardware stops writing received frames into host memory. |

### Usage in open() and stop()

```c
/* Register per-queue TX rings in open() */
for (i = 0; i < priv->num_queues; i++)
    vnet_hw_set_tx_ring_queue(priv->pdev, i,
                               q->tx_ring.desc, q->tx_ring.count);

/* Register RX ring (queue 0, legacy API) */
vnet_hw_set_rx_ring(priv->pdev, q->rx_ring.descs, q->rx_ring.count,
                    q->rx_ring.bufs_va, VNET_MAX_PKT_LEN);
```

```c
/* Deregister per-queue TX rings in stop() */
for (i = 0; i < priv->num_queues; i++)
    vnet_hw_clear_tx_ring_queue(priv->pdev, i);

/* Deregister RX ring in stop() */
vnet_hw_clear_rx_ring(priv->pdev);
```

---

*Document Number: VNET-DS-MQ-008*
*Part 9 of the PCI Network Driver Curriculum*
