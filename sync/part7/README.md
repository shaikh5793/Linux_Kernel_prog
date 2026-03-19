<!--
Copyright (c) 2024 TECH VEDA(www.techveda.org)
Author: Raghu Bharadwaj

This software is dual-licensed under the MIT License and GPL v2.
See the accompanying LICENSE file for the full text.
-->

# Part 4: Timing and Delay Mechanisms

This directory contains kernel modules demonstrating various timing mechanisms, delay functions, and time-based operations in the Linux kernel.

## Modules Overview

### Time Measurement
- **jiff.c** - Jiffies-based time measurement and operations
- **ktime.c** - High-resolution kernel time (ktime) operations

### Delay Mechanisms
- **delays.c** - Various kernel delay functions and timing operations
- **sleeps.c** - Different sleep mechanisms and thread suspension

## Key Concepts

### Jiffies
- Kernel's fundamental time unit
- System timer tick counter since boot
- Used for timeouts, scheduling, and time measurements
- Resolution depends on CONFIG_HZ (typically 100, 250, or 1000 Hz)

### High-Resolution Time (ktime)
- Nanosecond-precision time measurement
- Independent of jiffies and HZ value
- Used for precise timing requirements
- Essential for high-resolution timers

### Delay Functions
- **udelay()** - Microsecond busy-wait delays (blocking)
- **mdelay()** - Millisecond busy-wait delays (blocking)
- **usleep_range()** - Microsecond sleep with range (non-blocking)
- **msleep()** - Millisecond sleep (non-blocking)
- **ssleep()** - Second sleep (non-blocking)

### Sleep Mechanisms
- **schedule()** - Voluntary CPU yield
- **wait_event()** - Conditional sleep with wake-up conditions
- **msleep_interruptible()** - Interruptible millisecond sleep
- **schedule_timeout()** - Sleep with timeout

## Building

```bash
make build
```

## Cleaning

```bash
make clean
```

## Usage

Load modules individually:
```bash
sudo insmod <module_name>.ko
```

Unload modules:
```bash
sudo rmmod <module_name>
```

Monitor kernel messages:
```bash
dmesg | tail -f
```

## Important Notes

### Delay Function Selection
- Use **busy-wait delays** (udelay/mdelay) only for very short delays (<10ms)
- Use **sleep functions** for longer delays to avoid wasting CPU cycles
- **Interruptible sleeps** allow signal interruption
- **Non-interruptible sleeps** cannot be interrupted by signals

### Context Considerations
- Busy-wait delays can be used in any context (including interrupt context)
- Sleep functions can only be used in process context
- Choose appropriate delay mechanism based on timing requirements and context