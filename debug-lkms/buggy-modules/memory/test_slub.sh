#!/bin/sh
#
# Copyright (c) 2024 TECH VEDA(www.techveda.org)
# Author: Raghu Bharadwaj
#
# This software is dual-licensed under the MIT License and GPL v2.
# See the accompanying LICENSE file for the full text.
#
# Test SLUB debug detection

echo "====================================="
echo "  SLUB Debug Verification & Testing"
echo "====================================="
echo ""

# Check SLUB config
echo "Checking SLUB configuration..."
zcat /proc/config.gz 2>/dev/null | grep -E "SLUB|DEBUG_PAGEALLOC" || \
    grep -E "SLUB|DEBUG_PAGEALLOC" /proc/config.gz 2>/dev/null

echo ""
echo "Checking SLUB boot parameters..."
cat /proc/cmdline | grep -o "slub_debug[^ ]*"

echo ""
echo "====================================="
echo "Test 1: Redzone Violation (Most Reliable)"
echo "====================================="
insmod slab_corruption.ko
sleep 1
echo "redzones" > /proc/slab_trigger
sleep 1
echo ""
echo "Checking for SLUB errors:"
dmesg | tail -30 | grep -i -E "(slub|bug|corrupt|redzone|poison)"
rmmod slab_corruption

echo ""
echo "====================================="
echo "Test 2: Poison Pattern (UAF detection)"
echo "====================================="
insmod slab_corruption.ko
sleep 1
echo "poison" > /proc/slab_trigger
sleep 1
echo ""
echo "Checking for SLUB errors:"
dmesg | tail -30 | grep -i -E "(slub|bug|corrupt|redzone|poison)"
rmmod slab_corruption

echo ""
echo "====================================="
echo "Test 3: Metadata Corruption"
echo "====================================="
insmod slab_corruption.ko
sleep 1
echo "corrupt" > /proc/slab_trigger
sleep 1
echo ""
echo "Checking for SLUB errors:"
dmesg | tail -30 | grep -i -E "(slub|bug|corrupt|redzone|poison)"
rmmod slab_corruption

echo ""
echo "====================================="
echo "Summary"
echo "====================================="
echo "If you see SLUB errors above, detection is working!"
echo "If not, SLUB debug may need boot parameters."
echo ""
