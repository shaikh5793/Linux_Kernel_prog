/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <pci/pci.h>



int main()
{
    struct pci_access *pacc;
    struct pci_dev *dev;
    int fd;
    void *mapped_mem;
    uint32_t *registers;
    uint32_t bar0_addr, bar0_size;

    printf("Simple MMIO Read Example\n");
    printf("========================\n");

    /*
     * Step 1: Initialize PCI library
     * 
     * Standard libpci initialization to discover and access PCI devices.
     */
    pacc = pci_alloc();
    pci_init(pacc);
    pci_scan_bus(pacc);

    /*
     * Step 2: Find first device with memory BAR
     * 
     * Search for a device that has a memory-mapped BAR (not I/O space).
     * We need a memory BAR to demonstrate MMIO access.
     */
    for (dev = pacc->devices; dev; dev = dev->next) {
        /*
         * Request BAR information from the device
         * 
         * PCI_FILL_BASES tells libpci to read all BAR registers
         * and populate the device structure with address information.
         */
        pci_fill_info(dev, PCI_FILL_BASES);
        
        /*
         * Check if BAR0 is memory space and implemented
         * 
         * BAR validation criteria:
         * - Must not be zero (indicates unimplemented BAR)
         * - Space bit (bit 0) must be 0 (memory space, not I/O space)
         * 
         * PCI_BASE_ADDRESS_SPACE_IO is the bit mask for space type.
         */
        uint32_t bar0 = pci_read_long(dev, PCI_BASE_ADDRESS_0);
        if (bar0 != 0 && !(bar0 & PCI_BASE_ADDRESS_SPACE_IO)) {
            printf("Found device: %04x:%02x:%02x.%d\n", 
                   dev->domain, dev->bus, dev->dev, dev->func);
            break;
        }
    }

    /*
     * Validate that we found a suitable device
     * 
     * If no device with a memory BAR is found, we cannot proceed
     * with the MMIO demonstration.
     */
    if (!dev) {
        printf("No device with memory BAR found\n");
        pci_cleanup(pacc);
        return 1;
    }

    /*
     * Step 3: Get BAR0 address and size
     * 
     * Extract the physical memory address where the device registers
     * are located and determine the size of the register space.
     * 
     * Apply the memory address mask to remove the control bits
     * (space type, memory type, prefetchable flag) and get the
     * actual physical address.
     */
    bar0_addr = pci_read_long(dev, PCI_BASE_ADDRESS_0) & PCI_BASE_ADDRESS_MEM_MASK;
    
    /*
     * Calculate BAR size using the standard probing technique
     * 
     * This is the same technique used in the BAR analysis example:
     * 1. Write all 1s to the BAR to determine which bits are implemented
     * 2. Read back the result to get the address mask
     * 3. Restore the original BAR value
     * 4. Calculate size using two's complement arithmetic
     */
    pci_write_long(dev, PCI_BASE_ADDRESS_0, 0xFFFFFFFF);
    uint32_t bar_mask = pci_read_long(dev, PCI_BASE_ADDRESS_0) & PCI_BASE_ADDRESS_MEM_MASK;
    pci_write_long(dev, PCI_BASE_ADDRESS_0, bar0_addr); /* Restore original */
    bar0_size = ~bar_mask + 1;

    /*
     * Display BAR information for debugging
     * 
     * Show the physical address and size of the device's register space.
     */
    printf("BAR0: Address=0x%08x, Size=%u bytes\n", bar0_addr, bar0_size);

    /*
     * Step 4: Open /dev/mem for physical memory access
     * 
     * /dev/mem is a character device that provides access to the system's
     * physical memory. This is a privileged operation that requires root
     * access for security reasons.
     * 
     * SECURITY NOTE: Direct memory access can corrupt system memory or
     * crash the system if used incorrectly. Production drivers use proper
     * kernel APIs instead of direct /dev/mem access.
     */
    fd = open("/dev/mem", O_RDONLY);
    if (fd == -1) {
        perror("Cannot open /dev/mem (need root)");
        pci_cleanup(pacc);
        return 1;
    }

    /*
     * Step 5: Map device memory into virtual address space
     * 
     * mmap() creates a virtual memory mapping from the device's physical
     * registers to our process's virtual address space. This allows us to
     * access device registers using normal memory operations.
     * 
     * PARAMETERS:
     * - NULL: Let kernel choose virtual address
     * - 4096: Map one page (minimum mapping size)
     * - PROT_READ: Read-only access (safer for demonstration)
     * - MAP_SHARED: Share mapping with other processes
     * - fd: /dev/mem file descriptor
     * - bar0_addr: Physical address to map
     */
    mapped_mem = mmap(NULL, 4096, PROT_READ, MAP_SHARED, fd, bar0_addr);
    if (mapped_mem == MAP_FAILED) {
        perror("mmap failed");
        close(fd);
        pci_cleanup(pacc);
        return 1;
    }

    /*
     * Step 6: Read from device registers
     * 
     * Now that the device memory is mapped into our virtual address space,
     * we can access device registers using normal memory operations.
     * Cast the mapped memory to a 32-bit pointer for register access.
     */
    registers = (uint32_t *)mapped_mem;
    
    /*
     * Read the first 4 registers for demonstration
     * 
     * Each register is typically 32 bits (4 bytes) wide, so:
     * - registers[0] = offset 0x00
     * - registers[1] = offset 0x04
     * - registers[2] = offset 0x08
     * - registers[3] = offset 0x0C
     * 
     * IMPORTANT: Reading from device registers may have side effects
     * depending on the device. Some registers change state when read.
     */
    printf("\nReading first 4 registers:\n");
    printf("Register 0 (0x00): 0x%08x\n", registers[0]);
    printf("Register 1 (0x04): 0x%08x\n", registers[1]);
    printf("Register 2 (0x08): 0x%08x\n", registers[2]);
    printf("Register 3 (0x0C): 0x%08x\n", registers[3]);

    /*
     * Step 7: Cleanup all resources
     * 
     * Proper cleanup is essential to avoid resource leaks:
     * 1. Unmap the virtual memory mapping
     * 2. Close the /dev/mem file descriptor
     * 3. Clean up libpci resources
     */
    munmap(mapped_mem, 4096);
    close(fd);
    pci_cleanup(pacc);

    printf("\nMMIO read complete!\n");
    return 0;
}
