/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#include <stdio.h>    
            switch (bar & PCI_BASE_ADDRESS_MEM_TYPE_MASK) {
                case PCI_BASE_ADDRESS_MEM_TYPE_32:
                    printf("32-bit address");
                    break;
                case PCI_BASE_ADDRESS_MEM_TYPE_1M:
                    printf("Below 1MB");
                    break;
                case PCI_BASE_ADDRESS_MEM_TYPE_64:
                    printf("64-bit address");
                    break;
                default:
                    printf("Unknown type");
                    break;
            }

            /*
             * Check prefetchable flag (bit 3)
             * 
             * Prefetchable memory regions can be safely cached and prefetched
             * by the CPU and system bridges. This is typically used for:
             * - Frame buffers and video memory
             * - Large data buffers
             * - Read-only device memory
             * 
             * Non-prefetchable regions contain control registers that may
             * have side effects when read, so they cannot be cached.
             */
            if (bar & PCI_BASE_ADDRESS_MEM_PREFETCH) {
                printf(", prefetchable\n");
            } else {
                printf(", non-prefetchable\n");
            }

            /*
             * Apply the same size probing technique for memory BARs
             * 
             * The process is identical to I/O BARs, but we use the
             * memory address mask instead of the I/O address mask.
             */
            pci_write_long(dev, PCI_BASE_ADDRESS_0 + 4 * i, 0xFFFFFFFF);
            bar_mask = pci_read_long(dev, PCI_BASE_ADDRESS_0 + 4 * i) & PCI_BASE_ADDRESS_MEM_MASK;
            pci_write_long(dev, PCI_BASE_ADDRESS_0 + 4 * i, bar);

            uint32_t size = (~bar_mask + 1);
            
            /* Display size in human-readable format */
            if (size >= 1024 * 1024) {
                printf("BAR%d size: 0x%08x (%u MB)\n", i, size, size / (1024 * 1024));
            } else if (size >= 1024) {
                printf("BAR%d size: 0x%08x (%u KB)\n", i, size, size / 1024);
            } else {
                printf("BAR%d size: 0x%08x (%u bytes)\n", i, size, size);
            }
        }
    }
}

int main(void) {
    struct pci_access *pacc;
    struct pci_dev *dev;

    /*
     * Standard libpci initialization sequence
     * 
     * Same three-step initialization pattern used in all libpci programs.
     */
    pacc = pci_alloc();
    pci_init(pacc);
    pci_scan_bus(pacc);

    /*
     * Iterate through all discovered devices
     * 
     * For each device, we'll analyze its BARs to understand
     * how it maps into the system's address spaces.
     */
    for (dev = pacc->devices; dev; dev = dev->next) {
        /*
         * Request comprehensive device information
         * 
         * We need:
         * - PCI_FILL_IDENT: Vendor/device IDs for identification
         * - PCI_FILL_BASES: BAR values for address analysis
         * - PCI_FILL_CLASS: Device class for bridge detection
         */
        pci_fill_info(dev, PCI_FILL_IDENT | PCI_FILL_BASES | PCI_FILL_CLASS);

        /*
         * Display device identification
         * 
         * Show the standard PCI address and device name for context.
         */
        printf("PCI Device: %04x:%02x:%02x.%d\n",
               dev->domain, dev->bus, dev->dev, dev->func);
        printf("Device: %s\n", pci_lookup_name(pacc, (char[1024]){}, sizeof(pacc), 
                                             PCI_LOOKUP_DEVICE, dev->vendor_id, dev->device_id));

        /*
         * Differentiate bridges from endpoint devices
         * 
         * PCI bridges (class 0x06xx) have different BAR usage:
         * - BAR0-1: Bridge control registers
         * - No standard device BARs
         * 
         * Endpoint devices (all other classes) use BARs for:
         * - Device registers and control
         * - Memory buffers
         * - I/O port ranges
         */
        if ((dev->device_class & 0xFF00) == 0x0600) {
            printf("Device is a bridge\n");
        } else {
            handle_device(dev);
        }

        printf("\n\n");
    }

    /*
     * Cleanup libpci resources
     */
    pci_cleanup(pacc);
    return 0;
}

