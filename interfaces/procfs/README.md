<!--
Copyright (c) 2024 TECH VEDA(www.techveda.org)
Author: Raghu Bharadwaj

This software is dual-licensed under the MIT License and GPL v2.
See the accompanying LICENSE file for the full text.
-->

# ProcFS Counter Interface

## Overview
This example demonstrates user-kernel communication using the **ProcFS** interface. It implements a simple counter service accessible through `/proc/counter_service`.

## Interface Details
- **Path**: `/proc/counter_service` 
- **Operations**: Read current value, increment counter, reset counter
- **Access**: Standard file read/write operations

## Service Operations
| Operation | Command | Description |
|-----------|---------|-------------|
| Read      | `cat /proc/counter_service` | Display current counter value |
| Increment | `echo 1 > /proc/counter_service` | Increment counter by 1 |
| Reset     | `echo 0 > /proc/counter_service` | Reset counter to 0 |

## Key Concepts
- **proc_ops**: Modern ProcFS operation structure
- **seq_file**: Efficient output formatting for proc entries
- **copy_from_user**: Safe data transfer from userspace
- **single_open/single_release**: Simple single-entry proc files

## Build and Test
```bash
# Build module
make

# Load module
sudo make install

# Test operations
./test.sh

# Unload module  
sudo make uninstall

# Clean build files
make clean
```

## Implementation Notes
- Uses `seq_file` interface for reliable output
- Input validation prevents invalid operations
- Error handling for memory and parameter issues
- Standard module lifecycle management
