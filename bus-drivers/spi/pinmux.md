# BeagleBone Black (AM335x) Pinmuxing Guide for SPI0

## Table of Contents
1. [Overview](#overview)
2. [What is Pinmuxing?](#what-is-pinmuxing)
3. [AM335x Pin Multiplexing Architecture](#am335x-pin-multiplexing-architecture)
4. [Why We Need Pinmux for SPI0](#why-we-need-pinmux-for-spi0)
5. [Understanding the Pinmux Configuration](#understanding-the-pinmux-configuration)
6. [Device Tree Implementation](#device-tree-implementation)
7. [How It Works: The Complete Flow](#how-it-works-the-complete-flow)
8. [BeagleBone Black SPI0 Pin Mapping](#beaglebone-black-spi0-pin-mapping)
9. [Troubleshooting](#troubleshooting)
10. [References](#references)

---

## Overview

This document explains the pin multiplexing (pinmux) configuration required for the SPI0 controller on the BeagleBone Black. During our W25Q32 SPI flash driver integration, we discovered that simply enabling the SPI0 controller in the device tree was insufficient—we also needed to configure the physical pins to route SPI0 signals from the AM335x SoC to the expansion headers.

---

## What is Pinmuxing?

**Pin Multiplexing** (pinmux) is a hardware feature that allows a single physical pin on a microprocessor to serve multiple functions. This is necessary because:

- Modern SoCs have more peripheral functions than physical pins available
- Different applications need different peripheral configurations
- It provides flexibility for board designers to route signals as needed
- It reduces package size and cost

For example, a single pin on the AM335x might be capable of functioning as:
- GPIO (General Purpose I/O)
- SPI data line
- I2C signal
- UART signal
- PWM output
- And more...

The pinmux controller determines which function is active for each pin at any given time.

---

## AM335x Pin Multiplexing Architecture

### Hardware Components

The AM335x SoC contains:

1. **Control Module (SCM)**: System Control Module that manages pin configuration
2. **Pinmux Registers**: Memory-mapped registers (starting at `0x44E10800`) that configure each pin
3. **Pin Configuration Register**: Each pin has a 32-bit configuration register

### Pin Configuration Register Layout

Each pin's configuration is controlled by a 32-bit register with the following fields:

```
Bit 31-7: Reserved
Bit 6:    SLEW      (Slew rate: 0=Fast, 1=Slow)
Bit 5:    RXACTIVE  (Input enable: 1=Input enabled, 0=Input disabled)
Bit 4:    PULLUP    (Pull type: 1=Pullup, 0=Pulldown)
Bit 3:    PULLEN    (Pull enable: 1=Disabled, 0=Enabled)
Bit 2-0:  MUXMODE   (Function select: 0-7, selects one of 8 possible functions)
```

### Pinmux Controller in Device Tree

The AM335x pinmux controller is defined in `am33xx-l4.dtsi`:

```dts
am33xx_pinmux: pinmux@800 {
    compatible = "pinctrl-single";
    reg = <0x800 0x238>;              /* Offset 0x800, size 0x238 bytes */
    #pinctrl-cells = <2>;              /* Two cells: offset + config */
    pinctrl-single,register-width = <32>;  /* 32-bit registers */
    pinctrl-single,function-mask = <0x7f>; /* Bits 0-6 are configurable */
};
```

Key properties:
- **`compatible = "pinctrl-single"`**: Uses the `pinctrl-single` driver
- **`reg = <0x800 0x238>`**: Register space at offset 0x800 (physical address 0x44E10800)
- **`#pinctrl-cells = <2>`**: Each pin configuration requires 2 values (pin offset and config value)
- **`function-mask = <0x7f>`**: Bits 0-6 can be modified (mux mode + pull/input settings)

---

## Why We Need Pinmux for SPI0

### The Problem We Encountered

When we initially added the SPI0 node to enable the SPI controller:

```dts
&spi0 {
    status = "okay";
    /* ... */
};
```

The SPI controller driver loaded, but:
- **No communication occurred with the W25Q32 flash**
- **The probe function didn't detect the device**
- **No errors appeared in dmesg**

### Root Cause

The SPI0 controller was enabled in software, but the physical pins were not configured to route SPI0 signals. The pins were likely still in their default GPIO mode or configured for a different peripheral.

### The Solution

We needed to:
1. Create a pinmux configuration group for SPI0 pins
2. Configure each pin with the correct mode and electrical properties
3. Link the SPI0 controller to this pinmux configuration

After adding the pinmux configuration, the driver successfully probed and communicated with the W25Q32 flash device.

---

## Understanding the Pinmux Configuration

### Our SPI0 Pinmux Configuration

```dts
&am33xx_pinmux {
    spi0_pins: spi0-pins {
        pinctrl-single,pins = <
            AM33XX_PADCONF(AM335X_PIN_SPI0_SCLK, PIN_INPUT_PULLUP, MUX_MODE0)
            AM33XX_PADCONF(AM335X_PIN_SPI0_D0,   PIN_INPUT_PULLUP, MUX_MODE0)
            AM33XX_PADCONF(AM335X_PIN_SPI0_D1,   PIN_INPUT_PULLUP, MUX_MODE0)
            AM33XX_PADCONF(AM335X_PIN_SPI0_CS0,  PIN_INPUT_PULLUP, MUX_MODE0)
        >;
    };
};
```

### Breaking Down Each Line

#### 1. The Container
```dts
&am33xx_pinmux {
    spi0_pins: spi0-pins {
```
- References the AM335x pinmux controller
- Creates a named pinmux group called `spi0_pins`
- The label `spi0-pins` can be used for debugging

#### 2. The Pin List
```dts
pinctrl-single,pins = < ... >;
```
- Property understood by the `pinctrl-single` driver
- Contains a list of pin configurations

#### 3. Individual Pin Configuration
```dts
AM33XX_PADCONF(AM335X_PIN_SPI0_SCLK, PIN_INPUT_PULLUP, MUX_MODE0)
```

This macro expands to configure one pin. Let's understand each parameter:

**Parameter 1: Pin Address (`AM335X_PIN_SPI0_SCLK`)**
- Defined in `include/dt-bindings/pinctrl/am33xx.h`:
  ```c
  #define AM335X_PIN_SPI0_SCLK  0x950
  ```
- This is the offset from the base address (0x44E10800)
- Full address: 0x44E10800 + 0x950 = 0x44E10950

**Parameter 2: Pin Configuration (`PIN_INPUT_PULLUP`)**
- Defined in `include/dt-bindings/pinctrl/omap.h`:
  ```c
  #define INPUT_EN      (1 << 8)  /* Bit 5 in actual register */
  #define PULL_ENA      (1 << 3)  /* Bit 3 */
  #define PULL_UP       (1 << 4)  /* Bit 4 */
  
  #define PIN_INPUT_PULLUP  (PULL_ENA | INPUT_EN | PULL_UP)
  ```
- This sets:
  - **Input enable** (RXACTIVE = 1): Pin can receive signals
  - **Pull resistor enabled** (PULLEN = 0): Internal pull resistor active
  - **Pull direction = up** (PULLUP = 1): Pullup resistor (not pulldown)

**Parameter 3: Mux Mode (`MUX_MODE0`)**
- Defined in `include/dt-bindings/pinctrl/omap.h`:
  ```c
  #define MUX_MODE0  0
  ```
- Mode 0 selects the primary function for this pin
- For `AM335X_PIN_SPI0_SCLK`, Mode 0 = SPI0_SCLK function
- Other modes (1-7) would select different functions (GPIO, etc.)

### The AM33XX_PADCONF Macro

Defined in `include/dt-bindings/pinctrl/omap.h`:

```c
#define AM33XX_IOPAD(pa, val) \
    OMAP_IOPAD_OFFSET((pa), 0x0800) (val) (0)

#define AM33XX_PADCONF(pa, conf, mux) \
    OMAP_IOPAD_OFFSET((pa), 0x0800) (conf) (mux)

#define OMAP_IOPAD_OFFSET(pa, offset) \
    (((pa) & 0xffff) - (offset))
```

This macro:
1. Takes the physical address (e.g., 0x950)
2. Subtracts the base offset (0x800)
3. Produces the register offset: 0x950 - 0x800 = 0x150
4. Outputs: `offset (config_bits) (mux_mode)`

### Final Register Value Calculation

For `AM33XX_PADCONF(AM335X_PIN_SPI0_SCLK, PIN_INPUT_PULLUP, MUX_MODE0)`:

```
Register offset: 0x150 (from macro calculation)
Configuration: PIN_INPUT_PULLUP = INPUT_EN | PULL_ENA | PULL_UP
            = (1 << 8) | (1 << 3) | (1 << 4)
            = 0x138 (but mapped to actual register bits)
Mux mode: 0

Actual register value written:
Bit 5 (INPUT_EN):  1  - Input enabled
Bit 4 (PULLUP):    1  - Pullup selected
Bit 3 (PULLEN):    0  - Pull resistor enabled (note: 0 = enabled)
Bit 2-0 (MUXMODE): 0  - Mode 0 (SPI0_SCLK function)

Binary: 0b00110000 = 0x30
```

---

## Device Tree Implementation

### Step 1: Define Pinmux Group

In `am335x-boneblack.dts`:

```dts
&am33xx_pinmux {
    spi0_pins: spi0-pins {
        pinctrl-single,pins = <
            AM33XX_PADCONF(AM335X_PIN_SPI0_SCLK, PIN_INPUT_PULLUP, MUX_MODE0)
            AM33XX_PADCONF(AM335X_PIN_SPI0_D0,   PIN_INPUT_PULLUP, MUX_MODE0)
            AM33XX_PADCONF(AM335X_PIN_SPI0_D1,   PIN_INPUT_PULLUP, MUX_MODE0)
            AM33XX_PADCONF(AM335X_PIN_SPI0_CS0,  PIN_INPUT_PULLUP, MUX_MODE0)
        >;
    };
};
```

### Step 2: Link to SPI0 Controller

```dts
&spi0 {
    status = "okay";
    pinctrl-names = "default";
    pinctrl-0 = <&spi0_pins>;
    #address-cells = <1>;
    #size-cells = <0>;

    w25q32@0 {
        compatible = "techveda,w25q32";
        reg = <0>;
        spi-max-frequency = <100000>;
        /* ... */
    };
};
```

Key properties:
- **`pinctrl-names = "default"`**: Defines state name (typically "default")
- **`pinctrl-0 = <&spi0_pins>`**: Links to the pinmux group we defined
- The `0` in `pinctrl-0` corresponds to the first (and only) state in `pinctrl-names`

### Why Use References?

Using `<&spi0_pins>` creates a **phandle reference**:
- The pinctrl framework resolves this reference at boot time
- It finds the `spi0_pins` node in the `am33xx_pinmux` controller
- It applies the pin configurations before the SPI driver probes

---

## How It Works: The Complete Flow

### Boot Sequence

1. **U-Boot Stage**
   - Loads device tree blob (DTB) from boot partition
   - May apply some early pin configurations

2. **Linux Kernel Early Boot**
   - Device tree is parsed by the kernel
   - Pinctrl subsystem initializes

3. **Pinctrl Framework Initialization**
   ```
   pinctrl-single driver probes
   └─> Reads am33xx_pinmux node
       └─> Maps register space at 0x44E10800
           └─> Registers as a pinctrl provider
   ```

4. **SPI Controller Probe**
   ```
   spi-omap2-mcspi driver probes spi0
   └─> Checks for pinctrl properties
       └─> Finds pinctrl-0 = <&spi0_pins>
           └─> Calls pinctrl framework
               └─> Requests "default" state
                   └─> pinctrl-single driver applies configuration
                       ├─> Writes 0x30 to register at offset 0x150 (SPI0_SCLK)
                       ├─> Writes 0x30 to register at offset 0x154 (SPI0_D0)
                       ├─> Writes 0x30 to register at offset 0x158 (SPI0_D1)
                       └─> Writes 0x30 to register at offset 0x15C (SPI0_CS0)
   ```

5. **Driver Continues Probing**
   - Now that pins are configured, SPI signals are routed correctly
   - SPI controller can communicate with devices on the bus
   - W25Q32 driver probe succeeds

### Visual Representation

```
┌─────────────────────────────────────────────────────────────┐
│ AM335x SoC                                                   │
│                                                              │
│  ┌───────────────┐         ┌──────────────┐                │
│  │ SPI0 Controller│         │Control Module│                │
│  │  (McSPI)      │         │   (Pinmux)   │                │
│  └───────┬───────┘         └──────┬───────┘                │
│          │                        │                         │
│          │ Signals               │ Configuration           │
│          │ (SCLK, MOSI,          │ Registers              │
│          │  MISO, CS)            │ (0x44E10800)           │
│          │                        │                         │
│          v                        v                         │
│  ┌─────────────────────────────────────────┐                │
│  │      Configurable Pin Matrix            │                │
│  │  ┌────┐  ┌────┐  ┌────┐  ┌────┐        │                │
│  │  │Pin │  │Pin │  │Pin │  │Pin │        │                │
│  │  │P9_22  │P9_21  │P9_18  │P9_17        │                │
│  │  └─┬──┘  └─┬──┘  └─┬──┘  └─┬──┘        │                │
│  └────┼──────┼──────┼──────┼─────────────┘                │
└───────┼──────┼──────┼──────┼───────────────────────────────┘
        │      │      │      │
        │      │      │      │ Physical Pins
        v      v      v      v
   ┌────────────────────────────┐
   │  BeagleBone Black Header  │
   │       P9 Expansion         │
   └────────────────────────────┘
               │
               │ External Connection
               v
        ┌──────────────┐
        │  W25Q32 Flash │
        │   (SPI Slave) │
        └──────────────┘
```

### Register Write Details

When the pinctrl framework applies our configuration, it writes to these registers:

| Pin Function | Register Address | Offset | Value Written | Meaning |
|-------------|-----------------|---------|---------------|---------|
| SPI0_SCLK   | 0x44E10950      | 0x150   | 0x30          | Input+Pullup, Mode 0 |
| SPI0_D0     | 0x44E10954      | 0x154   | 0x30          | Input+Pullup, Mode 0 |
| SPI0_D1     | 0x44E10958      | 0x158   | 0x30          | Input+Pullup, Mode 0 |
| SPI0_CS0    | 0x44E1095C      | 0x15C   | 0x30          | Input+Pullup, Mode 0 |

### Why INPUT_EN for Output Pins?

You might wonder: "Why use `PIN_INPUT_PULLUP` for pins like SCLK and MOSI that are outputs?"

The answer:
- **Bidirectional nature**: The pinmux configuration applies to the physical pin, not the logical direction
- **Master side**: Even "output" pins from the SPI master can act as inputs in certain conditions
- **Signal integrity**: Input enable allows the GPIO block to read back the driven value
- **Standard practice**: Most SPI pinmux configurations use bidirectional settings
- **Pull resistors**: Help prevent floating lines when no device is actively driving

---

## BeagleBone Black SPI0 Pin Mapping

### Physical Pin Locations

| Function  | AM335x Signal | Expansion Header | Pin Name | Physical Pin |
|-----------|---------------|------------------|----------|--------------|
| SCLK      | SPI0_SCLK     | P9 Header        | P9_22    | Pin 22       |
| MOSI      | SPI0_D0       | P9 Header        | P9_21    | Pin 21       |
| MISO      | SPI0_D1       | P9 Header        | P9_18    | Pin 18       |
| CS0       | SPI0_CS0      | P9 Header        | P9_17    | Pin 17       |

### Pin Mode Tables

Each pin on the AM335x can be configured for up to 8 different functions (Mode 0-7).

**P9_22 (SPI0_SCLK) - Register 0x950**
| Mode | Function      | Description |
|------|---------------|-------------|
| 0    | SPI0_SCLK     | SPI0 Clock |
| 1    | UART2_RXD     | UART2 Receive |
| 2    | I2C2_SDA      | I2C2 Data |
| 3    | EHRPWM0A      | PWM Output |
| 4    | pr1_uart0_cts | PRU UART |
| 5    | pr1_ecap0     | PRU Capture |
| 6    | Reserved      | - |
| 7    | gpio0_2       | GPIO |

**P9_21 (SPI0_D0) - Register 0x954**
| Mode | Function      | Description |
|------|---------------|-------------|
| 0    | SPI0_D0       | SPI0 MOSI |
| 1    | UART2_TXD     | UART2 Transmit |
| 2    | I2C2_SCL      | I2C2 Clock |
| 3    | EHRPWM0B      | PWM Output |
| 4    | pr1_uart0_rts | PRU UART |
| 5    | pr1_ecap1     | PRU Capture |
| 6    | Reserved      | - |
| 7    | gpio0_3       | GPIO |

**P9_18 (SPI0_D1) - Register 0x958**
| Mode | Function      | Description |
|------|---------------|-------------|
| 0    | SPI0_D1       | SPI0 MISO |
| 1    | UART1_TXD     | UART1 Transmit |
| 2    | I2C1_SCL      | I2C1 Clock |
| 3    | EHRPWM0_SYNCI | PWM Sync Input |
| 4    | pr1_uart0_txd | PRU UART |
| 5    | pr1_pru0_16   | PRU Signal |
| 6    | Reserved      | - |
| 7    | gpio0_4       | GPIO |

**P9_17 (SPI0_CS0) - Register 0x95C**
| Mode | Function      | Description |
|------|---------------|-------------|
| 0    | SPI0_CS0      | SPI0 Chip Select 0 |
| 1    | UART1_TXD     | UART1 Transmit |
| 2    | I2C1_SCL      | I2C1 Clock |
| 3    | EHRPWM0_SYNCO | PWM Sync Output |
| 4    | pr1_uart0_txd | PRU UART |
| 5    | pr1_pru0_16   | PRU Signal |
| 6    | Reserved      | - |
| 7    | gpio0_5       | GPIO |

### Wiring to W25Q32 Flash

```
BeagleBone Black P9     W25Q32 Flash Chip
─────────────────────   ─────────────────
P9_22 (SPI0_SCLK)  ───> Pin 6 (CLK)
P9_21 (SPI0_D0)    ───> Pin 5 (DI/MOSI)
P9_18 (SPI0_D1)    <─── Pin 2 (DO/MISO)
P9_17 (SPI0_CS0)   ───> Pin 1 (/CS)
P9_01 (GND)        ───> Pin 4 (GND)
P9_03 (VDD_3V3)    ───> Pin 8 (VCC)
                        Pin 3 (/WP)  ──┐
                        Pin 7 (/HOLD)──┴─> Tie to VCC
```

---

## Troubleshooting

### Common Issues and Solutions

#### 1. SPI Device Not Detected
**Symptom**: Driver loads but device probe fails, no communication

**Check**:
```bash
# Verify pinmux is applied
cat /sys/kernel/debug/pinctrl/44e10800.pinmux/pinmux-pins | grep spi0
```

**Expected output**:
```
pin 84 (PIN84): spi0 (GPIO UNCLAIMED) function pinmux_spi0_pins group spi0-pins
pin 85 (PIN85): spi0 (GPIO UNCLAIMED) function pinmux_spi0_pins group spi0-pins
pin 86 (PIN86): spi0 (GPIO UNCLAIMED) function pinmux_spi0_pins group spi0-pins
pin 87 (PIN87): spi0 (GPIO UNCLAIMED) function pinmux_spi0_pins group spi0-pins
```

**Solution**: Add pinmux configuration as shown in this guide

#### 2. Wrong Pin Configuration
**Symptom**: Intermittent communication, data corruption

**Check**:
```bash
# Read actual register values
devmem2 0x44E10950 # SPI0_SCLK
devmem2 0x44E10954 # SPI0_D0
devmem2 0x44E10958 # SPI0_D1
devmem2 0x44E1095C # SPI0_CS0
```

**Expected**: All should show `0x30` (or similar, depending on configuration)

**Solution**: Verify pin configuration flags (INPUT_EN, PULL_UP, etc.)

#### 3. Pin Conflict
**Symptom**: Pinmux configuration doesn't apply, dmesg shows errors

**Check dmesg**:
```bash
dmesg | grep pinctrl
dmesg | grep "already requested"
```

**Cause**: Another driver or device tree node claims the same pins

**Solution**: Check device tree for conflicting pin assignments, disable conflicting peripherals

#### 4. Device Tree Not Updated
**Symptom**: Changes don't take effect after rebuilding

**Solution**:
```bash
# Verify DTB timestamp
ls -l /boot/am335x-boneblack.dtb

# Force clean rebuild
cd buildroot-directory
make linux-dirclean
make

# Reflash image
sudo dd if=output/images/sdcard.img of=/dev/sdX bs=4M
```

#### 5. Electrical Issues
**Symptom**: Pinmux is correct but still no communication

**Check**:
- Verify voltage levels (3.3V for both BBB and W25Q32)
- Check pull resistors: Some devices need external pulls
- Measure signal integrity with oscilloscope
- Verify physical connections

---

## References

### Documentation Files
- Kernel: `Documentation/devicetree/bindings/pinctrl/pinctrl-single.txt`
- AM335x Technical Reference Manual (TRM): Chapter 9 - System Control Module
- BeagleBone Black System Reference Manual

### Header Files
- `include/dt-bindings/pinctrl/am33xx.h` - AM335x pin definitions
- `include/dt-bindings/pinctrl/omap.h` - OMAP/AM33xx pinctrl macros
- `arch/arm/boot/dts/ti/omap/am33xx-l4.dtsi` - AM33xx pinmux controller definition

### Device Tree Files
- `arch/arm/boot/dts/ti/omap/am335x-boneblack.dts` - BeagleBone Black board file
- `arch/arm/boot/dts/ti/omap/am33xx.dtsi` - AM335x SoC base definition
- `arch/arm/boot/dts/ti/omap/am33xx-l4.dtsi` - AM335x L4 interconnect and peripherals

### Kernel Drivers
- `drivers/pinctrl/pinctrl-single.c` - Pinctrl-single driver implementation
- `drivers/spi/spi-omap2-mcspi.c` - AM335x SPI controller driver (McSPI)

### Online Resources
- BeagleBone Black Pinout: https://beagleboard.org/Support/bone101
- AM335x Sitara Processors: https://www.ti.com/product/AM3358
- Linux Pinctrl Subsystem: https://www.kernel.org/doc/html/latest/driver-api/pinctl.html

---

## Summary

Pin multiplexing is essential for enabling SPI0 communication on the BeagleBone Black because:

1. **Hardware Flexibility**: The AM335x pins can serve multiple functions; pinmux selects which function is active
2. **Required for Signal Routing**: Even though the SPI0 controller is enabled, signals won't reach the physical pins without proper pinmux configuration
3. **Electrical Configuration**: Pinmux also configures pull resistors and input/output enable, ensuring proper signal integrity
4. **Boot-Time Setup**: The pinctrl framework applies configuration before the driver probes, ensuring hardware is ready

Our SPI0 configuration sets four pins (SCLK, MOSI, MISO, CS0) to Mode 0 (SPI function) with input enable and pull-up resistors, allowing the SPI0 controller to communicate with external SPI devices like the W25Q32 flash chip.

Without this configuration, the pins remain in their default state (typically GPIO or disabled), and the SPI controller cannot send or receive data through the physical expansion header pins, resulting in failed device detection and communication errors.
