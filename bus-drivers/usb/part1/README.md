# USB Driver Examples - Part 1

Three USB driver examples targeting HP USB pendrive (0x03f0:0x6d40).

## Examples

**discovery.c** - USB descriptor analysis and endpoint discovery  
**sync.c** - Synchronous bulk transfers using `usb_bulk_msg()`  
**async.c** - Asynchronous bulk transfers using URBs

## Build and Test

```bash
make all                    # Build all modules
make load-discovery         # Load discovery driver
make dmesg                  # View kernel messages
make unload-all            # Unload all modules
```
