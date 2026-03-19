<!--
Copyright (c) 2024 TECH VEDA(www.techveda.org)
Author: Raghu Bharadwaj

This software is dual-licensed under the MIT License and GPL v2.
See the accompanying LICENSE file for the full text.
-->

# Buggy Kernel Modules for Debug Testing

This directory contains intentionally buggy kernel modules designed to test various kernel debug options on ARM VExpress QEMU.

## Overview

These modules are designed to trigger:
- **Memory bugs**: KASAN, KFENCE, SLUB debug, page poisoning
- **Locking bugs**: Lockdep, race conditions, deadlocks
- **Address translation bugs**: Page faults, MMU errors, null pointer dereferences

## Directory Structure

```
buggy-modules/
├── memory/              # Memory-related bugs
│   ├── use_after_free.c
│   ├── buffer_overflow.c
│   ├── double_free.c
│   └── slab_corruption.c
├── locking/             # Lock-related bugs
│   ├── deadlock.c
│   ├── double_unlock.c
│   └── missing_lock.c
├── address/             # Address translation bugs
│   ├── null_deref.c
│   └── invalid_access.c
├── Makefile             # Build system
├── build-and-deploy.sh  # Build and deploy to QEMU
├── test-all-bugs.sh     # Interactive test script (host)
└── test-modules-qemu.sh # Test script for QEMU target
```

## Quick Start

### 1. Build Modules

```bash
cd /home/raghub/arm-modules/buggy-modules
make
```

This will build all modules for ARM architecture.

### 2. Verify Build

```bash
make verify
```

Check that all modules are correctly built for ARM.

### 3. Deploy to QEMU

**Option A: Automatic deployment**
```bash
./build-and-deploy.sh
```

**Option B: Manual deployment**
```bash
# On host
scp -P 2222 memory/*.ko root@localhost:/tmp/
scp -P 2222 locking/*.ko root@localhost:/tmp/
scp -P 2222 address/*.ko root@localhost:/tmp/

# On QEMU
ssh -p 2222 root@localhost
cd /tmp
```

### 4. Test Modules

**On QEMU target:**
```bash
./test-modules-qemu.sh
```

Or test manually:
```bash
# Load a module
insmod use_after_free.ko

# Trigger the bug
echo "trigger" > /proc/uaf_trigger

# Check results
dmesg | tail -20
```

## Module Reference

### Memory Modules

#### 1. use_after_free.ko
Tests KASAN and KFENCE detection of use-after-free bugs.

**Triggers:**
- `/proc/uaf_trigger` - write "trigger" to cause use-after-free
- `/proc/uaf_trigger` - write "leak" to cause memory leak

**Detection:** KASAN, KFENCE, KMEMLEAK

#### 2. buffer_overflow.ko
Tests KASAN and SLUB debug detection of buffer overflows.

**Triggers:**
- `/proc/overflow_trigger` - write "overflow" for buffer overflow
- `/proc/overflow_trigger` - write "underflow" for buffer underflow

**Detection:** KASAN, SLUB debug with redzones

#### 3. double_free.ko
Tests SLUB debug detection of double-free bugs.

**Triggers:**
- `/proc/double_free_trigger` - write "trigger" for simple double free
- `/proc/double_free_trigger` - write "complex" for aliased pointer double free

**Detection:** SLUB debug

#### 4. slab_corruption.ko
Tests SLUB debug detection of slab corruption.

**Triggers:**
- `/proc/slab_trigger` - write "corrupt" for metadata corruption
- `/proc/slab_trigger` - write "redzones" for redzone violation
- `/proc/slab_trigger` - write "poison" for poison pattern violation

**Detection:** SLUB debug (redzones, poison, sanity checks)

### Locking Modules

#### 5. deadlock.ko
Tests lockdep detection of deadlock scenarios.

**Triggers:**
- `/proc/deadlock_trigger` - write "trigger" for ABBA deadlock
- `/proc/deadlock_trigger` - write "self" for self-deadlock (⚠️ WARNING: will hang!)

**Detection:** Lockdep

#### 6. double_unlock.ko
Tests lockdep detection of double unlock.

**Triggers:**
- `/proc/double_unlock_trigger` - write "mutex" for double mutex unlock
- `/proc/double_unlock_trigger` - write "spinlock" for double spinlock unlock
- `/proc/double_unlock_trigger` - write "unbalanced" for unlock without lock

**Detection:** Lockdep

#### 7. missing_lock.ko
Tests race condition from missing lock protection.

**Triggers:**
- `/proc/missing_lock_trigger` - write "race" to run unprotected concurrent access
- `/proc/missing_lock_trigger` - write "correct" to run with proper locking
- `/proc/missing_lock_trigger` - write "reset" to reset counter

**Status:** Read `/proc/missing_lock_status` to check counter value

**Detection:** Race condition visible in incorrect counter value

### Address Translation Modules

⚠️ **WARNING:** These modules will cause kernel oops and may require system reboot!

#### 8. null_deref.ko
Tests null pointer dereference handling.

