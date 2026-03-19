/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#include <stdio.h>    /* For printf() */
#include <pci/pci.h>  /* Main libpci header - contains all PCI definitions */

/*
 * PCI Configuration Space Register Offsets
 * 
 * The PCI configuration space is a standardized 256-byte area that contains
 * device identification, configuration, and control information. The first
 * 64 bytes are standardized across all PCI devices (Type 0 header).
 * 
 * STANDARD CONFIGURATION REGISTERS:
 * - 0x00-0x01: Vendor ID (identifies the manufacturer)
 * - 0x02-0x03: Device ID (identifies the specific device)
 * - 0x04-0x05: Command register (device control)
 * - 0x06-0x07: Status register (device status)
 * - 0x08: Revision ID (device revision)
 * - 0x09-0x0B: Class code (device function)
 * - 0x0C: Cache Line Size
 * - 0x0D: Latency Timer
 * - 0x0E: Header Type
 * - 0x0F: BIST (Built-in Self Test)
 * - 0x10-0x27: Base Address Registers (BARs)
 * - 0x3C: Interrupt Line
 * - 0x3D: Interrupt Pin
 */

int main() {
    struct pci_access *pacc;
    struct pci_dev *dev;
    unsigned int vendor_id, device_id;

    /*
     * Standard libpci initialization sequence
     * 
     * This is the standard pattern for all libpci programs:
     * 1. pci_alloc() - Allocate the main access structure
     * 2. pci_init() - Initialize the library and detect access methods
     * 3. pci_scan_bus() - Scan the PCI bus and discover all devices
     */
    pacc = pci_alloc();
    pci_init(pacc);
    pci_scan_bus(pacc);

    /*
     * Iterate through all discovered PCI devices
     * 
     * The devices are linked in a simple linked list accessible
     * through pacc->devices. Each device structure contains basic
     * addressing information (domain, bus, device, function).
     */
    for (dev = pacc->devices; dev; dev = dev->next) {
        /*
         * Request device identification information
         * 
         * pci_fill_info() populates the device structure with
         * requested information. PCI_FILL_IDENT requests vendor ID,
         * device ID, and other basic identification data.
         */
        pci_fill_info(dev, PCI_FILL_IDENT);

        /*
         * Direct configuration space register access
         * 
         * pci_read_word() reads a 16-bit value from the configuration space.
         * This demonstrates direct register access using standard offsets:
         * - PCI_VENDOR_ID (0x00): 16-bit vendor identifier
         * - PCI_DEVICE_ID (0x02): 16-bit device identifier
         * 
         * These IDs uniquely identify the device type and can be used
         * to load appropriate drivers or determine device capabilities.
         */
        vendor_id = pci_read_word(dev, PCI_VENDOR_ID);  /* Offset 0x00 */
        device_id = pci_read_word(dev, PCI_DEVICE_ID);  /* Offset 0x02 */

        /*
         * Display device information in standard PCI addressing format
         * 
         * Format: domain:bus:device.function
         * - domain: PCI domain (usually 0000 on single-domain systems)
         * - bus: PCI bus number (0-255)
         * - device: Device number on the bus (0-31)
         * - function: Function number within the device (0-7)
         */
        printf("PCI Device: %04x:%02x:%02x.%d - Vendor ID: %04x, Device ID: %04x\n",
               dev->domain, dev->bus, dev->dev, dev->func, vendor_id, device_id);
    }

    /*
     * Cleanup libpci resources
     * 
     * Always call pci_cleanup() to free allocated memory and
     * close any open file descriptors used by libpci.
     */
    pci_cleanup(pacc);
    
    return 0;
}
