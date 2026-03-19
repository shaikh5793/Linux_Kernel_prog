<!--
Copyright (c) 2024 TECH VEDA(www.techveda.org)
Author: Raghu Bharadwaj

This software is licensed under GPL v2.
See the accompanying LICENSE file for the full text.
-->

# eBPF Debugging Examples for ARM VExpress

Collection of eBPF programs for debugging kernel behavior on ARM VExpress QEMU.

## 📦 What's Included

### 1. Memory Leak Tracker (`memleak_tracker.c`)
Tracks `kmalloc()` and `kfree()` calls to detect memory leaks.

**Use cases:**
- Find memory leaks in kernel code
- Monitor memory allocation patterns
- Debug out-of-memory issues

**Maps:**
- `allocs` - Active allocations (address → size)
- `stats` - Allocation/free counters

### 2. Syscall Tracer (`syscall_tracer.c`)
Traces system calls with timing information.

**Use cases:**
- Identify slow syscalls
- Monitor syscall frequency
- Debug userspace-kernel interaction

**Maps:**
- `syscall_start` - Entry timestamps
- `syscall_latency` - Total latency per syscall
- `syscall_count` - Call counts per syscall

### 3. Function Latency Profiler (`func_latency.c`)
Measures kernel function execution time using kprobes.

**Use cases:**
- Find performance bottlenecks
- Profile specific kernel functions
- Measure code paths

**Maps:**
- `func_start` - Entry timestamps
- `latency_hist` - Latency histogram (64 buckets)
- `stats` - Min/max/avg/count stats

### 4. Lock Contention Analyzer (`lock_contention.c`)
Monitors mutex lock/unlock with timing.

**Use cases:**
- Find lock contention hotspots
- Debug deadlocks and lockdep warnings
- Optimize locking strategy

**Maps:**
- `lock_start` - Lock acquisition time
- `hold_time_hist` - Lock hold time histogram
- `lock_stats` - Lock/unlock counters

### 5. Page Fault Tracker (`pagefault_tracker.c`)
Monitors page faults (user and kernel).

**Use cases:**
- Analyze memory access patterns
- Debug MMU/TLB issues
- Optimize page usage

**Maps:**
- `pf_stats` - Page fault counters by type
- `pf_addrs` - Fault address histogram

---

## 🔧 Prerequisites

### On Host (x86 Ubuntu)
- clang (for compiling eBPF)
- llvm (for objdump)
- Kernel headers from buildroot

### On Target (ARM VExpress QEMU)
- Kernel with eBPF support (CONFIG_BPF_SYSCALL=y)
- bpftool (from buildroot)
- debugfs mounted

---

## 🚀 Quick Start

### 1. Build eBPF Programs (On Host)

```bash
cd /home/raghub/arm-modules/ebpf-examples
make
```

Output: `obj/*.o` files ready for ARM

### 2. Deploy to QEMU

```bash
# Copy to QEMU
scp -P 2222 obj/*.o root@localhost:/tmp/

# Or use deploy script
./deploy-to-qemu.sh
```

### 3. Load and Run (On QEMU Target)

```bash
# Mount debugfs (if not mounted)
mount -t debugfs none /sys/kernel/debug

# Load a program with bpftool
bpftool prog load memleak_tracker.o /sys/fs/bpf/memleak

# Attach to kprobes (example)
bpftool prog attach pinned /sys/fs/bpf/memleak kprobe __kmalloc

# View loaded programs
bpftool prog list

# View maps
bpftool map list

# Dump map data
bpftool map dump name allocs
```

---

## 📊 Usage Examples

### Example 1: Track Memory Leaks

```bash
# On QEMU target
cd /tmp

# Load memory leak tracker
bpftool prog load memleak_tracker.o /sys/fs/bpf/memleak type tracing

# Run your buggy module
insmod /tmp/buggy_modules/use_after_free.ko
echo "leak" > /proc/uaf_trigger

# Check allocations
bpftool map dump name allocs

# Check stats
bpftool map dump name stats
```

### Example 2: Trace Syscalls

```bash
# Load syscall tracer
bpftool prog load syscall_tracer.o /sys/fs/bpf/syscall type tracing

# Let it run for a while
sleep 10

# View syscall counts
bpftool map dump name syscall_count

# View latencies
bpftool map dump name syscall_latency
```

### Example 3: Profile Function Latency

