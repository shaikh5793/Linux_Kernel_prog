<!--
Copyright (c) 2024 TECH VEDA(www.techveda.org)
Author: Raghu Bharadwaj

This software is dual-licensed under the MIT License and GPL v2.
See the accompanying LICENSE file for the full text.
-->

# DebugFS Counter Interface

## Overview
This example demonstrates user-kernel communication using the **DebugFS** interface. It implements a simple counter service accessible through the debug filesystem at `/sys/kernel/debug/counter/value`.

## Interface Details
- **Path**: `/sys/kernel/debug/counter/value`
- **Operations**: Read current value, increment counter, reset counter
- **Access**: Standard file read/write operations (requires root/debug access)

## Service Operations
| Operation | Command | Description |
|-----------|---------|-------------|
| Read      | `cat /sys/kernel/debug/counter/value` | Display current counter value |
| Increment | `echo 1 > /sys/kernel/debug/counter/value` | Increment counter by 1 |
| Reset     | `echo 0 > /sys/kernel/debug/counter/value` | Reset counter to 0 |

## Key Concepts
- **DebugFS**: Virtual filesystem for kernel debugging information
- **Debug Directory**: Hierarchical organization of debug entries
- **Root Access**: DebugFS typically requires privileged access
- **Development Tool**: Intended for debugging and development, not production
- **Recursive Cleanup**: Automatic cleanup of directory trees

## Build and Test
```bash
# Build module
make

# Load module
sudo make install

# Test operations (requires root)
sudo ./test.sh

# Unload module  
sudo make uninstall

# Clean build files
make clean
```

## Implementation Notes
- Creates debug directory and file entries
- Uses same file operations as ProcFS
- Automatic recursive cleanup on module unload
- Requires debugfs to be mounted (usually at `/sys/kernel/debug/`)
- Intended for debugging and development purposes only
