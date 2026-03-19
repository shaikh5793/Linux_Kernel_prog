#!/bin/bash

set -e  # Exit on error

echo "========================================="
echo "Linux Device Driver Development Setup"
echo "========================================="

# Update package lists
echo -e "\n[1/8] Updating package lists..."
sudo apt-get update

# Install essential build tools
echo -e "\n[2/8] Installing build-essential and core compilers..."
sudo apt-get install -y build-essential clang

# Install kernel development essentials
echo -e "\n[3/8] Installing kernel development tools..."
sudo apt-get install -y \
    linux-headers-$(uname -r) \
    kmod \
    dkms \
    bc \
    bison \
    flex

# Install development libraries
echo -e "\n[4/8] Installing development libraries..."
sudo apt-get install -y \
    libssl-dev \
    libc6-dev \
    libncurses-dev \
    libncursesw5-dev \
    libelf-dev \
    glibc-source

# Install debugging and analysis tools
echo -e "\n[5/8] Installing debugging and analysis tools..."
sudo apt-get install -y \
    gdb \
    valgrind \
    sparse \
    cppcheck \
    dwarves

# Install kernel code navigation tools
echo -e "\n[6/8] Installing code navigation tools..."
sudo apt-get install -y \
    vim \
    universal-ctags \
    cscope \
    global

# Install version control and utilities
echo -e "\n[7/8] Installing version control and utilities..."
sudo apt-get install -y \
    git \
    make \
    tilix \
    tree \
    htop

# Install tracing and performance tools
echo -e "\n[8/10] Installing tracing and performance tools..."
sudo apt-get install -y \
    trace-cmd \
    linux-tools-common \
    linux-tools-generic \
    linux-tools-$(uname -r) 2>/dev/null || echo "Note: linux-tools for current kernel not available"

# Install QEMU for ARM emulation
echo -e "\n[9/10] Installing QEMU ARM emulator..."
sudo apt-get install -y \
    qemu-system-arm \
    qemu-system-aarch64 \
    qemu-user \
    qemu-user-static \
    qemu-utils

# Install ARM cross-compilation toolchain
echo -e "\n[10/10] Installing ARM cross-compilation toolchain..."
sudo apt-get install -y \
    gcc-arm-linux-gnueabi \
    gcc-arm-linux-gnueabihf \
    gcc-aarch64-linux-gnu \
    gdb-multiarch \
    binutils-arm-linux-gnueabi \
    binutils-aarch64-linux-gnu

echo -e "\n========================================="
echo "Installation complete!"
echo "========================================="
echo ""
echo "Installed tools summary:"
echo "  - Compilers: GCC, Clang"
echo "  - Kernel headers for: $(uname -r)"
echo "  - Debuggers: GDB, Valgrind, gdb-multiarch"
echo "  - Code navigation: universal-ctags, cscope, global"
echo "  - Static analysis: sparse, cppcheck"
echo "  - Tracing: trace-cmd, perf"
echo "  - Module tools: kmod, DKMS"
echo "  - QEMU: ARM (32-bit & 64-bit) system and user mode"
echo "  - Cross-compilers: ARM (EABI/EABIHF), AArch64"
echo ""
echo "Optional installations (uncomment if needed):"
echo "  - coccinelle (semantic patch tool)"
echo "  - systemtap (advanced tracing)"
echo "  - crash (kernel crash dump analysis)"
echo "  - emacs or other editors"

# Optional: Install advanced tools (uncommented)
# echo -e "\n[Optional] Installing Coccinelle..."
# sudo apt-get install -y coccinelle

# echo -e "\n[Optional] Installing SystemTap..."
# sudo apt-get install -y systemtap systemtap-sdt-dev

# echo -e "\n[Optional] Installing crash utility..."
# sudo apt-get install -y crash

# echo -e "\n[Optional] Installing Emacs..."
# sudo apt-get install -y emacs



