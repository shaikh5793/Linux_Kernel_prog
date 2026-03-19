#!/bin/bash
#
# Copyright (c) 2024 TECH VEDA(www.techveda.org)
# Author: Raghu Bharadwaj
#
# This software is licensed under GPL v2.
# See the accompanying LICENSE file for the full text.
#
# Deploy eBPF programs to ARM VExpress QEMU

QEMU_HOST="localhost"
QEMU_PORT="2222"
QEMU_USER="root"
REMOTE_DIR="/tmp/ebpf-examples"

echo "╔════════════════════════════════════════╗"
echo "║  Deploy eBPF Programs to QEMU        ║"
echo "╚════════════════════════════════════════╝"
echo ""

# Check if build complete
if [ ! -d "obj" ] || [ -z "$(ls -A obj/*.o 2>/dev/null)" ]; then
    echo "Error: No built eBPF objects found!"
    echo "Run 'make' first to build programs."
    exit 1
fi

# Check QEMU connectivity
echo "Checking QEMU connectivity..."
if ! nc -z $QEMU_HOST $QEMU_PORT 2>/dev/null; then
    echo "Error: Cannot connect to QEMU at ${QEMU_HOST}:${QEMU_PORT}"
    echo "Make sure QEMU is running with SSH forwarding."
    exit 1
fi

echo "✓ QEMU is accessible"
echo ""

# Create remote directory
echo "Creating remote directory..."
ssh -p $QEMU_PORT ${QEMU_USER}@${QEMU_HOST} "mkdir -p $REMOTE_DIR" || true

# Copy eBPF objects
echo "Copying eBPF programs..."
scp -P $QEMU_PORT obj/*.o ${QEMU_USER}@${QEMU_HOST}:${REMOTE_DIR}/

if [ $? -ne 0 ]; then
    echo "✗ Failed to copy files"
    exit 1
fi

echo "✓ eBPF programs copied"

# Copy helper scripts
echo "Copying helper scripts..."
scp -P $QEMU_PORT scripts/*.py ${QEMU_USER}@${QEMU_HOST}:${REMOTE_DIR}/ 2>/dev/null || true
scp -P $QEMU_PORT README.md ${QEMU_USER}@${QEMU_HOST}:${REMOTE_DIR}/ 2>/dev/null || true

echo ""
echo "╔════════════════════════════════════════╗"
echo "║  Deployment Complete!                 ║"
echo "╚════════════════════════════════════════╝"
echo ""
echo "eBPF programs deployed to:"
echo "  $REMOTE_DIR/"
echo ""
echo "Next steps:"
echo "  1. SSH to QEMU: ssh -p $QEMU_PORT ${QEMU_USER}@${QEMU_HOST}"
echo "  2. Go to directory: cd $REMOTE_DIR"
echo "  3. Load program: bpftool prog load memleak_tracker.o /sys/fs/bpf/memleak"
echo "  4. View programs: bpftool prog list"
echo "  5. View maps: bpftool map list"
echo ""
echo "See README.md for detailed usage examples."
echo ""
