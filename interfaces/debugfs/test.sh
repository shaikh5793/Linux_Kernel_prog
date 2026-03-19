#!/bin/bash

DEBUGFS_FILE="/sys/kernel/debug/counter/value"

echo "=== DebugFS Counter Service Test ==="

# Check if debugfs is mounted
if [ ! -d "/sys/kernel/debug" ]; then
    echo "Error: DebugFS not mounted. Try: sudo mount -t debugfs none /sys/kernel/debug"
    exit 1
fi

# Check if module is loaded
if [ ! -e "$DEBUGFS_FILE" ]; then
    echo "Error: $DEBUGFS_FILE not found. Is the module loaded?"
    echo "Run: sudo make install"
    exit 1
fi

# Ensure we're running as root
if [ "$EUID" -ne 0 ]; then
    echo "Error: DebugFS requires root access. Run with sudo."
    exit 1
fi

# Test initial read
echo "1. Initial value:"
cat "$DEBUGFS_FILE"

# Test increment
echo "2. Increment counter (write 1):"
echo "1" > "$DEBUGFS_FILE"
echo "   Current value: $(cat $DEBUGFS_FILE)"

# Test another increment
echo "3. Increment again (write 1):"
echo "1" > "$DEBUGFS_FILE"  
echo "   Current value: $(cat $DEBUGFS_FILE)"

# Test reset
echo "4. Reset counter (write 0):"
echo "0" > "$DEBUGFS_FILE"
echo "   Current value: $(cat $DEBUGFS_FILE)"

# Test multiple increments
echo "5. Multiple increments:"
for i in {1..5}; do
    echo "1" > "$DEBUGFS_FILE"
done
echo "   Final value: $(cat $DEBUGFS_FILE)"

# Test invalid input
echo "6. Test invalid input (should fail):"
echo "2" > "$DEBUGFS_FILE" 2>/dev/null
if [ $? -ne 0 ]; then
    echo "   ✓ Invalid input rejected as expected"
else
    echo "   ✗ Invalid input accepted (unexpected)"
fi

echo "7. Final counter value:"
cat "$DEBUGFS_FILE"

# Show debug directory structure
echo "8. Debug directory structure:"
ls -la /sys/kernel/debug/counter/

echo "=== Test Complete ==="