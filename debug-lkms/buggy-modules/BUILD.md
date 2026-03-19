<!--
Copyright (c) 2024 TECH VEDA(www.techveda.org)
Author: Raghu Bharadwaj

This software is dual-licensed under the MIT License and GPL v2.
See the accompanying LICENSE file for the full text.
-->

# Building Buggy Kernel Modules

## Quick Build

All Makefiles are pre-configured with the correct Buildroot toolchain path:

```bash
cd /home/raghub/arm-modules/buggy-modules

# Build all modules
make

# Clean build
make clean

# Verify modules
make verify

# List available modules
make list
```

## What Gets Built

**Memory Bug Modules (7):**
- `use_after_free.ko` - Use-after-free for KASAN/KFENCE
- `buffer_overflow.ko` - Buffer overflow for KASAN
- `double_free.ko` - Double free for SLUB debug
- `slab_corruption.ko` - Slab corruption
- `invalid_free.ko` - Invalid kfree()
- `oob_read.ko` - Out-of-bounds read
- `krealloc_misuse.ko` - Krealloc bugs

**Locking Bug Modules (8):**
- `deadlock.ko` - ABBA deadlock
- `double_unlock.ko` - Double unlock
- `missing_lock.ko` - Race conditions
- `sleep_in_atomic.ko` - Sleep in atomic context
- `spin_recursion.ko` - Spinlock recursion
- `rwlock_misuse.ko` - Read-write lock misuse
- `rwsem_misuse.ko` - RW semaphore misuse
- `rcu_misuse.ko` - RCU misuse

**Address Translation Modules (2):**
- `null_deref.ko` - Null pointer dereference ⚠️
- `invalid_access.ko` - Invalid memory access ⚠️

**Total: 17 modules**

## Build Environment

**Toolchain:**
```
CROSS_COMPILE=/home/raghub/raghu/buildroot/output/host/bin/arm-buildroot-linux-gnueabihf-
ARCH=arm
KERNEL_DIR=/home/raghub/raghu/buildroot/output/build/linux-6.12.47
```

**Kernel Config:**
- ✅ KASAN (Kernel Address Sanitizer)
- ✅ KFENCE (Kernel Electric Fence)
- ✅ SLUB_DEBUG_ON
- ✅ LOCKDEP
- ✅ eBPF + Kprobes

## Individual Category Builds

```bash
# Build memory modules only
cd memory && make

# Build locking modules only
cd locking && make

# Build address modules only
cd address && make
```

## Deploying to QEMU

### Method 1: SCP (Recommended)
```bash
# Deploy all modules
./deploy-to-qemu.sh

# Or manually
scp -P 2222 memory/*.ko locking/*.ko address/*.ko \
    root@localhost:/tmp/buggy_modules/
```

### Method 2: Pre-embedded
Modules are already in QEMU rootfs at `/root/modules/`

### Method 3: Mount and Copy
```bash
sudo mount -o loop /home/raghub/raghu/buildroot/output/images/rootfs.ext2 /mnt
sudo cp memory/*.ko locking/*.ko address/*.ko /mnt/root/modules/
sudo umount /mnt
```

## Troubleshooting

**Build fails with "No such file or directory":**
```bash
# Check kernel source exists
ls -la /home/raghub/raghu/buildroot/output/build/linux-6.12.47

# Rebuild kernel first
cd /home/raghub/raghu/buildroot
make linux-rebuild
```

**Module version mismatch:**
```bash
# Clean and rebuild modules
make clean
make
```

**Cross-compiler not found:**
```bash
# Verify toolchain exists
ls -la /home/raghub/raghu/buildroot/output/host/bin/arm-buildroot-linux-gnueabihf-gcc

# Rebuild buildroot toolchain
cd /home/raghub/raghu/buildroot
make toolchain
```

## Testing in QEMU

```bash
# In QEMU console
cd /root/modules  # or /tmp/buggy_modules

# Test use-after-free
insmod use_after_free.ko
echo "trigger" > /proc/uaf_trigger
dmesg | tail -30
rmmod use_after_free

# Test deadlock
insmod deadlock.ko
echo "trigger" > /proc/deadlock_trigger
dmesg | grep -i lockdep
rmmod deadlock
```

## Module Compatibility

**Kernel Version:** 6.12.47  
**Architecture:** ARM (Cortex-A9, VExpress)  
**Vermagic:** Should match QEMU kernel

Check compatibility:
```bash
modinfo use_after_free.ko | grep vermagic
# Should match: uname -r in QEMU
```

## Clean Everything

```bash
# Clean all modules
make clean

# Remove all .ko files
find . -name "*.ko" -delete

# Start fresh build
make clean && make
```
