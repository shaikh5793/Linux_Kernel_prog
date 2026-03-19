# Makefile for Syscall Tracer eBPF Example
# 
# This builds an eBPF program that traces open(), read(), and write() system calls.
# It demonstrates multiple tracepoint attachments and syscall argument extraction.

BPFTOOL := bpftool

BPF_CLANG ?= clang
BPF_CFLAGS ?= -O2 -g -Wall -Werror -target bpf -D__TARGET_ARCH_x86

LIBBPF_CFLAGS := $(shell pkg-config --cflags libbpf 2>/dev/null || echo "")
LIBBPF_LDLIBS := $(shell pkg-config --libs libbpf 2>/dev/null || echo "-lbpf -lelf -lz")

all: user/trace_syscalls

# Generate vmlinux.h from the running kernel's BTF data
bpf/vmlinux.h:
	@echo "🔧 Generating vmlinux.h from kernel BTF..."
	@mkdir -p bpf
	$(BPFTOOL) btf dump file /sys/kernel/btf/vmlinux format c > bpf/vmlinux.h
	@echo "✅ vmlinux.h generated ($(shell wc -l < bpf/vmlinux.h) lines)"

# Compile the eBPF program to bytecode
bpf/trace_syscalls.bpf.o: bpf/trace_syscalls.bpf.c bpf/vmlinux.h
	@echo "🔨 Compiling eBPF program..."
	$(BPF_CLANG) $(BPF_CFLAGS) -I bpf -c $< -o $@
	@echo "✅ eBPF bytecode compiled"

# Generate the skeleton header from the eBPF bytecode
bpf/trace_syscalls.skel.h: bpf/trace_syscalls.bpf.o
	@echo "🏗️  Generating skeleton header..."
	$(BPFTOOL) gen skeleton $< > $@
	@echo "✅ Skeleton header generated"

# Compile the user-space program
user/trace_syscalls: user/trace_syscalls.c bpf/trace_syscalls.skel.h
	@echo "🔗 Compiling user-space program..."
	$(CC) -g -O2 -Wall -Werror -I bpf $(LIBBPF_CFLAGS) $< -o $@ $(LIBBPF_LDLIBS)
	@echo "✅ User-space program compiled"
	@echo ""
	@echo "🎉 Build complete! Run with:"
	@echo "   sudo ./user/trace_syscalls"

# Clean build artifacts
.PHONY: clean
clean:
	@echo "🧹 Cleaning build artifacts..."
	rm -f bpf/*.o bpf/*.skel.h bpf/vmlinux.h user/trace_syscalls
	@echo "✅ Clean complete"

# Help target
.PHONY: help
help:
	@echo "Available targets:"
	@echo "  all     - Build the complete syscall tracer"
	@echo "  clean   - Remove build artifacts"
	@echo "  help    - Show this help message"
	@echo ""
	@echo "Usage:"
	@echo "  make               # Build everything"
	@echo "  sudo ./user/trace_syscalls  # Run the tracer"
	@echo "  make clean         # Clean up"

# Show what this example demonstrates
.PHONY: info
info:
	@echo "eBPF Syscall Tracer Example"
	@echo "==========================="
	@echo "This example demonstrates:"
	@echo "• Multiple eBPF programs in one file"
	@echo "• Tracing different system calls (openat, read, write)"
	@echo "• Extracting syscall arguments"
	@echo "• Process information (PID, TID, command name)"
	@echo "• Multiple tracepoint attachments"
	@echo ""
	@echo "Files:"
	@echo "• bpf/trace_syscalls.bpf.c - eBPF kernel programs"
	@echo "• user/trace_syscalls.c    - User-space loader"
	@echo "• Makefile                 - Build configuration"