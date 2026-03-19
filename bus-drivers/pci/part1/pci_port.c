/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/io.h>
#include <pci/pci.h>


    pci_write_long(dev, PCI_BASE_ADDRESS_0, 0xFFFFFFFF);
    uint32_t bar_mask = pci_read_long(dev, PCI_BASE_ADDRESS_0) & PCI_BASE_ADDRESS_IO_MASK;
    pci_write_long(dev, PCI_BASE_ADDRESS_0, bar0_addr | PCI_BASE_ADDRESS_SPACE_IO);
    bar0_size = (~bar_mask + 1) & 0xFFFF;

    
    if (iopl(3) != 0) {
        perror("Cannot get I/O port permissions (need root)");
        pci_cleanup(pacc);
        return 1;
    }

    
    pci_cleanup(pacc);

    printf("\nPort I/O read complete!\n");
    return 0;
}
