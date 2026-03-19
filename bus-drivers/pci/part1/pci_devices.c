/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#include <stdio.h>    /* For printf() */
#include <pci/pci.h>  /* Main libpci header - contains all PCI definitions */

int main() {
    struct pci_access *pacc;     /* Main libpci access structure */
    struct pci_dev *dev;         /* Device iterator */
    char namebuf[1024], *name;   /* Name lookup buffer */

    /*
     * Standard libpci initialization sequence
     * 
     * This three-step initialization is required for all libpci programs:
     * 1. pci_alloc() - Creates and initializes the main access structure
     * 2. pci_init() - Detects available PCI access methods and initializes them
     * 3. pci_scan_bus() - Scans all PCI buses and builds device list
     * 
     * The access structure (pacc) contains configuration, device list,
     * and method-specific data needed for PCI operations.
     */
    pacc = pci_alloc();          /* Allocate access structure */
    pci_init(pacc);              /* Initialize library */
    pci_scan_bus(pacc);          /* Discover devices */

    /*
     * Iterate through discovered devices
     * 
     * After pci_scan_bus(), all discovered devices are linked in a
     * simple linked list accessible via pacc->devices. Each device
     * contains basic addressing information but detailed data must
     * be requested explicitly using pci_fill_info().
     */
    for (dev = pacc->devices; dev; dev = dev->next) {
        
        /*
         * Request device identification information
         * 
         * pci_fill_info() populates the device structure with requested data.
         * Flags specify what information to retrieve:
         * - PCI_FILL_IDENT: Vendor ID, device ID, device class
         * - PCI_FILL_BASES: Base Address Registers (BARs)
         * - PCI_FILL_CLASS: Device class code and related information
         * 
         * This lazy loading approach improves performance by only reading
         * configuration space when needed.
         */
        pci_fill_info(dev, PCI_FILL_IDENT | PCI_FILL_BASES | PCI_FILL_CLASS);
        
        /*
         * Translate vendor/device IDs to human-readable names
         * 
         * pci_lookup_name() uses the PCI ID database to convert numeric
         * IDs to descriptive names. The lookup flags specify what to look up:
         * - PCI_LOOKUP_VENDOR: Look up vendor name
         * - PCI_LOOKUP_DEVICE: Look up device name
         * 
         * The function returns a pointer to the formatted name string,
         * which combines vendor and device names for easy identification.
         */
        name = pci_lookup_name(pacc, namebuf, sizeof(namebuf), 
                              PCI_LOOKUP_VENDOR | PCI_LOOKUP_DEVICE,
                              dev->vendor_id, dev->device_id);
        
        /*
         * Display in standard PCI addressing format
         * 
         * Standard PCI address format: domain:bus:device.function
         * - domain: PCI domain number (usually 0000 on single-domain systems)
         * - bus: PCI bus number (0-255)
         * - device: Device slot number on the bus (0-31)
         * - function: Function number within the device (0-7)
         * 
         * This addressing scheme uniquely identifies every PCI device
         * in the system and is used by tools like lspci.
         */
        printf("%04x:%02x:%02x.%d %s\n", 
               dev->domain, dev->bus, dev->dev, dev->func, name);
    }

    /*
     * Cleanup libpci resources
     * 
     * Always call pci_cleanup() to properly free all allocated memory,
     * close file descriptors, and clean up any method-specific resources.
     * This prevents memory leaks and ensures proper cleanup.
     */
    pci_cleanup(pacc);
    return 0;
}
