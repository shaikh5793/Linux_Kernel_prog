# PCI Driver Examples

Simple PCI driver examples for Linux kernel learning.

## Files

1. **pci_basic.c** - Basic PCI probe/remove and BARs
2. **pci_mmio.c** - Memory-mapped I/O operations  
3. **pci_irq.c** - Legacy interrupt handling
4. **pci_msi.c** - MSI (Message Signaled Interrupts)
5. **pci_msix.c** - MSI-X (Extended MSI)
6. **pci_dma.c** - DMA buffer operations
7. **pci_ethernet.c** - Ethernet driver example

## Building

```bash
# Build all modules
make

# Clean build artifacts
make clean
```

## Key Concepts

### Basic PCI (pci_basic.c)
- PCI driver registration
- Device probe/remove
- BAR (Base Address Register) handling
- Resource management

### MMIO (pci_mmio.c)
- Memory-mapped I/O operations
- Register read/write (8/16/32-bit)
- Safe register access

### Interrupts (pci_irq.c)
- Legacy INTx interrupt handling
- Shared interrupts (IRQF_SHARED)
- Interrupt statistics

### MSI (pci_msi.c)
- Message Signaled Interrupts
- Multiple vectors (up to 32)
- Dedicated interrupts (no sharing)

### MSI-X (pci_msix.c)
- Extended MSI (up to 2048 vectors)
- DMA table management
- Per-vector masking

### DMA (pci_dma.c)
- Coherent DMA buffers
- Streaming DMA
- Cache synchronization
- 32/64-bit DMA masks

### Ethernet (pci_ethernet.c)
- Network device registration
- Ring buffer management
- NAPI polling
- Packet TX/RX

## Testing

1. Check available PCI devices: `lspci -nn`
2. Load module: `sudo insmod pci_basic.ko`
3. Check messages: `dmesg`
4. Unload module: `sudo rmmod pci_basic`

## Device Matching

Examples target common vendor IDs:
- Intel: 0x8086
- Xilinx: 0x10ee
- Test: 0x1234

Modify `pci_device_id` tables for your hardware.

## License

MIT License - Educational use

---
**Author**: Raghu Bharadwaj (raghu@techveda.org)