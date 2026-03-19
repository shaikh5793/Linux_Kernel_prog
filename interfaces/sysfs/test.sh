#!/bin/bash

SYSFS_FILE="/sys/kernel/counter/value"

echo "=== SysFS Counter Service Test ==="

# Check if module is loaded
if [ ! -e "$SYSFS_FILE" ]; then
    echo "Error: $SYSFS_FILE not found. Is the module loaded?"
    echo "Run: sudo make install"
    exit 1
fi

# Test initial read
echo "1. Initial value:"
cat "$SYSFS_FILE"

# Test increment
echo "2. Increment counter (write 1):"
echo "1" > "$SYSFS_FILE"
echo "   Current value: $(cat $SYSFS_FILE)"

# Test another increment
echo "3. Increment again (write 1):"
echo "1" > "$SYSFS_FILE"  
echo "   Current value: $(cat $SYSFS_FILE)"

# Test reset
echo "4. Reset counter (write 0):"
echo "0" > "$SYSFS_FILE"
echo "   Current value: $(cat $SYSFS_FILE)"

# Test multiple increments
echo "5. Multiple increments:"
for i in {1..5}; do
    echo "1" > "$SYSFS_FILE"
done
echo "   Final value: $(cat $SYSFS_FILE)"

# Test invalid input
echo "6. Test invalid input (should fail):"
echo "2" > "$SYSFS_FILE" 2>/dev/null
if [ $? -ne 0 ]; then
    echo "   ✓ Invalid input rejected as expected"
else
    echo "   ✗ Invalid input accepted (unexpected)"
fi

echo "7. Final counter value:"
cat "$SYSFS_FILE"

# Show sysfs structure
echo "8. SysFS hierarchy:"
ls -la /sys/kernel/counter/

echo "=== Test Complete ==="