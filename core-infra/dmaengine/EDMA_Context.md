<!--
Copyright (c) 2024 TECH VEDA
Author: Raghu Bharadwaj
This code is dual-licensed under the MIT License and GPL v2
-->

# BeagleBone Black EDMA Overview
   
The BeagleBone Black utilizes the Texas Instruments AM335x processor, which includes the Enhanced DMA (EDMA) controller. This document provides a high-level overview of the EDMA features, capabilities, and configuration parameters specific to BeagleBone Black.

## Key Features

- **Channel Support**: EDMA includes 64 channels capable of handling multiple DMA requests simultaneously.
- **Transfer Types**: Supports both memory-to-memory, peripheral-to-memory, and memory-to-peripheral transfers.
- **Data Handling**: Handles 8-bit, 16-bit, and 32-bit data transfers.
- **Priority Levels**: Configurable priority levels for channels to manage resource allocation.
- **Event Mapping**: EDMA event mapping allows precise control over peripheral-to-memory transfer requests.
- **Interrupt Options**: Includes various interrupt options like transfer completion, error, and chaining interrupts.

## System Architecture

The EDMA in AM335x is integrated with the Programmable Real-Time Unit Subsystem (PRUSS) and provides high-performance data movement:

```
┌───────────────────────────────┐
│     AM335x SOC Architecture  │
├───────────────────────────────┤
│  [ ARM Cortex-A8 CPU ]       │
│  [ GPU ]                     │
│  [ PRUSS ]──[ EDMA ]─────────┤
│  [ L3/L4 Interconnect ]      │
│  [ Peripheral Interfaces ]   │
└───────────────────────────────┘
```

## Functional Description

- **Channel Synchronization**: EDMA channels support synchronized and unsynchronized transfer modes.
- **FIFO Handling**: Allows buffering to manage burst transfers efficiently.
- **PaRAM Set**: Configuration includes a PaRAM (Parameter RAM) set for each channel that controls source, destination, and transfer size.
- **Chaining and Linking**: Supports chaining and linking of channels for complex transfer sequences.

## Configuration Parameters

### Transfer Parameters

- **ACNT (A-Count)**: Describes the number of bytes in one dimension.
- **BCNT (B-Count)**: Represents the number of such ACNT arrays in one block.
- **CCNT (C-Count)**: Counts the number of blocks of BCNT arrays.

### Addressing Modes

- **Source & Destination Addressing**: Can be increment, decrement, or constant based on configuration.

## Device Tree Configuration

The EDMA controller is configured in the AM335x device tree as follows:

### Main EDMA Controller (TPCC - Transfer Parameter Control Channel)

```dts
target-module@49000000 {
    compatible = "ti,sysc-omap4", "ti,sysc";
    reg = <0x49000000 0x4>;
    reg-names = "rev";
    clocks = <&l3_clkctrl AM3_L3_TPCC_CLKCTRL 0>;
    clock-names = "fck";
    #address-cells = <1>;
    #size-cells = <1>;
    ranges = <0x0 0x49000000 0x10000>;

    edma: dma@0 {
        compatible = "ti,edma3-tpcc";
        reg = <0 0x10000>;
        reg-names = "edma3_cc";
        interrupts = <12 13 14>;
        interrupt-names = "edma3_ccint", "edma3_mperr",
                         "edma3_ccerrint";
        dma-requests = <64>;
        #dma-cells = <2>;

        ti,tptcs = <&edma_tptc0 7>, <&edma_tptc1 5>,
                   <&edma_tptc2 0>;

        ti,edma-memcpy-channels = <20 21>;
    };
};
```

### Transfer Parameter Controllers (TPTC)