```bash
# Load function profiler
bpftool prog load func_latency.o /sys/fs/bpf/functime type tracing

# Attach to specific function (e.g., kmalloc)
echo 'p:mykprobe __kmalloc' > /sys/kernel/debug/tracing/kprobe_events
bpftool prog attach pinned /sys/fs/bpf/functime kprobe mykprobe

# View histogram
bpftool map dump name latency_hist

# View stats (min/max/avg)
bpftool map dump name stats
```

### Example 4: Analyze Lock Contention

```bash
# Load lock analyzer
bpftool prog load lock_contention.o /sys/fs/bpf/locks type tracing

# Run code that uses locks
insmod /tmp/buggy_modules/missing_lock.ko
echo "race" > /proc/missing_lock_trigger

# View hold time distribution
bpftool map dump name hold_time_hist

# View lock stats
bpftool map dump name lock_stats
```

---

## 🛠️ Build System

### Makefile Targets

```bash
make              # Build all eBPF programs
make clean        # Clean build artifacts
make install      # Copy .o files to bin/
make verify       # Verify built objects
make list         # List available programs
make help         # Show help
```

### File Structure

```
ebpf-examples/
├── src/                    # eBPF C source files
│   ├── memleak_tracker.c
│   ├── syscall_tracer.c
│   ├── func_latency.c
│   ├── lock_contention.c
│   └── pagefault_tracker.c
├── obj/                    # Compiled .o files (created by make)
├── bin/                    # Installed binaries
├── scripts/                # Helper scripts
│   └── load_ebpf.py        # Python loader
├── Makefile                # Build system
└── README.md               # This file
```

---

## 🐛 Debugging Tips

### Check if eBPF is Enabled

```bash
# On QEMU
zcat /proc/config.gz | grep -E "(BPF_SYSCALL|BPF_JIT|KPROBES)"

# Should see:
# CONFIG_BPF_SYSCALL=y
# CONFIG_BPF_JIT=y
# CONFIG_KPROBES=y
```

### Mount debugfs

```bash
mount -t debugfs none /sys/kernel/debug
```

### Verify bpftool

```bash
which bpftool
bpftool version
```

### Check Program Load Errors

```bash
# Load with verbose output
bpftool -d prog load memleak_tracker.o /sys/fs/bpf/memleak

# Check kernel log
dmesg | tail -20
```

### List Available Tracepoints

```bash
ls /sys/kernel/debug/tracing/events/
```

### List Available Kprobes

```bash
cat /sys/kernel/debug/tracing/available_filter_functions | grep kmalloc
```

---

## 📝 Notes

### Without BTF

These programs are compiled WITHOUT BTF (BPF Type Format) to avoid build issues. This means:
- ✅ Works without CO-RE
- ✅ Faster compilation
- ✅ Smaller binary size
- ❌ No automatic struct layout adaptation
- ❌ Less portable across kernel versions

For our single-target ARM kernel, this is fine!

### ARM-Specific

Programs are compiled with `-target bpf` and `-D__TARGET_ARCH_arm` for ARM compatibility.

### Memory Limits

Maps have size limits:
- Hash maps: 10,240 entries
- Array maps: 32-64 entries
- Adjust `MAX_ENTRIES` if needed

---

## 🔗 Integration with Buggy Modules

These eBPF programs work great with the buggy kernel modules:

```bash
# Test memory leak detection
insmod use_after_free.ko
# Load memleak eBPF program
# Trigger: echo "leak" > /proc/uaf_trigger
# Check: bpftool map dump name allocs

# Test lock contention
insmod deadlock.ko
# Load lock_contention eBPF program
# Trigger: echo "trigger" > /proc/deadlock_trigger
# Check: bpftool map dump name lock_stats
```

---

## 📚 Further Reading

- **BPF Documentation**: `/home/raghub/raghu/buildroot/output/build/linux-6.12.47/Documentation/bpf/`
- **bpftool Manual**: `man bpftool` (on system with it installed)
- **Kernel Tracing**: `/sys/kernel/debug/tracing/README`

---

## 🎯 Next Steps

1. **Boot QEMU with eBPF-enabled kernel**
2. **Copy eBPF objects to QEMU** (`scp` or shared folder)
3. **Load programs with bpftool**
4. **Trigger events** (run buggy modules, syscalls, etc.)
5. **Dump maps** to see collected data

Happy eBPF debugging! 🐝
