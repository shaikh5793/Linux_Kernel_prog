<!--
Copyright (c) 2024 TECH VEDA(www.techveda.org)
Author: Raghu Bharadwaj

This software is dual-licensed under the MIT License and GPL v2.
See the accompanying LICENSE file for the full text.
-->

# Part 1: Basic Kernel Threads and Synchronization Primitives

This directory contains kernel modules demonstrating basic kernel thread creation and fundamental synchronization mechanisms in the Linux kernel.

## Modules Overview

### Thread Creation and Management
- **kthread.c** - Basic kernel thread creation and lifecycle management
- **kthread_at.c** - Kernel threads with atomic operations

### Mutual Exclusion Primitives
- **kthread_mutex.c** - Mutex-based synchronization between threads
- **kthread_spin.c** - Spinlock implementation for thread coordination
- **kthread_rawspin.c** - Raw spinlock usage patterns
- **kthread_rtmut.c** - RT (Real-time) mutex implementation

### Semaphore-based Synchronization  
- **kthread_semlck.c** - Semaphore locking mechanisms
- **kthread_csem.c** - Counting semaphore implementation

### Bit-level Operations
- **kthread_bitlck.c** - Bit-level locking and atomic bit operations

### Advanced Mutex Features
- **kthread_wwtest.c** - Wound-wait mutex implementation for deadlock prevention

## Building

```bash
make build
```

## Cleaning

```bash
make clean
```

## Usage

Each module can be loaded individually using:
```bash
sudo insmod <module_name>.ko
```

And unloaded with:
```bash
sudo rmmod <module_name>
```

Monitor kernel messages with:
```bash
dmesg | tail -f
```
