<!--
Copyright (c) 2024 TECH VEDA(www.techveda.org)
Author: Raghu Bharadwaj

This software is dual-licensed under the MIT License and GPL v2.
See the accompanying LICENSE file for the full text.
-->

# VM VMA Inspector

## Overview
This example demonstrates detailed inspection of Virtual Memory Areas (VMAs) of the calling process using **DebugFS**. It provides comprehensive information about process memory layout, VMA properties, and memory usage statistics.

## Interface Details
- **Path**: `/sys/kernel/debug/inspectvma/vma_details`
- **Operations**: Read-only detailed VMA inspection
- **Access**: Root access required (DebugFS)

## Service Operations
| Operation | Command | Description |
|-----------|---------|-------------|
| Inspect VMAs | `sudo cat /sys/kernel/debug/inspectvma/vma_details` | Display all VMA details of reading process |

## Information Displayed

### **Memory Layout Overview**
- Code segment boundaries (start_code, end_code)
- Data segment boundaries (start_data, end_data)  
- Heap boundaries (start_brk, brk)
- Stack location (start_stack)
- Command line arguments (arg_start, arg_end)
- Environment variables (env_start, env_end)

### **VMA Details Table**
- **START/END**: Virtual address range
- **SIZE**: VMA size in KB
- **OFFSET**: File offset (for file-backed VMAs)
- **FLAGS**: Permission and property flags
- **TYPE/FILE**: VMA type ([heap], [stack], [anon]) or backing file name

### **VMA Flags Decoded**
- **Basic Permissions**: r(read), w(write), x(execute), s(shared)/p(private)
- **Growth**: gd(grows down), gu(grows up)
- **Access Control**: dw(deny write), lo(locked)
- **I/O**: io(I/O mapping)
- **Optimization**: sr(sequential read), rr(random read)
- **Memory Management**: ht(huge pages), nh(no huge pages), mg(mergeable)
- **Fork Behavior**: dc(don't copy), de(don't expand)

### **Memory Statistics**
- Total number of VMAs
- Total virtual memory usage
- RSS (Resident Set Size) - actual physical memory
- Peak virtual memory usage
- Peak RSS usage

## Key Concepts
- **VMA Structure**: Core virtual memory management unit
- **Memory Descriptor (mm_struct)**: Process memory management structure
- **Virtual Address Space Layout**: Code, data, heap, stack organization
- **Memory Flags**: VMA properties and permissions
- **Anonymous vs File-backed**: Different VMA types

## Build and Test
```bash
# Build module and all test programs
make

# Load module
sudo make install

# Run individual tests
sudo make test-vma         # Basic VMA inspection
sudo make test-stack       # Stack growth analysis
sudo make test-malloc      # malloc() and heap behavior

# Run all tests in sequence
sudo make test-all

# Or run specific test programs directly
sudo ./test_vma
sudo ./test_stack
sudo ./test_malloc
sudo ./test_mmap1        # Basic mmap operations (recommended starting point)
sudo ./test_mmap2        # Advanced VMA management

# Makefile targets for mmap tests
sudo make test-mmap1
sudo make test-mmap2

# Manually inspect current shell's VMAs
sudo cat /sys/kernel/debug/inspectvma/vma_details

# Unload module  
sudo make uninstall

# Clean build files
make clean
```

## Individual Test Programs

### **test_vma** - Basic VMA Details
- Shows basic VMA inspection and memory layout overview
- Displays process memory structure (code, data, heap, stack)
- Demonstrates VMA information reading from debugfs
- Provides foundation understanding of VMA concepts

### **test_stack** - Stack Growth Analysis
- Tests stack VMA behavior through recursive function calls
- Shows large stack allocations and growth patterns
- Compares stack vs heap address ranges
- Demonstrates automatic stack expansion

### **test_malloc** - malloc() and Heap Analysis
- Shows how malloc() affects heap VMA
- Demonstrates small vs large allocation strategies
- Shows difference between brk() and mmap() for large allocations
- Uses mincore() to check memory residency
- Traces heap growth and memory usage patterns


### **test_mmap1** - Fundamental mmap Operations
- **Focus**: Anonymous mappings and file-backed mappings with interactive learning
- Anonymous mmap() with lazy allocation demonstration
- File-backed mmap() with shared mapping
- Memory residency checking with mincore()
- Step-by-step execution with getchar() controls
- Clear explanations of virtual vs physical memory
- **Best for**: Understanding basic mmap concepts, lazy allocation, and file mapping fundamentals

### **test_mmap2** - Advanced VMA Management
- **Focus**: mprotect(), VMA splitting, and complex memory protection patterns
- Multiple mappings with different sizes (1MB, 2MB, 4MB, 8MB)
- VMA splitting demonstration with mprotect()
- Memory protection testing with segfault handling
- Complex protection patterns (READ, WRITE, NONE)
- Advanced VMA fragmentation examples
- **Best for**: Understanding VMA splitting, memory protection mechanisms, and kernel VMA management

### **Key Features Added**
- **mincore() Integration**: All tests use `mincore()` to show lazy allocation
- **Focused Learning**: Each test focuses on one specific concept
- **Progressive Complexity**: Tests build understanding step by step
- **Real Memory Tracking**: Shows difference between virtual and physical memory

## Educational Benefits of Split mmap Tests

### **test_mmap1** Interactive Flow:
1. Initial VMA state display
2. 4MB anonymous mapping creation and observation
3. Lazy allocation demonstration (before touching pages)
4. Page touching to trigger physical allocation  
5. Lazy allocation demonstration (after touching pages)
6. File creation and mapping demonstration
7. File modification through memory mapping
8. Cleanup and final state observation

### **test_mmap2** Interactive Flow:
1. Multiple anonymous mappings creation
2. VMA splitting with mprotect() (2MB → 3 VMAs)
3. Protection violation testing 
4. Complex protection patterns (4MB → 4 VMAs)
5. Cleanup and analysis

### Learning Outcomes:
- **From test_mmap1**: Virtual vs physical memory allocation, lazy allocation and demand paging, anonymous vs file-backed VMAs, memory mapping lifecycle, file modification through mapping
- **From test_mmap2**: VMA splitting mechanics, memory protection enforcement, complex protection patterns, kernel VMA management strategies, memory access violation handling

### Enhanced Learning Experience:
- **Focused Content**: Each test concentrates on specific concepts
- **Better Pacing**: Interactive getchar() controls allow observation
- **Clear Progression**: Logical flow from basic to advanced concepts
- **Detailed Explanations**: Context-specific guidance throughout execution
- **Granular Control**: Pause at critical VMA state changes for better understanding

## Implementation Notes
- Uses `current` task to access calling process
- Safely locks memory descriptor with `mmap_read_lock()`
- Walks VMA linked list from `mm->mmap`
- Provides comprehensive flag interpretation
- Handles both anonymous and file-backed VMAs
- Shows relationship between different memory segments

## Example Output
```
VMA Inspector for Process: bash (PID: 1234)
==========================================================

Memory Layout Overview:
  Code Start:  0x00400000
  Code End:    0x004a5000
  ...

Virtual Memory Areas (VMAs):
START              END                SIZE       OFFSET FLAGS                TYPE/FILE
================== ================== ========== ====== ==================== ================
0x0000000000400000 0x00000000004a5000      676k      0 r-xp                 bash
0x00000000006a4000 0x00000000006a8000       16k   a4000 rw-p                 bash
0x0000000001c4d000 0x0000000001c6e000      132k      0 rw-p                 [heap]
...
```
