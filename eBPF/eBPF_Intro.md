<!--
Copyright (c) 2024 TECH VEDA(www.techveda.org)
Author: Raghu Bharadwaj

This software is licensed under GPL v2.
See the accompanying LICENSE file for the full text.
-->

# What is eBPF?
- eBPF (extended Berkeley Packet Filter) is a revolutionary technology in the Linux kernel.
- It allows small, sandboxed programs to run safely in the kernel without writing modules.
- Originally for packet filtering, now extended to tracing, observability, and security.

# Why eBPF Matters
- Extends the Linux kernel dynamically, safely, and efficiently.
- Runs programs triggered by events like system calls, packet reception, or kernel tracepoints.
- Enables deep visibility with low overhead and real-time action.

# Core Architecture
- **Attach Points / Hooks**: Syscalls, tracepoints, kprobes, network stack, cgroups, sockets.
- **Verifier**: Ensures program safety before loading into kernel (no invalid access, bounded loops).
- **JIT Compiler**: Translates eBPF bytecode into native machine instructions for speed.
- **Maps**: Shared key-value stores for communication between kernel and user space.

# Data Flow in eBPF
1. Event occurs (e.g., packet received, syscall invoked).
2. Kernel invokes the attached eBPF program.
3. Program executes in a sandboxed environment (verified + JIT compiled).
4. Data stored/queried via maps.
5. Results flow back to kernel or user space.

# Use Cases
- **Networking**: Firewalls, load balancing, DDoS mitigation (e.g., Cilium, Katran).
- **Observability**: System tracing, performance profiling (bcc, bpftrace, perf).
- **Security**: Runtime monitoring, intrusion detection, policy enforcement (Falco, Tracee).

# Benefits
- High performance: JIT execution close to native speed.
- Safety: Verified before execution, prevents kernel crashes.
- Flexibility: Attach anywhere in the kernel stack.
- Portability: Works across different kernel versions.

# Challenges
- Learning curve for developers (new programming model).
- Limited debugging compared to traditional tools.
- Program size and complexity limits enforced by verifier.

# Conclusion
- eBPF is transforming Linux by safely extending kernel capabilities.
- It powers next-generation **networking, observability, and security** tools.
- Mastery of eBPF is essential for modern Linux and cloud-native engineers.

---
© 2025 TECH VEDA.
