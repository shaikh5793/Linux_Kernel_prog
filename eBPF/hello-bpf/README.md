<!--
Copyright (c) 2024 TECH VEDA(www.techveda.org)
Author: Raghu Bharadwaj

This software is licensed under GPL v2.
See the accompanying LICENSE file for the full text.
-->

Goals
- Verify toolchain and kernel support.
- Build a minimal CO-RE BPF program and userspace loader (libbpf skeleton).
- See output via `bpf_printk` in `trace_pipe`.

Build
- `make` (from this directory) builds both BPF and userspace.

What to expect
- Each new process exec (`execve`) prints a line like: `hello_bpf: execve by pid=1234`.

Files
- `bpf/hello.bpf.c` — BPF program attached to `tracepoint/syscalls/sys_enter_execve`.
- `user/hello.c` — Userspace loader generated from libbpf skeleton.

Cleanup
- `make clean`
