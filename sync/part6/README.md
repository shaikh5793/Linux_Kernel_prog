<!--
Copyright (c) 2024 TECH VEDA(www.techveda.org)
Author: Raghu Bharadwaj

This software is dual-licensed under the MIT License and GPL v2.
See the accompanying LICENSE file for the full text.
-->

# Part 6: Lockdep Annotations and Lock Debugging

This directory contains kernel modules demonstrating lockdep annotations and lock dependency validation techniques for debugging synchronization code.

## Modules Overview

### Lock Dependency Validation
- **lockdep_annotations.c** - Lockdep validation and annotation techniques

## Key Concepts

### Lockdep System
- Linux kernel's lock dependency validator
- Automatically detects potential deadlock scenarios
- Validates lock ordering at runtime during development

### Lock Classes
- Unique identifiers for different types of locks
- Help lockdep distinguish between lock instances
- Prevent false positive deadlock detection

### Manual Annotations
- `lock_acquire()` / `lock_release()` for custom tracking
- Inform lockdep about non-standard locking patterns
- Required for custom synchronization primitives

### Conditional Lock Validation
- `might_lock()` annotations for potential lock usage
- Validate lock dependencies in conditional code paths
- Ensure proper ordering even when locks aren't always acquired

## Lock Ordering Rules

### Consistent Ordering
- Always acquire locks in the same order across all code paths
- Release locks in reverse order of acquisition
- Prevents circular dependencies that lead to deadlocks

### Lock Hierarchy
- Define clear lock hierarchy in complex systems
- Higher-level locks acquired before lower-level locks
- Document lock ordering requirements

## Building

```bash
make build
```

## Cleaning

```bash
make clean
```

## Usage

Load the module:
```bash
sudo insmod lockdep_annotations.ko
```

Monitor lockdep output:
```bash
dmesg | tail -f
```

Unload the module:
```bash
sudo rmmod lockdep_annotations
```

## Debugging with Lockdep

### Enable Lockdep
Ensure kernel is built with lockdep support:
```
CONFIG_LOCKDEP=y
CONFIG_LOCKDEP_SUPPORT=y
CONFIG_DEBUG_LOCKDEP=y
```

### Common Lockdep Warnings
- **Circular dependency**: Lock ordering violation detected
- **Inconsistent lock state**: Lock acquired in different contexts
- **Recursive locking**: Same lock acquired twice by same task

### Best Practices
- Use consistent lock ordering throughout codebase
- Annotate custom locking with lockdep functions
- Test thoroughly with lockdep enabled during development
- Document lock hierarchies and ordering requirements
