# USB Driver Examples - Part 2

Advanced USB driver examples with real-world patterns.

## Examples

**interrupt.c** - Interrupt transfers with periodic URBs  
**hid.c** - HID class driver with boot protocol support  
**chardev.c** - Character device interface for user-space access

## Build and Test

```bash
make all                    # Build all modules
make load-interrupt         # Load interrupt driver
make dmesg                  # View kernel messages
make unload-all            # Unload all modules
```

## Device IDs

Default placeholder IDs (0x1234:0x5678). Update for real devices:

```c
#define USB_VENDOR_ID   0x045e  // Your device VID
#define USB_PRODUCT_ID  0x0040  // Your device PID
```
