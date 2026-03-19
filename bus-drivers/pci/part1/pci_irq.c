/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pci/pci.h>

/* Structure to hold interrupt analysis results */
struct interrupt_info {
    char device_addr[32];         /* PCI address string */
    char device_name[256];        /* Human-readable device name */
    uint8_t int_line;            /* Interrupt Line register (0x3C) */
    uint8_t int_pin;             /* Interrupt Pin register (0x3D) */
    int active_interrupts;        /* Number of active system interrupts */
    char interrupt_lines[5][64];  /* Store up to 5 interrupt descriptions */
    int total_interrupts;         /* Total interrupt count for this device */
};

/*
 * ========================================================================
 * CORE PCI INTERRUPT ANALYSIS FUNCTIONS
 * ========================================================================
 * Function to analyze interrupt configuration for a single device
 * 
 * This function reads the PCI interrupt configuration registers and
 * correlates them with system interrupt assignments to provide a
 * comprehensive view of how the device handles interrupts.
 */
struct interrupt_info analyze_device_interrupts(struct pci_dev *dev)
{
    struct interrupt_info info = {0};
    
    /*
     * Read PCI interrupt configuration registers
     * 
     * These are 8-bit registers in the PCI configuration space:
     * - Interrupt Line (0x3C): IRQ number assigned by system
     * - Interrupt Pin (0x3D): Which INTx pin the device uses
     * 
     * Use pci_read_byte() for single-byte register access.
     */
    info.int_line = pci_read_byte(dev, PCI_INTERRUPT_LINE);  /* Offset 0x3C */
    info.int_pin = pci_read_byte(dev, PCI_INTERRUPT_PIN);    /* Offset 0x3D */
    
    /*
     * Format device address for identification
     * 
     * Create a standard PCI address string for easy device identification
     * and correlation with system interrupt tables.
     */
    snprintf(info.device_addr, sizeof(info.device_addr), "%04x:%02x:%02x.%d", 
             dev->domain, dev->bus, dev->dev, dev->func);
    
    /*
     * Get human-readable device name
     * 
     * Translate vendor and device IDs to descriptive names using
     * the PCI ID database for better user understanding.
     */
    pci_lookup_name(dev->access, info.device_name, sizeof(info.device_name), 
                    PCI_LOOKUP_VENDOR | PCI_LOOKUP_DEVICE,
                    dev->vendor_id, dev->device_id);
    
    /*
     * Correlate with system interrupt assignments
     * 
     * Read /proc/interrupts to see how the kernel has actually assigned
     * interrupts to this device. This shows the difference between PCI
     * configuration and actual system interrupt usage.
     * 
     * /proc/interrupts format:
     * IRQ: CPU0 CPU1 ... device_name
     * 
     * Modern systems often show MSI/MSI-X interrupts instead of
     * the legacy IRQ numbers from PCI configuration.
     */
    FILE *interrupts = fopen("/proc/interrupts", "r");
    if (interrupts) {
        char line[1024];
        info.active_interrupts = 0;
        info.total_interrupts = 0;
        
        /*
         * Search for lines containing our device address
         * 
         * The kernel includes the PCI address in interrupt descriptions,
         * allowing us to correlate configuration with actual usage.
         */
        while (fgets(line, sizeof(line), interrupts)) {
            if (strstr(line, info.device_addr)) {
                if (info.active_interrupts < 3) {
                    /*
                     * Extract and format interrupt information
                     * 
                     * Parse the interrupt number and type to show
                     * how the device actually uses interrupts.
                     */
                    char *int_num = strtok(line, ":");
                    char *rest = strtok(NULL, "\n");
                    if (int_num && rest) {
                        char *type_start = strstr(rest, "IR-PCI-");
                        if (type_start) {
                            snprintf(info.interrupt_lines[info.active_interrupts], 
                                   sizeof(info.interrupt_lines[0]), 
                                   "IRQ %s: %s", int_num, type_start);
                            info.active_interrupts++;
                        }
                    }
                }
                info.total_interrupts++;
            }
        }
        fclose(interrupts);
    }
    
    return info;
}

/*
 * ========================================================================
 * INTERRUPT INFORMATION DISPLAY FUNCTIONS
 * ========================================================================
 */

void show_header(const struct interrupt_info *info)
{
    printf("\n═════════════════════════════════════════════════════════════════\n");
    printf("║ DEVICE: %-55s ║\n", info->device_addr);
    printf("║ %-62s ║\n", info->device_name);
    printf("╠════════════════════════════════════════════════════════════════╣\n");
}

void show_config(const struct interrupt_info *info)
{
    printf("│ PCI CONFIGURATION SPACE:                                      │\n");
    
    /*
     * Display and interpret Interrupt Line register
     * 
     * The Interrupt Line register contains the system-assigned IRQ number.
     * Different values have specific meanings in modern systems.
     */
    printf("│   Interrupt Line: %3d ", info->int_line);
    if (info->int_line == 0xFF) {
        printf("(No IRQ assigned)                         │\n");
        printf("│     → Modern devices use MSI/MSI-X instead              │\n");
    } else if (info->int_line == 0) {
        printf("(IRQ 0 - System Timer)                     │\n");
        printf("│     → Unusual assignment, may indicate conflict         │\n");
    } else {
        printf("(IRQ %d)                                    │\n", info->int_line);
        if (info->int_line < 16) {
            printf("│     → Legacy ISA IRQ range, shared interrupts likely   │\n");
        } else {
            printf("│     → Extended IRQ range, APIC-managed               │\n");
        }
    }
    
    /*
     * Display and interpret Interrupt Pin register
     * 
     * The Interrupt Pin register indicates which of the four legacy
     * interrupt pins (INTA# through INTD#) the device uses.
     */
    printf("│   Interrupt Pin:  %3d ", info->int_pin);
    switch (info->int_pin) {
        case 0: 
            printf("(No interrupt pin)                       │\n");
            printf("│     → Device doesn't use legacy interrupts           │\n");
            break;
        case 1: printf("(INTA# - Primary)                        │\n"); break;
        case 2: printf("(INTB# - Secondary)                      │\n"); break;
        case 3: printf("(INTC# - Tertiary)                       │\n"); break;
        case 4: printf("(INTD# - Quaternary)                     │\n"); break;
        default: 
            printf("(Invalid pin)                            │\n");
            printf("│     → Configuration error or corrupted data         │\n");
            break;
    }
}

