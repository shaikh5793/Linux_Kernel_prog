<!--
Copyright (c) 2024 TECH VEDA(www.techveda.org)
Author: Raghu Bharadwaj

This software is dual-licensed under the MIT License and GPL v2.
See the accompanying LICENSE file for the full text.
-->

# Part 2: Reader-Writer Synchronization

This directory contains kernel modules demonstrating reader-writer synchronization mechanisms that allow multiple concurrent readers OR one exclusive writer.

## Modules Overview

### Reader-Writer Synchronization
- **kthread_rwspin.c** - Reader-writer spinlock implementation
- **kthread_rwsem.c** - Reader-writer semaphore usage patterns

## Key Concepts

### Reader-Writer Locks
- Allow multiple concurrent readers OR one exclusive writer
- Useful for data structures with read-heavy access patterns
- Two implementations: spinlock-based and semaphore-based
- Choice depends on whether readers can sleep during critical sections

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

## Notes

### Reader-Writer Spinlocks
- Fast but readers cannot sleep while holding the lock
- Suitable for short critical sections
- Lower overhead than semaphores

### Reader-Writer Semaphores
- Readers can sleep while holding the lock
- Suitable for longer critical sections
- Can be used in contexts where sleeping is required

### Performance Considerations
- Reader-writer locks work best when reads significantly outnumber writes
- In write-heavy scenarios, regular mutexes may perform better
- Consider cache line bouncing effects in SMP systems
