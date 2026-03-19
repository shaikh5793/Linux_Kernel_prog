<!--
Copyright (c) 2024 TECH VEDA(www.techveda.org)
Author: Raghu Bharadwaj

This software is dual-licensed under the MIT License and GPL v2.
See the accompanying LICENSE file for the full text.
-->

# Part 3: Advanced Thread Synchronization and Per-CPU Operations

This directory contains kernel modules demonstrating advanced synchronization primitives, per-CPU operations, and thread coordination mechanisms.

## Modules Overview

### Completion Mechanisms
- **kthread_compl.c** - Completion primitive for thread coordination and synchronization

### Generic Thread Synchronization
- **kthread_sync.c** - Generic synchronization patterns and coordination
- **kthread_sync_waitq.c** - Wait queue-based synchronization mechanisms

### Per-CPU Operations
- **kthread_pcpu.c** - Per-CPU variable operations and CPU-local data structures

### CPU Reader-Writer Semaphores
- **kthread_cpurwsem.c** - CPU reader-writer semaphore implementation for NUMA-aware synchronization

## Key Concepts

### Completions
- One-shot synchronization primitive
- Used when one thread needs to wait for another to complete a task
- More efficient than using mutexes for simple signaling

### Wait Queues
- Kernel mechanism for putting processes to sleep
- Processes wait for specific conditions to become true
- Efficient event-driven synchronization

### Per-CPU Variables
- Variables that exist separately on each CPU
- Eliminates cache line bouncing and improves SMP scalability
- Critical for high-performance kernel code

### CPU Reader-Writer Semaphores
- NUMA-aware synchronization primitives
- Optimized for multi-processor systems
- Reduces cross-CPU cache coherency traffic

## Building

```bash
make build
```

## Cleaning

```bash
make clean
```

## Usage

Load modules individually:
```bash
sudo insmod <module_name>.ko
```

Unload modules:
```bash
sudo rmmod <module_name>
```

Monitor kernel messages:
```bash
dmesg | tail -f
```

## Performance Considerations

- Per-CPU operations are crucial for scalability on multi-core systems
- Wait queues provide efficient blocking synchronization
- Completions are preferred over semaphores for simple signaling scenarios
- CPU reader-writer semaphores optimize for NUMA topology