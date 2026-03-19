#!/bin/bash
#
# Copyright (c) 2024 TECH VEDA(www.techveda.org)
# Author: Raghu Bharadwaj
#
# This software is dual-licensed under the MIT License and GPL v2.
# See the accompanying LICENSE file for the full text.
#
# Deploy buggy modules to running QEMU instance

set -e

echo "========================================="
echo "  Deploy ALL Buggy Modules to QEMU"
echo "  (17 modules total)"
echo "========================================="
echo ""

# Check if QEMU is running
if ! nc -z localhost 2222 2>/dev/null; then
    echo "❌ ERROR: QEMU is not running or SSH not available on port 2222"
    echo ""
    echo "Start QEMU first:"
    echo "  cd /home/raghub/raghu/buildroot/output/images"
    echo "  ./boot-qemu-ssh.sh"
    exit 1
fi

echo "✓ QEMU is running"
echo ""

# Create target directory
echo "Creating /tmp/buggy_modules on QEMU..."
ssh -p 2222 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null root@localhost \
    "mkdir -p /tmp/buggy_modules" 2>/dev/null || true

# Count modules
MODULE_COUNT=$(find memory/ locking/ address/ -name "*.ko" | wc -l)
echo "Found $MODULE_COUNT modules to deploy"
echo ""

# Copy all modules
echo "Copying modules..."
echo "  Memory bugs: $(ls memory/*.ko 2>/dev/null | wc -l) modules"
echo "  Locking bugs: $(ls locking/*.ko 2>/dev/null | wc -l) modules"
echo "  Address bugs: $(ls address/*.ko 2>/dev/null | wc -l) modules"
echo ""

scp -P 2222 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
    memory/*.ko locking/*.ko address/*.ko \
    root@localhost:/tmp/buggy_modules/ 2>&1 | grep -v "Warning: Permanently added" || true

echo ""
echo "========================================="
echo "  ✓ Deployment Complete!"
echo "========================================="
echo ""
echo "$MODULE_COUNT modules deployed to QEMU:/tmp/buggy_modules/"
echo ""
echo "Memory bugs (7): use_after_free, buffer_overflow, double_free,"
echo "                 slab_corruption, invalid_free, oob_read, krealloc_misuse"
echo ""
echo "Locking bugs (8): deadlock, double_unlock, missing_lock,"
echo "                  sleep_in_atomic, spin_recursion, rwlock_misuse,"
echo "                  rwsem_misuse, rcu_misuse"
echo ""
echo "Address bugs (2): null_deref, invalid_access (⚠️ will crash)"
echo ""
echo "Test in QEMU console:"
echo "  cd /tmp/buggy_modules"
echo "  ls -lh *.ko"
echo ""
echo "Example - Test use-after-free:"
echo "  insmod use_after_free.ko"
echo "  echo 'trigger' > /proc/uaf_trigger"
echo "  dmesg | tail -30"
echo "  rmmod use_after_free"
echo ""
echo "Example - Test deadlock:"
echo "  insmod deadlock.ko"
echo "  echo 'trigger' > /proc/deadlock_trigger"
echo "  dmesg | grep -i lockdep"
echo "  rmmod deadlock"
echo ""
