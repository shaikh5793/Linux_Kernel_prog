<!--
Copyright (c) 2024 TECH VEDA(www.techveda.org)
Author: Raghu Bharadwaj

This software is dual-licensed under the MIT License and GPL v2.
See the accompanying LICENSE file for the full text.
-->

#Page Allocator Examples (p1_page)

This directory contains kernel modules demonstrating Linux page allocator interfaces and concepts.

## Files (Recommended Learning Order)

1. **`alloc.c`** - Basic page allocation interfaces (`alloc_page`, `alloc_pages`, `__free_pages`)
2. **`mask.c`** - GFP (Get Free Pages) masks and allocation flags (`GFP_KERNEL`, `GFP_ATOMIC`, etc.)  
3. **`zones.c`** - Memory zones demonstration (`ZONE_DMA`, `ZONE_NORMAL`, `ZONE_HIGHMEM`)
4. **`numa.c`** - NUMA-aware page allocation and node-specific memory operations
5. **`pfn.c`** - Page Frame Number (PFN) management and conversion operations
6. **`limits.c`** - Page allocator limits and maximum order allocation testing
7. **`cma_large.c`** - CMA (Contiguous Memory Allocator) large block allocation demonstration

### Module Descriptions (Study in Order)

#### 1. alloc.c - Page Allocation Basics
Demonstrates fundamental page allocator interfaces:
- Single page allocation with `alloc_page(GFP_KERNEL)`
- Multi-page allocation with `alloc_pages(GFP_KERNEL, order)`
- Order-based allocation (order 0=1 page, order 1=2 pages, order 2=4 pages, etc.)
- Memory writing and verification using `memset()`
- Proper memory cleanup with `__free_pages()`
- Array allocation patterns for multiple separate pages

#### 2. mask.c - GFP Allocation Flags
Explores different allocation behaviors through GFP masks:
- **`GFP_KERNEL`** - Standard kernel allocation (can sleep)
- **`GFP_ATOMIC`** - Atomic allocation (cannot sleep, for interrupts)
- **`GFP_HIGHUSER`** - High memory allocation for user pages
- **Modifier flags**: `__GFP_ZERO`, `__GFP_COMP`, `__GFP_NOWARN`, etc.
- Flag combinations and their effects on allocation behavior
- Zero-initialization verification and compound page testing

#### 3. zones.c - Memory Zone Management
Demonstrates memory zone concepts and zone-specific allocations:
- Zone information display (`ZONE_DMA`, `ZONE_NORMAL`, `ZONE_HIGHMEM`)
- Zone-specific allocation attempts with different GFP flags
- Zone fallback behavior when preferred zones are unavailable
- Memory statistics (total RAM, high memory, low memory)
- Zone characteristics and performance implications
- Cross-zone allocation patterns and success rates

#### 4. numa.c - NUMA-Aware Memory Allocation
Demonstrates Non-Uniform Memory Access (NUMA) aware page allocation:
- NUMA node detection and system topology discovery
- Node-specific allocation with `alloc_pages_node()`
- Local vs. remote memory allocation patterns
- NUMA policy effects on allocation behavior
- Cross-node allocation performance implications
- Memory locality optimization techniques
- NUMA-aware GFP flag usage and combinations

#### 5. pfn.c - Page Frame Number Operations
Comprehensive PFN management and conversion demonstrations:
- Basic PFN conversion (`page_to_pfn`, `pfn_to_page`)
- Multi-page allocation PFN ranges and arithmetic
- Complete conversion chains between different memory representations
- PFN validation and boundary checking (`pfn_valid`)
- Physical address calculation from PFNs
- System-wide PFN statistics and memory layout understanding

#### 6. limits.c - Allocation Limits Testing
Tests the boundaries and limits of the page allocator:
- Progressive order testing to find maximum allocatable order
- Boundary condition testing (success/failure thresholds)
- Different GFP flag behavior at allocation limits
- Large allocation testing and failure pattern analysis
- Memory pressure impact on allocation success
- Human-readable size conversion (bytes/KB/MB/GB)
- System memory capacity analysis

