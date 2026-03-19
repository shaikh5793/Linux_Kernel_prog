<!--
Copyright (c) 2024 TECH VEDA(www.techveda.org)
Author: Raghu Bharadwaj

This software is licensed under GPL v2.
See the accompanying LICENSE file for the full text.
-->

# eBPF Examples - Quick Start Guide

## 📦 What You Have

**5 eBPF debugging programs ready to use:**
1. Memory leak tracker
2. Syscall tracer  
3. Function latency profiler
4. Lock contention analyzer
5. Page fault tracker

---

## 🚀 3-Step Workflow

### Step 1: Build (On Host x86)

```bash
cd /home/raghub/arm-modules/ebpf-examples
make
```

Output: `obj/*.o` files ready for ARM

### Step 2: Deploy to QEMU

**Wait for kernel rebuild to finish first!**

Then:
```bash
./deploy-to-qemu.sh
```

Or manually:
```bash
scp -P 2222 obj/*.o root@localhost:/tmp/ebpf-examples/
```

### Step 3: Use on QEMU Target

```bash
# SSH to QEMU
ssh -p 2222 root@localhost
cd /tmp/ebpf-examples

# Mount debugfs (needed once)
mount -t debugfs none /sys/kernel/debug

# Load a program
bpftool prog load syscall_tracer.o /sys/fs/bpf/syscall type tracing

# View loaded programs
bpftool prog list

# View maps
bpftool map list

# Dump map data
bpftool map dump name syscall_count
```

---

## 🎯 Quick Examples

### Track Memory Leaks

```bash
bpftool prog load memleak_tracker.o /sys/fs/bpf/memleak type tracing
# Run your code...
bpftool map dump name allocs
```

### Trace Syscalls

```bash
bpftool prog load syscall_tracer.o /sys/fs/bpf/syscall type tracing
sleep 10
bpftool map dump name syscall_count
```

### Analyze Locks

```bash
bpftool prog load lock_contention.o /sys/fs/bpf/locks type tracing
# Run code with locks...
bpftool map dump name lock_stats
```

---

## 📁 What's Where

```
/home/raghub/arm-modules/ebpf-examples/
├── src/           - eBPF source code (.c files)
├── obj/           - Built programs (.o files) [created by make]
├── scripts/       - Helper scripts
├── Makefile       - Build system
├── README.md      - Full documentation
└── QUICKSTART.md  - This file
```

---

## 🔗 Integration with Buggy Modules

Perfect combo! Use eBPF to monitor buggy modules:

```bash
# Example: Monitor memory while running buggy module
bpftool prog load memleak_tracker.o /sys/fs/bpf/memleak type tracing
insmod /tmp/buggy_modules/use_after_free.ko
echo "leak" > /proc/uaf_trigger
bpftool map dump name stats
```

---

## ⚠️ Prerequisites

**On QEMU (check once):**

```bash
# 1. Check eBPF is enabled
zcat /proc/config.gz | grep BPF_SYSCALL
# Should show: CONFIG_BPF_SYSCALL=y

# 2. Check bpftool exists
which bpftool

# 3. Mount debugfs
mount -t debugfs none /sys/kernel/debug
```

---

## 🐛 Troubleshooting

**"bpftool: command not found"**
- Kernel rebuild still running OR bpftool not in rootfs
- Wait for rebuild to complete

**"Failed to load program"**
- Check: `dmesg | tail`
- Verify: `mount | grep debugfs`

**"Unknown map name"**
- Program not loaded or wrong name
- List: `bpftool map list`

---

## 📚 More Info

- **Full docs**: `README.md`
- **Build options**: `make help`
- **Examples**: See README.md "Usage Examples" section

---

## ✅ Summary

1. ✅ Build: `make`
2. ✅ Deploy: `./deploy-to-qemu.sh`
3. ✅ Use: `bpftool prog load ...`
4. ✅ Check: `bpftool map dump ...`

**That's it!** 🎉

Your eBPF debugging arsenal is ready once the kernel rebuild finishes!
