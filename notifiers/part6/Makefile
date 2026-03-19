# Part 6: Device Bus Notifiers (PCI/I2C/SPI/Platform)

obj-m += pci_bus_notifier.o
obj-m += i2c_bus_notifier.o
obj-m += spi_bus_notifier.o
obj-m += platform_bus_notifier.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

install:
	sudo insmod pci_bus_notifier.ko || true
	sudo insmod i2c_bus_notifier.ko || true
	sudo insmod spi_bus_notifier.ko || true
	sudo insmod platform_bus_notifier.ko || true

uninstall:
	sudo rmmod platform_bus_notifier || true
	sudo rmmod spi_bus_notifier || true
	sudo rmmod i2c_bus_notifier || true
	sudo rmmod pci_bus_notifier || true

info:
	@echo "=== Part 6: Device Bus Notifiers (PCI/I2C/SPI/Platform) ==="
	@echo "Modules:"
	@ls -lh *.ko 2>/dev/null || echo "  (not built yet)"
	@echo ""
	@echo "Loaded modules:"
	@lsmod | grep -E "(pci|i2c|spi|platform)_bus_notifier" || echo "  (none loaded)"

.PHONY: all clean install uninstall info