#### 7. cma_large.c - CMA Large Block Allocation
Demonstrates Contiguous Memory Allocator (CMA) for large allocations:
- CMA availability detection (`CONFIG_CMA` check)
- Large block allocation testing (4MB-64MB orders)
- Comparison between normal allocator and CMA-friendly flags
- Multiple large allocation stress testing
- CMA configuration requirements and setup instructions
- Demonstration of allocation strategies beyond buddy allocator limits


## PFN (Page Frame Number) Concepts

### What is a PFN?
- **PFN = Page Frame Number** - An index representing a physical memory page
- Each PFN represents exactly one physical page in the system
- PFNs provide a way to uniquely identify and reference physical memory pages

### Address Conversion Formulas
```
PFN = Physical Address >> PAGE_SHIFT
Physical Address = PFN << PAGE_SHIFT

Example (4KB pages, PAGE_SHIFT=12):
Physical Address: 0x00100000 -> PFN: 0x100 (256)
PFN: 256 -> Physical Address: 0x00100000
```

### Memory Layout Relationship
```
Physical Memory                    Virtual Memory
┌─────────────────┐               ┌─────────────────┐
│  Page Frame 0   │ <- PFN 0      │  Virtual Page   │
│   (4KB/8KB)     │               │   0xFFFF8000... │
├─────────────────┤               ├─────────────────┤
│  Page Frame 1   │ <- PFN 1      │  Virtual Page   │
│   (4KB/8KB)     │               │   0xFFFF8001... │
├─────────────────┤               ├─────────────────┤
│  Page Frame 2   │ <- PFN 2      │  Virtual Page   │
│   (4KB/8KB)     │               │   0xFFFF8002... │
└─────────────────┘               └─────────────────┘
```

## Kernel PFN API Reference

### Core Conversion Functions
- **`page_to_pfn(page)`** - Convert struct page* to PFN
- **`pfn_to_page(pfn)`** - Convert PFN to struct page*
- **`page_address(page)`** - Get virtual address from struct page*
- **`virt_to_page(addr)`** - Get struct page* from virtual address

### Validation Functions
- **`pfn_valid(pfn)`** - Check if PFN is valid and backed by RAM

### Address Conversion Macros
- **`pfn << PAGE_SHIFT`** - Convert PFN to physical address
- **`addr >> PAGE_SHIFT`** - Convert physical address to PFN

### PFN Arithmetic
- **Addition/Subtraction** - Use for range calculations and page iteration
- **PFN differences** - Calculate number of pages between two PFNs
- **Memory distance** - `(pfn2 - pfn1) * PAGE_SIZE` gives byte distance

## Memory Management Concepts

### Order-Based Allocation System
- **Order 0**: 1 page = 4KB (most common)
- **Order 1**: 2 pages = 8KB  
- **Order 2**: 4 pages = 16KB
- **Order 3**: 8 pages = 32KB
- **Higher orders**: Exponentially larger, harder to allocate

#### Maximum Order Configuration
MAX_ORDER configuration has been modernized:

```
CONFIG_SET_MAX_ORDER=10  # Default is 10, range: 11-255
```

**Kconfig Dependencies:**
- Requires `CONFIG_SPARSEMEM_VMEMMAP=y`
- Requires `ARCH_FORCE_MAX_ORDER=0` (automatically met on x86_64)

**Menu Location in `make menuconfig`:**
```
Memory Management options  --->
    [*] Sparse Memory virtual memmap
    (11) Set maximum order of buddy allocator  # <-- SET_MAX_ORDER
```

**Legacy vs Modern Configuration:**
- **Old**: `CONFIG_FORCE_MAX_ZONEORDER` (removed in recent kernels)
- **New**: `CONFIG_SET_MAX_ORDER`

**Direct Source Modification (if Kconfig unavailable):**
If the Kconfig option isn't visible, you can modify the kernel source directly:

1. **Edit** `include/linux/mmzone.h`:
```c
// Find this line and modify the value:
#define MAX_ORDER 11  // Change to desired value (e.g., 13)
```