**Triggers:**
- `/proc/null_deref_trigger` - write "read" for null read
- `/proc/null_deref_trigger` - write "write" for null write
- `/proc/null_deref_trigger` - write "struct" for struct member access
- `/proc/null_deref_trigger` - write "function" for function pointer call
- `/proc/null_deref_trigger` - write "offset" for offset access

**Effect:** Kernel oops

#### 9. invalid_access.ko
Tests various invalid memory access patterns.

**Triggers:**
- `/proc/invalid_access_trigger` - write "kernel" for invalid kernel address
- `/proc/invalid_access_trigger` - write "user" for direct userspace access
- `/proc/invalid_access_trigger` - write "unaligned" for unaligned access
- `/proc/invalid_access_trigger` - write "high" for out-of-range address
- `/proc/invalid_access_trigger` - write "freed" for freed page access
- `/proc/invalid_access_trigger` - write "exec" for executing data as code
- `/proc/invalid_access_trigger` - write "io" for invalid I/O address

**Effect:** Kernel oops

## Testing Workflow

### Systematic Testing

1. **Memory Debugging**
   ```bash
   # Test KASAN
   insmod use_after_free.ko
   echo "trigger" > /proc/uaf_trigger
   dmesg | grep KASAN
   
   # Test SLUB debug
   insmod double_free.ko
   echo "trigger" > /proc/double_free_trigger
   dmesg | grep SLUB
   ```

2. **Lock Debugging**
   ```bash
   # Test lockdep
   insmod deadlock.ko
   echo "trigger" > /proc/deadlock_trigger
   dmesg | grep lockdep
   
   # Test race conditions
   insmod missing_lock.ko
   echo "race" > /proc/missing_lock_trigger
   sleep 2
   cat /proc/missing_lock_status
   ```

3. **Address Translation**
   ```bash
   # ⚠️ Only if you're ready for oops!
   insmod null_deref.ko
   echo "read" > /proc/null_deref_trigger
   # System will oops
   ```

### Checking Debug Output

**KASAN reports:**
```bash
dmesg | grep -i kasan
```

**Lockdep warnings:**
```bash
dmesg | grep -i lockdep
```

**SLUB debug:**
```bash
dmesg | grep -i slub
```

**All debug messages:**
```bash
dmesg | grep -E "(KASAN|lockdep|SLUB|BUG|Oops)" | tail -30
```

## Makefile Targets

- `make` or `make all` - Build all modules
- `make clean` - Clean build artifacts
- `make install` - Build and copy to install/ directory
- `make list` - List all available modules
- `make verify` - Verify built modules
- `make help` - Show help

## Environment Variables

You can override the following variables:

```bash
export KERNEL_DIR=/path/to/kernel/source
export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-
make
```

## Debugging Tips

1. **Enable verbose kernel messages:**
   ```bash
   echo 8 > /proc/sys/kernel/printk
   ```

2. **Clear dmesg before testing:**
   ```bash
   dmesg -C
   # Run test
   dmesg
   ```

3. **Save debug output:**
   ```bash
   dmesg > /tmp/debug_output.txt
   ```

4. **Check enabled debug options:**
   ```bash
   zcat /proc/config.gz | grep -E "(KASAN|SLUB_DEBUG|LOCKDEP|KFENCE)"
   ```

## Safety Notes

⚠️ **Important:**
- These modules are intentionally buggy - for testing only!
- Address translation modules WILL crash the kernel
- Some tests may require system reboot
- Do NOT use on production systems
- Some tests (like self-deadlock) will hang the system

## Cleanup

To unload all modules:
```bash
rmmod invalid_access null_deref missing_lock double_unlock deadlock \
      slab_corruption double_free buffer_overflow use_after_free 2>/dev/null
```

Or use the test script:
```bash
./test-all-bugs.sh
# Select option 5: Unload All Modules
```

## Troubleshooting

**Module won't load:**
- Check kernel version matches: `uname -r` vs module vermagic
- Verify ARM architecture: `file module.ko`
- Check dmesg for errors: `dmesg | tail`

**No debug output:**
- Verify debug options enabled in kernel config
- Check printk level: `cat /proc/sys/kernel/printk`
- Look for messages: `dmesg | grep -i debug`

**Build fails:**
- Verify KERNEL_DIR points to correct kernel source
- Check cross-compiler: `arm-linux-gnueabihf-gcc --version`
- Ensure kernel is built: `ls $KERNEL_DIR/.config`

## References

- Main buildroot guide: `/home/raghub/raghu/buildroot/ARM_VExpress_Build_Guide.md`
- Module development guide: `/home/raghub/raghu/buildroot/Out_of_Tree_Module_Development.md`
- Kernel documentation: `Documentation/dev-tools/` in kernel source

## License

GPL v2 - For testing and educational purposes only.
# Buggy Kernel Modules for Debug Testing
<!--
Copyright (c) 2024 TECH VEDA(www.techveda.org)
Author: Raghu Bharadwaj

This software is dual-licensed under the MIT License and GPL v2.
See the accompanying LICENSE file for the full text.
-->
