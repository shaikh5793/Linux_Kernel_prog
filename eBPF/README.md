<!--
Copyright (c) 2024 TECH VEDA(www.techveda.org)
Author: Raghu Bharadwaj

This software is licensed under GPL v2.
See the accompanying LICENSE file for the full text.
-->

# eBPF Learning Examples

Welcome to the comprehensive eBPF learning series! This repository contains 8 carefully designed examples that will take you from eBPF beginner to advanced practitioner.

## 🎯 Learning Objectives

By completing these examples in order, you will master:
- eBPF program types and attachment points
- Kernel-userspace communication patterns
- eBPF maps and data structures
- CO-RE (Compile Once - Run Everywhere) technology
- Performance monitoring and debugging
- Advanced tracing techniques

## 📚 Learning Path Overview

```
1. hello-bpf           → Basic tracepoint + skeleton
2. trace_syscalls      → Multiple programs + syscall tracing
3. kprobe_exec         → Kprobe attachment points
4. map_counter_exec    → eBPF maps for data storage
5. ringbuf_exec        → Ring buffer communication
6. pin_map_exec        → Map pinning and persistence
7. core_task_read      → CO-RE technology + kernel structures
8. dynamic_traces      → Multiple kprobes + stack traces
```

## 🛠️ Prerequisites

### System Requirements
- Linux kernel 5.4+ (with BTF support)
- Ubuntu 20.04+ or similar distribution
- Root/sudo privileges

### Development Tools
```bash
# Install required packages
sudo apt update
sudo apt install -y build-essential clang llvm libbpf-dev linux-headers-$(uname -r)

# Verify bpftool is available
which bpftool
```

### Kernel Support Verification
```bash
# Check BTF support
ls /sys/kernel/btf/vmlinux

# Check tracepoint availability
ls /sys/kernel/tracing/events/syscalls/
```

## 📖 Detailed Learning Sequence

## Level 1: Foundation (Beginner)

### 1. 🌟 **hello-bpf** - Your First eBPF Program
**Duration:** 30 minutes
**Complexity:** ⭐☆☆☆☆

**What you'll learn:**
- Basic eBPF program structure
- Tracepoint attachment (`sys_enter_execve`)
- Skeleton-based development workflow
- Using `bpf_printk()` for debugging

**Key concepts:**
- SEC() macro for program sections
- libbpf skeleton generation
- Basic eBPF helper functions

**Prerequisites:** None - start here!

---

### 2. 🔍 **trace_syscalls** - Multiple Program Management
**Duration:** 45 minutes
**Complexity:** ⭐⭐☆☆☆

**What you'll learn:**
- Multiple eBPF programs in one file
- Different syscall tracepoints (openat, read, write)
- Syscall argument extraction
- Process information gathering

**Key concepts:**
- Multiple SEC() declarations
- Tracepoint context structure
- System call argument arrays
- Modern syscall naming (openat vs open)

**Prerequisites:** Complete hello-bpf

---

## Level 2: Data Structures (Intermediate)

### 3. 📊 **map_counter_exec** - Introduction to eBPF Maps
**Duration:** 45 minutes
**Complexity:** ⭐⭐☆☆☆

**What you'll learn:**
- eBPF hash maps for data storage
- Map operations (lookup, update)
- Key-value data persistence
- Userspace map access

**Key concepts:**
- `BPF_MAP_TYPE_HASH`
- `bpf_map_lookup_elem()` and `bpf_map_update_elem()`
- Map section definition
- Cross-program data sharing

**Prerequisites:** Complete trace_syscalls

---

### 4. 🔄 **ringbuf_exec** - Efficient Kernel-User Communication
**Duration:** 60 minutes
**Complexity:** ⭐⭐⭐☆☆

**What you'll learn:**
- Ring buffer for high-performance data streaming
- Event structure design
- Producer-consumer patterns
- Real-time data processing

**Key concepts:**
- `BPF_MAP_TYPE_RINGBUF`
- `bpf_ringbuf_reserve()` and `bpf_ringbuf_submit()`
- Shared data structures
- Event-driven programming

**Prerequisites:** Complete map_counter_exec

---

## Level 3: Advanced Attachment (Intermediate-Advanced)

### 5. ⚡ **kprobe_exec** - Kernel Function Interception
**Duration:** 45 minutes
**Complexity:** ⭐⭐⭐☆☆

**What you'll learn:**
- Kprobe vs tracepoint differences
- Dynamic function instrumentation
- Kernel symbol resolution
- Direct syscall interception

**Key concepts:**
- `SEC("kprobe/function_name")`
- Kernel function entry points
- Symbol naming conventions (`__x64_sys_*`)
- Performance considerations

**Prerequisites:** Complete ringbuf_exec

---

### 6. 🧬 **core_task_read** - CO-RE Technology Mastery
**Duration:** 75 minutes
**Complexity:** ⭐⭐⭐⭐☆

**What you'll learn:**
- CO-RE (Compile Once - Run Everywhere)
- Kernel structure field access
- Process genealogy and relationships
- Portable eBPF programs

**Key concepts:**
- `bpf_core_read()` functions
- BTF-powered field access
- Kernel version independence
- Task structure navigation

**Prerequisites:** Complete kprobe_exec

