<!--
Copyright (c) 2024 TECH VEDA(www.techveda.org)
Author: Raghu Bharadwaj

This software is licensed under GPL v2.
See the accompanying LICENSE file for the full text.
-->

# Dynamic Stack Traces Example

This example demonstrates **dynamic hook points** using kprobes to capture stack traces from different kernel functions. Unlike static tracepoints, this approach shows how different execution paths create different stack traces.

## What it does

Hooks multiple kernel functions to capture diverse stack traces:
- `kmalloc` - Memory allocation (different allocation contexts)
- `vfs_read` - File system reads (different file types/paths)
- `tcp_sendmsg` - Network operations (different network stacks)
- `schedule` - Process scheduling (different scheduling contexts)

## Why it's useful

Each hook point captures different execution contexts, showing:
- **Varied stack traces** from different kernel subsystems
- **Real-world diversity** in kernel execution paths
- **Performance hotspots** in different system areas
- **Calling patterns** across kernel subsystems

## Build and run

```bash
make
sudo ./user/dynamic_traces
```

## Expected output

You'll see different stack IDs and traces from:
- Different processes triggering each hook
- Different kernel code paths to the same function
- Various system call contexts
- Multiple execution scenarios

This demonstrates the real value of stack trace profiling!