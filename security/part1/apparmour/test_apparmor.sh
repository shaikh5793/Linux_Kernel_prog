#!/bin/bash
# Test script for enhanced apparmor_info module

echo "=== Testing Enhanced AppArmor Module ==="
echo ""

# Check if AppArmor is available
if [ ! -d /sys/kernel/security/apparmor ]; then
    echo "❌ AppArmor not available on this system"
    exit 1
fi

echo "✓ AppArmor is available"
echo ""

# Remove old module if loaded
sudo rmmod apparmor_info 2>/dev/null

# Load the module
echo "Loading apparmor_info module..."
sudo insmod apparmor_info.ko

if [ $? -ne 0 ]; then
    echo "❌ Failed to load module"
    exit 1
fi

echo "✓ Module loaded successfully"
echo ""

# Display the module output
echo "=== Module Output: /proc/apparmor_info ==="
echo ""
cat /proc/apparmor_info

echo ""
echo "=== Comparing with aa-status output ==="
echo ""
sudo aa-status | head -20

echo ""
echo "=== Testing with different processes ==="
echo ""

# Test with bash (usually unconfined)
echo "Current shell confinement:"
cat /proc/self/attr/current

# Test with a confined process if available
if pgrep -x "snap" > /dev/null; then
    echo ""
    echo "Snap processes (usually confined):"
    for pid in $(pgrep snap | head -3); do
        echo "  PID $pid: $(cat /proc/$pid/attr/current 2>/dev/null || echo 'N/A')"
    done
fi

echo ""
echo "=== Cleanup ==="
sudo rmmod apparmor_info
echo "✓ Module removed"