2. **Alternative**: Edit `mm/Kconfig` and add/modify:
```
config SET_MAX_ORDER
    int "Set maximum order of buddy allocator"
    range 11 255
    default 11
    depends on SPARSEMEM_VMEMMAP
```

**Current System Check:**
```bash
# Check current MAX_ORDER value
cat /proc/buddyinfo
# Shows available free blocks per order (0 to MAX_ORDER-1)

# Check kernel config
grep CONFIG_SET_MAX_ORDER /boot/config-$(uname -r)
# Or check for legacy option
grep CONFIG_FORCE_MAX_ZONEORDER /boot/config-$(uname -r)
```

**Maximum Order Limits and Recommendations:**

*Theoretical Maximum:*
- **CONFIG_SET_MAX_ORDER Range**: 11-255 (theoretical limit)
- **Order 254**: 2^254 pages (astronomically large, exceeds all physical memory)

*Practical Limits by System:*

| System RAM | Practical Max Order | Max Allocation Size | Recommended MAX_ORDER |
|------------|-------------------|-------------------|---------------------|
| 1GB        | 10-12             | 4-16MB           | 13                  |
| 8GB        | 12-15             | 16-128MB         | 14                  |
| 32GB       | 15-18             | 128MB-1GB        | 15                  |
| 128GB+     | 18-20             | 1-4GB            | 16-18               |

*Architecture-Specific Considerations:*
- **x86_64**: Can handle higher orders than other architectures
- **ARM**: May have lower practical limits due to memory layout
- **Embedded**: Usually limited to MAX_ORDER 11-12 due to small RAM

*Fragmentation Impact:*
- **Order 10-11**: Usually successful early in boot
- **Order 12-15**: Possible with good memory conditions
- **Order 16+**: Very unlikely after system has been running (use CMA instead)

*Safe Configuration Values:*
- **Desktop/Server**: MAX_ORDER 13-15 (supports up to 64-512MB allocations)
- **Embedded Systems**: MAX_ORDER 11-12 (supports up to 16MB allocations)  
- **High-memory Servers**: MAX_ORDER 16-18 (supports up to 1-4GB allocations)

**Important Notes:**
- MAX_ORDER value = maximum order + 1 (i.e., MAX_ORDER=11 allows up to order 10)
- Higher MAX_ORDER values increase kernel memory overhead
- Memory fragmentation makes high-order allocations fail frequently after boot
- Boot-time allocations have much higher success rates than runtime allocations
- Consider CMA (Contiguous Memory Allocator) for reliable large allocations
- Kernel rebuild required after configuration changes
- Test thoroughly - setting MAX_ORDER too high can cause boot failures

## CMA (Contiguous Memory Allocator)

### What is CMA?
CMA is a Linux kernel framework that reserves contiguous memory regions at boot time for large allocations that require physically contiguous memory. Unlike the buddy allocator, CMA can reliably provide multi-megabyte allocations.

### CMA Configuration Requirements

#### 1. Kernel Configuration
```bash
# Required in kernel config:
CONFIG_CMA=y                    # Enable CMA framework
CONFIG_DMA_CMA=y               # DMA CMA support (optional)
CONFIG_CMA_SIZE_MBYTES=128     # Default CMA pool size (optional)
```

#### 2. Boot Parameters
Add to kernel command line (in GRUB or bootloader):
```bash
# Reserve 128MB for CMA
cma=128M

# Alternative: reserve 256MB
cma=256M

# Percentage-based: reserve 10% of total RAM
cma=10%
```

#### 3. Check Current CMA Status
```bash
# Check if CMA is enabled
grep CONFIG_CMA /boot/config-$(uname -r)

# Check CMA regions (if enabled)
cat /proc/cmainfo

# Check boot parameters
cat /proc/cmdline | grep cma
```

### CMA vs Buddy Allocator Comparison