```dts
/* TPTC0 */
target-module@49800000 {
    compatible = "ti,sysc-omap4", "ti,sysc";
    reg = <0x49800000 0x4>, <0x49800010 0x4>;
    reg-names = "rev", "sysc";
    clocks = <&l3_clkctrl AM3_L3_TPTC0_CLKCTRL 0>;
    clock-names = "fck";
    ranges = <0x0 0x49800000 0x100000>;

    edma_tptc0: dma@0 {
        compatible = "ti,edma3-tptc";
        reg = <0 0x100000>;
        interrupts = <112>;
        interrupt-names = "edma3_tcerrint";
    };
};

/* TPTC1 */
target-module@49900000 {
    compatible = "ti,sysc-omap4", "ti,sysc";
    reg = <0x49900000 0x4>, <0x49900010 0x4>;
    reg-names = "rev", "sysc";
    clocks = <&l3_clkctrl AM3_L3_TPTC1_CLKCTRL 0>;
    clock-names = "fck";
    ranges = <0x0 0x49900000 0x100000>;

    edma_tptc1: dma@0 {
        compatible = "ti,edma3-tptc";
        reg = <0 0x100000>;
        interrupts = <113>;
        interrupt-names = "edma3_tcerrint";
    };
};

/* TPTC2 */
target-module@49a00000 {
    compatible = "ti,sysc-omap4", "ti,sysc";
    reg = <0x49a00000 0x4>, <0x49a00010 0x4>;
    reg-names = "rev", "sysc";
    clocks = <&l3_clkctrl AM3_L3_TPTC2_CLKCTRL 0>;
    clock-names = "fck";
    ranges = <0x0 0x49a00000 0x100000>;

    edma_tptc2: dma@0 {
        compatible = "ti,edma3-tptc";
        reg = <0 0x100000>;
        interrupts = <114>;
        interrupt-names = "edma3_tcerrint";
    };
};
```

### Device Tree Properties Explained

- **compatible**: Identifies the EDMA3 controller type ("ti,edma3-tpcc" for main controller, "ti,edma3-tptc" for transfer controllers)
- **reg**: Memory-mapped register regions for the controller
- **interrupts**: IRQ numbers for completion, error, and memory protection interrupts
- **dma-requests**: Total number of DMA request lines (64 channels)
- **#dma-cells**: Number of cells required to specify a DMA channel (2 for EDMA)
- **ti,tptcs**: References to Transfer Parameter Controllers with priorities (7, 5, 0)
- **ti,edma-memcpy-channels**: Dedicated channels for memory-to-memory transfers (channels 20, 21)

### Memory Map

```
EDMA Controller Memory Layout:
┌─────────────────────────────────────┐
│ 0x49000000 - EDMA3CC (64KB)        │  Main Channel Controller
├─────────────────────────────────────┤
│ 0x49800000 - EDMA3TC0 (1MB)        │  Transfer Controller 0
├─────────────────────────────────────┤
│ 0x49900000 - EDMA3TC1 (1MB)        │  Transfer Controller 1
├─────────────────────────────────────┤
│ 0x49a00000 - EDMA3TC2 (1MB)        │  Transfer Controller 2
└─────────────────────────────────────┘
```

## DMA Engine Subsystem Integration

The DMA Engine API in the Linux kernel abstracts hardware-specific implementations of DMA controllers:

- **Channel Request**: Allocate channels for specific transfer types (memcpy, slave, etc.).
- **Descriptor Preparation**: Prepare transactions with direction, size, and interrupt options.
- **Asynchronous Execution**: Issue pending transactions and wait for completion callbacks.

## Considerations for BeagleBone Black

- **Compatibility**: Ensure kernel configuration supports EDMA and DMA engine framework.
- **Performance**: Proper channel prioritization and memory alignment can significantly improve performance.
- **Debugging**: Utilize kernel logs (`dmesg`) to troubleshoot DMA transfer issues.

## References

- [AM335x Technical Reference Manual](https://www.ti.com/lit/ug/spruh73q/spruh73q.pdf)
- [BeagleBone Black System Reference Manual](https://github.com/beagleboard/beaglebone-black)
- [Linux Kernel Documentation: DMA Engine](https://www.kernel.org/doc/Documentation/dmaengine/dmaengine.txt)

## Conclusion

The EDMA controller on the BeagleBone Black is a powerful component for managing data transfers efficiently. By leveraging the DMA Engine framework, developers can abstract the complexity of DMA interactions, allowing focus on application-specific logic. Proper understanding and configuration of EDMA can lead to highly optimized data handling in embedded systems.
