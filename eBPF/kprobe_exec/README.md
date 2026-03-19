<!--
Copyright (c) 2024 TECH VEDA(www.techveda.org)
Author: Raghu Bharadwaj

This software is licensed under GPL v2.
See the accompanying LICENSE file for the full text.
-->

# Example — kprobe_exec

Attach a kprobe to the syscall entry `__x64_sys_execve` and log via `bpf_printk`.

Build
- `make`

Run
- Terminal A: `sudo ./user/kprobe_exec`
- Terminal B: `sudo cat /sys/kernel/tracing/trace_pipe`

Note
- This example targets x86_64 (Ubuntu 24.04). Symbol names differ across arches.