| Aspect | Buddy Allocator | CMA |
|--------|----------------|-----|
| **Max Reliable Size** | ~4MB (order 10) | 64MB+ (depends on reserved size) |
| **Allocation Speed** | Very fast | Slower (migration may be needed) |
| **Fragmentation** | Suffers from fragmentation | Pre-reserved, no fragmentation |
| **Memory Efficiency** | High (all memory available) | Lower (reserves memory at boot) |
| **Use Cases** | General kernel allocations | DMA buffers, large contiguous blocks |
| **Runtime Reliability** | Decreases over time | Consistent reliability |

### CMA Allocation Methods
```c
// Method 1: GFP flags that may use CMA
alloc_pages(GFP_HIGHUSER_MOVABLE, order);  // May use CMA for large orders
alloc_pages(GFP_USER | __GFP_MOVABLE, order);

// Method 2: DMA coherent (often CMA-backed)
dma_alloc_coherent(dev, size, &dma_addr, GFP_KERNEL);

// Method 3: Direct CMA API (if available)
alloc_pages(__GFP_MOVABLE | __GFP_CMA, order);  // Requires newer kernels
```

### When to Use CMA
- **Device drivers** needing large DMA buffers
- **Graphics/video** requiring large framebuffers  
- **Networking** for zero-copy packet processing
- **Any case** requiring guaranteed large contiguous allocations

### CMA Limitations
- **Boot-time reservation** reduces available system memory
- **Migration overhead** when freeing CMA pages back to buddy allocator
- **Configuration complexity** requires kernel rebuild and boot parameter changes
- **Size limits** based on total system memory and fragmentation

### GFP Flag Categories

#### Basic Allocation Flags
- **`GFP_KERNEL`** - Standard kernel allocation (can sleep)
- **`GFP_ATOMIC`** - Interrupt context allocation (cannot sleep)
- **`GFP_HIGHUSER`** - High memory allocation for user pages

#### Modifier Flags (combine with basic flags)
- **`__GFP_ZERO`** - Zero-initialize allocated memory
- **`__GFP_COMP`** - Create compound pages (for huge pages)
- **`__GFP_NOWARN`** - Suppress failure warning messages
- **`__GFP_RETRY_MAYFAIL`** - Retry but allow failure
- **`__GFP_NOMEMALLOC`** - Don't use emergency reserves
- **`__GFP_NOFAIL`** - Never fails, retries indefinitely (⚠️ use with caution)
- **`__GFP_THISNODE`** - NUMA: no fallback to other nodes
- **`__GFP_NORETRY`** - NUMA: fail fast if node busy

### Memory Zone Layout
- **ZONE_DMA**: 0-16MB (device compatibility)
- **ZONE_NORMAL**: Kernel direct mapping
- **ZONE_HIGHMEM**: >896MB on 32-bit (temporary mapping)
  - Not available on 64-bit systems (direct mapping)

#### Zone Target Mapping
- `GFP_DMA` → ZONE_DMA
- `GFP_KERNEL` → ZONE_NORMAL  
- `GFP_HIGHUSER` → ZONE_HIGHMEM
- `GFP_USER` → ZONE_NORMAL

### NUMA (Non-Uniform Memory Access)
- **Local allocations**: Fastest access, lowest latency
- **Remote allocations**: Slower access, higher latency
- **alloc_pages_node()**: Target specific NUMA node
- **alloc_pages()**: Use current node with fallback

### System Limits and Behavior
- **Maximum order**: Varies by system (typically 10-11)
- **Fragmentation impact**: Higher orders fail more often
- **Memory pressure**: Affects allocation success rates
- **Zone fallback**: Allocator tries other zones if preferred unavailable

## Real-World Usage

### Where These Concepts Apply
- **Device drivers** - DMA buffer allocation and hardware memory
- **Network subsystem** - Packet buffer management
- **File systems** - Page cache and buffer management
- **Memory allocators** - Custom allocation strategies
- **NUMA optimization** - Locality-aware memory placement
- **Memory debugging** - Low-level analysis and profiling

### Common Use Cases
- **Single pages**: Most kernel data structures
- **Multi-page allocations**: Large buffers, DMA transfers
- **Zone-specific**: Device compatibility requirements
- **NUMA-aware**: Performance-critical applications
- **Limit testing**: Understanding system capabilities
