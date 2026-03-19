# Modules to build
obj-m += discovery.o
obj-m += sync.o
obj-m += async.o

# Original variables
KERNEL_DIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

# Build
all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules
	@echo ""
	@echo "Part 1 USB examples built successfully:"
	@ls -la *.ko 2>/dev/null || echo "No .ko files found"

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean

# Individual module operations
load-discovery:
	sudo insmod discovery.ko

unload-discovery:
	sudo rmmod discovery

load-sync:
	sudo insmod sync.ko

unload-sync:
	sudo rmmod sync

load-async:
	sudo insmod async.ko

unload-async:
	sudo rmmod async

# Load all modules
load-all: load-discovery load-sync load-async
	@echo "All Part 1 modules loaded"

# Unload all modules
unload-all:
	-sudo rmmod async 2>/dev/null
	-sudo rmmod sync 2>/dev/null
	-sudo rmmod discovery 2>/dev/null
	@echo "All Part 1 modules unloaded"

# Show kernel messages
dmesg:
	sudo dmesg | tail -20

# Show loaded USB modules
lsmod:
	lsmod | grep -E "(discovery|sync|async)"

# Test sequence - load, show status, unload
test: all
	@echo "Testing Part 1 USB drivers..."

.PHONY: all build_targets
