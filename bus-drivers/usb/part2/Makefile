# Modules to build
obj-m += interrupt.o
obj-m += hid.o
obj-m += chardev.o

# Original variables
KERNEL_DIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

# Build with license validation
all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules
	@echo ""
	@echo "Part 2 USB examples built successfully:"
	@ls -la *.ko 2>/dev/null || echo "No .ko files found"

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean

# Individual module operations
load-interrupt:
	sudo insmod interrupt.ko

unload-interrupt:
	sudo rmmod interrupt

load-hid:
	sudo insmod hid.ko

unload-hid:
	sudo rmmod hid

load-chardev:
	sudo insmod chardev.ko

unload-chardev:
	sudo rmmod chardev

# Load all modules
load-all: load-interrupt load-hid load-chardev
	@echo "All Part 2 modules loaded"

# Unload all modules
unload-all:
	-sudo rmmod chardev 2>/dev/null
	-sudo rmmod hid 2>/dev/null
	-sudo rmmod interrupt 2>/dev/null
	@echo "All Part 2 modules unloaded"

# Show kernel messages
dmesg:
	sudo dmesg | tail -20

# Show loaded USB modules
lsmod:
	lsmod | grep -E "(interrupt|hid|chardev)"

# Test sequence - load, show status, unload
test: all
	@echo "Testing Part 2 USB drivers..."

.PHONY: all build_targets
