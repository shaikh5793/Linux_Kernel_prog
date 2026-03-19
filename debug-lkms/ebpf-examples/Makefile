# Copyright (c) 2024 TECH VEDA(www.techveda.org)
# Author: Raghu Bharadwaj
#
# This software is licensed under GPL v2.
# See the accompanying LICENSE file for the full text.
#
# Makefile for eBPF examples - ARM VExpress
# Builds on host x86, runs on ARM target

CLANG ?= clang
LLC ?= llc
ARCH := arm

# Kernel headers from buildroot
KERNEL_DIR := /home/raghub/raghu/buildroot/output/build/linux-6.12.47
INCLUDES := -I$(KERNEL_DIR)/arch/$(ARCH)/include \
            -I$(KERNEL_DIR)/arch/$(ARCH)/include/generated \
            -I$(KERNEL_DIR)/include \
            -I$(KERNEL_DIR)/include/generated/uapi \
            -I$(KERNEL_DIR)/include/uapi \
            -I$(KERNEL_DIR)/tools/testing/selftests/bpf

# eBPF compilation flags
BPF_CFLAGS := -target bpf \
              -D__KERNEL__ \
              -D__BPF_TRACING__ \
              -D__TARGET_ARCH_arm \
              -Wno-unused-value \
              -Wno-pointer-sign \
              -Wno-compare-distinct-pointer-types \
              -O2 -g

SRC_DIR := src
OBJ_DIR := obj
BIN_DIR := bin

# eBPF source files
SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))
BINS := $(patsubst $(SRC_DIR)/%.c,$(BIN_DIR)/%,$(SRCS))

.PHONY: all clean install verify list

all: dirs $(OBJS)
	@echo "eBPF programs built successfully!"
	@echo "Object files in $(OBJ_DIR)/"

dirs:
	@mkdir -p $(OBJ_DIR) $(BIN_DIR)

# Compile eBPF C to object file
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "Compiling $< ..."
	$(CLANG) $(BPF_CFLAGS) $(INCLUDES) -c $< -o $@

# Install to bin directory
install: all
	@echo "Installing eBPF objects to $(BIN_DIR)/"
	@cp $(OBJ_DIR)/*.o $(BIN_DIR)/ 2>/dev/null || true
	@echo "Installed:"
	@ls -lh $(BIN_DIR)/

# Verify eBPF objects
verify: all
	@echo "Verifying eBPF objects..."
	@for obj in $(OBJS); do \
		echo ""; \
		echo "=== $$obj ==="; \
		file $$obj; \
		llvm-objdump -h $$obj | grep -E "(\.text|\.maps|kprobe|tracepoint)" || true; \
	done

# List all programs
list:
	@echo "Available eBPF Programs:"
	@echo "  memleak_tracker    - Track kmalloc/kfree to detect leaks"
	@echo "  syscall_tracer     - Trace syscalls with latency"
	@echo "  func_latency       - Measure function execution time"
	@echo "  lock_contention    - Analyze mutex lock contention"
	@echo "  pagefault_tracker  - Monitor page faults"

# Clean build artifacts
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
	@echo "Cleaned"

# Help
help:
	@echo "eBPF Examples Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all (default)  - Build all eBPF programs"
	@echo "  install        - Install to bin/ directory"
	@echo "  verify         - Verify built eBPF objects"
	@echo "  list           - List available programs"
	@echo "  clean          - Remove build artifacts"
	@echo ""
	@echo "Programs will be compiled on host (x86) for target (ARM)"