void show_interrupts(const struct interrupt_info *info)
{
    printf("│                                                              │\n");
    printf("│ SYSTEM INTERRUPTS (/proc/interrupts):                     │\n");
    
    if (info->active_interrupts == 0) {
        printf("│   No active interrupts found                              │\n");
        printf("│   (Device inactive or uses different mechanism)           │\n");
    } else {
        for (int i = 0; i < info->active_interrupts; i++) {
            printf("│   %-58s │\n", info->interrupt_lines[i]);
        }
        if (info->total_interrupts > 3) {
            printf("│   ... and %d more interrupts                             │\n", 
                   info->total_interrupts - 3);
        }
    }
}

void show_analysis(const struct interrupt_info *info)
{
    printf("│                                                              │\n");
    printf("│ ANALYSIS:                                                  │\n");
    
    if (info->int_line == 0xFF && info->int_pin != 0) {
        printf("│   • Config shows interrupt support but no IRQ assigned      │\n");
        printf("│   • Device likely uses MSI/MSI-X for modern performance   │\n");
    } else if (info->int_line != 0xFF && info->int_pin == 0) {
        printf("│   • Unusual: IRQ assigned but no interrupt pin            │\n");
        printf("│   • May indicate configuration issue                      │\n");
    } else if (info->int_line != 0xFF && info->int_pin != 0) {
        printf("│   • Traditional legacy interrupt configuration             │\n");
        printf("│   • Uses shared IRQ line with potential conflicts        │\n");
    } else {
        printf("│   • Device appears to not use interrupts at all           │\n");
        printf("│   • May use polling or be a passive device               │\n");
    }
}

void show_footer()
{
    printf("╰────────────────────────────────────────────────────────────────╯\n");
}

/*
 * Main function to analyze and display interrupt information
 * 
 * This function demonstrates the clean separation of logic and presentation.
 * It first analyzes the device's interrupt configuration, then formats
 * and displays the results in a user-friendly manner.
 */
void print_irq_info(struct pci_dev *dev)
{
    /*
     * Analyze interrupt configuration
     * 
     * Separate the core PCI analysis logic from the display formatting
     * to make the code more maintainable and educational.
     */
    struct interrupt_info info = analyze_device_interrupts(dev);
    
    /*
     * Display comprehensive interrupt analysis
     * 
     * Use separate formatting functions to present the information
     * in a structured, easy-to-understand format.
     */
    show_header(&info);
    show_config(&info);
    show_interrupts(&info);
    show_analysis(&info);
    show_footer();
}

int main()
{
    struct pci_access *pacc;
    struct pci_dev *dev;
    int device_count = 0;

    printf("Simple PCI Interrupt Information\n");
    printf("================================\n\n");

    /* Standard libpci initialization */
    pacc = pci_alloc();
    pci_init(pacc);
    pci_scan_bus(pacc);

    for (dev = pacc->devices; dev; dev = dev->next) {
        pci_fill_info(dev, PCI_FILL_IDENT | PCI_FILL_CLASS);
        
        /* Filter out bridge devices */
        if ((dev->device_class & 0xFF00) == 0x0600) {
            continue;
        }
        
        print_irq_info(dev);
        device_count++;
        
        if (device_count >= 10) {
            printf("... (showing first 10 devices only)\n");
            break;
        }
    }

    if (device_count == 0) {
        printf("No suitable PCI devices found\n");
    }

    printf("\n=== SYSTEM INTERRUPT ANALYSIS ===\n");
    
    /* Check if running as root to access more system information */
    if (geteuid() == 0) {
        printf("Running with root privileges - checking actual system interrupts...\n\n");
        
        /* Show current interrupt assignments from /proc/interrupts */
        FILE *interrupts = fopen("/proc/interrupts", "r");
        if (interrupts) {
            char line[1024];
            int msi_count = 0, legacy_count = 0;
            
            printf("Active MSI/MSI-X interrupts in system:\n");
            while (fgets(line, sizeof(line), interrupts)) {
                if (strstr(line, "PCI-MSI") || strstr(line, "PCI-MSIX")) {
                    printf("  %s", line);
                    msi_count++;
                }
            }
            
            rewind(interrupts);
            printf("\nActive legacy IO-APIC interrupts in system:\n");
            while (fgets(line, sizeof(line), interrupts)) {
                if (strstr(line, "IO-APIC") && !strstr(line, "CPU")) {
                    printf("  %s", line);
                    legacy_count++;
                }
            }
            
            fclose(interrupts);
            printf("\nInterrupt Statistics:\n");
            printf("  - MSI/MSI-X interrupts found: %d\n", msi_count);
            printf("  - Legacy IO-APIC interrupts: %d\n", legacy_count);
            printf("  - This explains why most devices show interrupt line 255!\n");
        } else {
            printf("Could not read /proc/interrupts\n");
        }
    } else {
        printf("Run with 'sudo' to see actual system interrupt assignments\n");
        printf("This will show MSI/MSI-X usage vs legacy IRQ lines\n");
    }

    pci_cleanup(pacc);
    return 0;
}
