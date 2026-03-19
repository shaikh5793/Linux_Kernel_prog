<!--
Copyright (c) 2024 TECH VEDA(www.techveda.org)
Author: Raghu Bharadwaj

This software is dual-licensed under the MIT License and GPL v2.
See the accompanying LICENSE file for the full text.
-->

# SysFS Counter Interface

## Overview
This example demonstrates user-kernel communication using the **SysFS** interface. It implements a simple counter service accessible through kernel objects at `/sys/kernel/counter/value`.

## Interface Details
- **Path**: `/sys/kernel/counter/value`
- **Operations**: Read current value, increment counter, reset counter
- **Access**: Standard file read/write operations

## Service Operations
| Operation | Command | Description |
|-----------|---------|-------------|
| Read      | `cat /sys/kernel/counter/value` | Display current counter value |
| Increment | `echo 1 > /sys/kernel/counter/value` | Increment counter by 1 |
| Reset     | `echo 0 > /sys/kernel/counter/value` | Reset counter to 0 |

## Key Concepts
- **Kernel Objects (kobject)**: Basic kernel object management
- **SysFS Hierarchy**: Structured filesystem representation
- **Kernel Attributes**: Direct kernel module parameter exposure
- **Attribute Groups**: Organization of related attributes
- **Independent Interface**: No device model dependency

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
- Uses kernel objects (kobject) for simple sysfs integration
- Creates entries directly under `/sys/kernel/`
- No device model complexity or dependencies
- Direct kernel module attribute exposure
- Automatic sysfs cleanup via kobject reference counting