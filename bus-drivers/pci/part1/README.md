# PCI Userspace Examples

Simple PCI programming examples using libpci library.

## Files

1. **pci_devices.c** - List and identify PCI devices
2. **pci_config.c** - Read PCI configuration space
3. **pci_bar.c** - Analyze Base Address Registers
4. **pci_irq.c** - Show PCI interrupt information
5. **pci_mmio.c** - Memory-mapped I/O access via /dev/mem
6. **pci_port.c** - Legacy I/O port access

## Building

```bash
# Install dependencies
sudo apt install libpci-dev

# Build all examples
make

# Clean
make clean
```

## Usage

```bash
# List all PCI devices
./pci_devices

# Show configuration registers
./pci_config

# Analyze BARs (requires root)
sudo ./pci_bar

# Show interrupt info
./pci_irq

# MMIO access (requires root)
sudo ./pci_mmio

# Port I/O access (requires root)
sudo ./pci_port
```

## Key Concepts

### Device Enumeration (pci_devices.c)
- PCI device discovery using libpci
- Vendor/device ID resolution
- Device addressing (bus:dev.func)

### Configuration Space (pci_config.c)
- Direct register access
- PCI header types
- Configuration space layout

### Base Address Registers (pci_bar.c)
- BAR detection and sizing
- Memory vs I/O space
- 32-bit vs 64-bit addressing

### Interrupts (pci_irq.c)
- Legacy INTx interrupt lines
- System interrupt correlation
- IRQ routing analysis

### Memory Access (pci_mmio.c)
- /dev/mem access patterns
- Memory-mapped register access
- mmap() usage for PCI

### Port I/O (pci_port.c)
- Legacy I/O port access
- ioperm() and iopl() usage
- Port-based register access

## Prerequisites

- libpci development headers: `sudo apt install libpci-dev`
- Root access for hardware access (mmio, port, bar)
- x86/x86_64 architecture for port I/O

## License

MIT License - Educational use

---
**Author**: Raghu Bharadwaj (raghu@techveda.org)