---

## Level 4: Advanced Concepts (Advanced)

### 7. 📌 **pin_map_exec** - Map Persistence and Sharing
**Duration:** 60 minutes
**Complexity:** ⭐⭐⭐⭐☆

**What you'll learn:**
- Map pinning to filesystem
- Cross-program data sharing
- Map lifecycle management
- Persistent data storage

**Key concepts:**
- Map pinning APIs
- Filesystem-based persistence
- Multiple program coordination
- State management

**Prerequisites:** Complete core_task_read

---

### 8. 🌊 **dynamic_traces** - Advanced Multi-Point Tracing
**Duration:** 90 minutes
**Complexity:** ⭐⭐⭐⭐⭐

**What you'll learn:**
- Multiple kprobe attachments
- Stack trace collection
- Complex event correlation
- Performance analysis techniques

**Key concepts:**
- Stack map usage
- Multiple attachment coordination
- Advanced data aggregation
- Real-world monitoring patterns

**Prerequisites:** Complete pin_map_exec

---

## 🚀 Quick Start Guide

### Build All Examples
```bash
# Clone and build everything
cd eBPF/
for example in hello-bpf trace_syscalls map_counter_exec ringbuf_exec kprobe_exec core_task_read pin_map_exec dynamic_traces; do
    echo "Building $example..."
    cd $example && make && cd ..
done
```

### Run Your First Example
```bash
cd hello-bpf/
make
sudo ./user/hello

# In another terminal:
sudo cat /sys/kernel/tracing/trace_pipe
```

## 📋 Each Example Includes

- **README.md** - Detailed explanation and learning objectives
- **Makefile** - Build configuration with proper clean targets
- **bpf/*.bpf.c** - Commented eBPF kernel programs
- **user/*.c** - Comprehensive userspace applications
- **Examples and experiments** - Hands-on learning activities

## 🔧 Build System Features

All examples use a consistent build system:
```bash
make           # Build everything
make clean     # Remove all generated files
make help      # Show available targets (where implemented)
```

**Generated files:**
- `bpf/vmlinux.h` - Kernel type definitions
- `bpf/*.bpf.o` - Compiled eBPF bytecode
- `bpf/*.skel.h` - Auto-generated skeleton headers
- `user/*` - Userspace executables

## 🎓 Learning Tips

### 1. **Follow the Order**
Each example builds on previous concepts. Skipping ahead may cause confusion.

### 2. **Read the Code Comments**
All code is extensively commented to explain eBPF concepts and best practices.

### 3. **Experiment**
Each example includes suggested modifications and experiments.

### 4. **Use the Debugger**
- `bpf_printk()` for kernel-side debugging
- `sudo cat /sys/kernel/tracing/trace_pipe` for output
- `bpftool prog list` to see loaded programs

### 5. **Monitor Performance**
```bash
# Check eBPF program statistics
bpftool prog show
bpftool map show

# Monitor system impact
top -p $(pgrep your_program)
```

## 🛠️ Troubleshooting

### Common Issues

**Build Errors:**
```bash
# Missing headers
sudo apt install linux-headers-$(uname -r) libbpf-dev

# Old kernel
uname -r  # Should be 5.4+
```

**Runtime Errors:**
```bash
# Permissions
sudo ./your_program

# BTF not available
ls /sys/kernel/btf/vmlinux

# Verify eBPF support
zgrep CONFIG_BPF /proc/config.gz
```

**No Output:**
```bash
# Check program loading
sudo bpftool prog list

# Verify tracepoints
ls /sys/kernel/tracing/events/syscalls/

# Generate test activity
ls /tmp  # For file-related traces
```

## 📚 Additional Resources

### Documentation
- [eBPF Official Documentation](https://ebpf.io/)
- [libbpf Documentation](https://libbpf.readthedocs.io/)
- [BPF Type Format (BTF)](https://www.kernel.org/doc/html/latest/bpf/btf.html)

### Books
- "Learning eBPF" by Liz Rice
- "BPF Performance Tools" by Brendan Gregg

### Online Resources
- [eBPF Tutorial](https://github.com/xdp-project/xdp-tutorial)
- [libbpf-bootstrap](https://github.com/libbpf/libbpf-bootstrap)

## 🤝 Contributing

Found an issue or want to improve an example?
- Check code for clarity and correctness
- Verify all examples build and run
- Test on multiple kernel versions
- Update documentation as needed

## ⚠️ Important Notes

1. **Requires Root:** All examples need sudo privileges to load eBPF programs
2. **Kernel Compatibility:** Tested on kernels 5.4+
3. **Performance Impact:** Some examples trace high-frequency events
4. **Security:** eBPF programs have extensive kernel access - use responsibly

---

**Happy Learning!** 🎉

Start with `hello-bpf/` and work your way through each example. By the end, you'll have a solid foundation in eBPF development and be ready to build your own advanced monitoring and tracing tools.

## 📈 Next Steps After Completion

After mastering these examples, consider exploring:
- **XDP (eXpress Data Path)** for network packet processing
- **LSM (Linux Security Modules)** integration
- **eBPF for Kubernetes** monitoring
- **Custom eBPF libraries** and frameworks
- **Production deployment** patterns and best practices
