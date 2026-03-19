<!--
Copyright (c) 2024 TECH VEDA(www.techveda.org)
Author: Raghu Bharadwaj

This software is dual-licensed under the MIT License and GPL v2.
See the accompanying LICENSE file for the full text.
-->

# Part 3: Lockless Synchronization and RCU

This directory contains kernel modules demonstrating lockless synchronization mechanisms, Read-Copy-Update (RCU), sequential locks, and lockfree data structures.

## Modules Overview

### Sequential Locking
- **kthread_seq.c** - Sequential lock (seqlock) implementation for read-heavy workloads

### Basic RCU (Read-Copy-Update)
- **kthread_rctest.c** - Basic RCU testing and verification
- **kthread_rcasync.c** - Asynchronous RCU operations
- **list_rcu.c** - RCU-protected linked list implementation

### Advanced RCU Variants
- **rcu_variants.c** - Comparison of different RCU variants (Classic, Tree, Preemptible)
- **srcu_example.c** - Sleepable RCU (SRCU) demonstration

### Lockfree Data Structures
- **lockfree_list.c** - Lock-free list operations using RCU
- **lockless_queue.c** - Lockless queue implementation using kfifo

### Documentation
- **list_rcu.md** - Detailed documentation for RCU list operations

## Key Concepts

### Sequential Locks (Seqlocks)
- Optimistic locking mechanism for read-heavy scenarios
- Readers check sequence numbers to detect writer interference
- Very low overhead for readers, writers get priority
- Ideal for frequently read, infrequently updated data

### RCU (Read-Copy-Update)
- Scalable synchronization mechanism for read-mostly data
- Readers access data without any locking overhead
- Writers create new versions and wait for grace periods
- Excellent for data structures like linked lists and hash tables
- Three main variants: Classic, Tree, and Preemptible RCU

### Sleepable RCU (SRCU)
- Allows readers to block/sleep while in RCU read-side critical sections
- Useful when readers need to perform operations that might sleep
- More overhead than regular RCU but adds flexibility

### Lockfree Data Structures
- Data structures that operate without traditional locking
- Use atomic operations and memory ordering constraints
- Provide better scalability and avoid priority inversion
- Require careful design to avoid ABA problems and memory ordering issues

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

## Performance Characteristics

### Sequential Locks
- **Read Path**: Extremely fast, just sequence number checks
- **Write Path**: Slightly more overhead than regular locks
- **Best For**: Data read frequently, updated rarely

### RCU Mechanisms
- **Read Path**: Zero overhead - no atomic operations or memory barriers
- **Write Path**: More complex - copy, update, synchronize
- **Best For**: Read-heavy workloads with occasional updates

### SRCU
- **Read Path**: Low overhead but higher than regular RCU
- **Write Path**: Similar to RCU but must handle sleeping readers
- **Best For**: Readers that need to perform blocking operations

### Lockfree Structures
- **Performance**: Excellent scalability, no lock contention
- **Complexity**: High implementation complexity
- **Best For**: High-concurrency scenarios with experienced developers

## Important Notes

### RCU Grace Periods
- Writers must wait for grace periods before freeing old data
- Grace period ensures all readers have finished accessing old data
- Different RCU variants have different grace period mechanisms

### Memory Ordering
- Lockfree algorithms require careful attention to memory ordering
- Use appropriate barriers and atomic operations
- Consider different CPU architectures and their memory models

### Debugging
- RCU and lockfree code can be difficult to debug
- Use RCU debugging options in kernel config when developing
- Lockdep can help detect some synchronization issues

## Safety Considerations

- RCU requires that read-side critical sections are non-blocking
- SRCU allows blocking in read sections but has higher overhead  
- Lockfree algorithms must handle all possible interleavings
- Memory reclamation must be synchronized with readers
