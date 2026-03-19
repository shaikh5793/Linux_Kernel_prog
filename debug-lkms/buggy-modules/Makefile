# Copyright (c) 2024 TECH VEDA(www.techveda.org)
# Author: Raghu Bharadwaj
#
# This software is dual-licensed under the MIT License and GPL v2.
# See the accompanying LICENSE file for the full text.
#
# Buggy Kernel Modules for Debug Testing
# Makefile for building all modules

# Module object files
obj-m += memory/use_after_free.o
obj-m += memory/buffer_overflow.o
obj-m += memory/double_free.o
obj-m += memory/slab_corruption.o
obj-m += memory/invalid_free.o
obj-m += memory/oob_read.o
obj-m += memory/krealloc_misuse.o
obj-m += locking/deadlock.o
obj-m += locking/double_unlock.o
obj-m += locking/missing_lock.o
obj-m += locking/sleep_in_atomic.o
obj-m += locking/spin_recursion.o
obj-m += locking/rwlock_misuse.o
obj-m += locking/rwsem_misuse.o
obj-m += locking/rcu_misuse.o
obj-m += address/null_deref.o
obj-m += address/invalid_access.o

# Kernel source directory (from Buildroot)
KERNEL_DIR ?= /home/raghub/raghu/buildroot/output/build/linux-6.12.47

# Architecture and cross-compiler
ARCH ?= arm
CROSS_COMPILE ?= /home/raghub/raghu/buildroot/output/host/bin/arm-buildroot-linux-gnueabihf-

# Current directory
PWD := $(shell pwd)

# Default target - build all modules
all: modules

modules:
	@echo "Building all buggy kernel modules..."
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules
	@echo ""
	@echo "Build complete! Modules created:"
	@find . -name "*.ko" -type f

# Clean build artifacts
clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean
	@echo "Clean complete"

# Install modules to local directory
install: modules
	@echo "Installing modules to $(PWD)/install"
	@mkdir -p $(PWD)/install
	@find . -name "*.ko" -type f -exec cp {} $(PWD)/install/ \;
	@echo "Modules installed to $(PWD)/install/"
	@ls -lh $(PWD)/install/

# List all modules
list:
	@echo "=== Buggy Kernel Modules ==="
	@echo ""
	@echo "Memory Bug Modules:"
	@echo "  - use_after_free.ko      : Use-after-free for KASAN/KFENCE"
	@echo "  - buffer_overflow.ko     : Buffer overflow for KASAN/SLUB"
	@echo "  - double_free.ko         : Double free for SLUB debug"
	@echo "  - slab_corruption.ko     : Slab corruption for SLUB debug"
	@echo ""
	@echo "Locking Bug Modules:"
	@echo "  - deadlock.ko            : Deadlock scenarios for lockdep"
	@echo "  - double_unlock.ko       : Double unlock for lockdep"
	@echo "  - missing_lock.ko        : Race conditions from missing locks"
	@echo ""
	@echo "Address Translation Bug Modules:"
	@echo "  - null_deref.ko          : Null pointer dereference"
	@echo "  - invalid_access.ko      : Invalid memory access patterns"

# Verify built modules
verify:
	@echo "=== Verifying Module Builds ==="
	@for ko in $$(find . -name "*.ko" -type f); do \
		echo ""; \
		echo "Module: $$ko"; \
		file $$ko | grep -q "ARM" && echo "  ✓ Architecture: ARM" || echo "  ✗ Architecture: NOT ARM"; \
		modinfo $$ko 2>/dev/null | grep -E "(filename|version|description)" || true; \
	done

# Help target
help:
	@echo "Buggy Kernel Modules Build System"
	@echo ""
	@echo "Available targets:"
	@echo "  all (default) - Build all buggy kernel modules"
	@echo "  modules       - Same as 'all'"
	@echo "  clean         - Clean build artifacts"
	@echo "  install       - Build and copy modules to install directory"
	@echo "  list          - List all available modules with descriptions"
	@echo "  verify        - Verify built modules are correct ARM binaries"
	@echo "  help          - Show this help message"
	@echo ""
	@echo "Environment variables:"
	@echo "  KERNEL_DIR     - Path to kernel source (default: $(KERNEL_DIR))"
	@echo "  ARCH           - Target architecture (default: $(ARCH))"
	@echo "  CROSS_COMPILE  - Cross-compiler prefix (default: $(CROSS_COMPILE))"
	@echo ""
	@echo "Examples:"
	@echo "  make                    # Build all modules"
	@echo "  make clean              # Clean build"
	@echo "  make install            # Build and install modules"
	@echo "  make list               # List available modules"
	@echo "  make verify             # Verify module builds"

.PHONY: all modules clean install list verify help
