#!/bin/bash
#
# Automated Kernel Build Script for x86 Machines
# Builds kernel 6.14.x with optimized configuration for development
# Copyright (c) 2025 Techveda
#
# Licensed under MIT License
#

set -e

SCRIPT_NAME=$(basename $0)
KERNEL_SRC="$HOME/linux-kernel-6.14"
BUILD_DIR="$KERNEL_SRC/build-output"
CONFIG_NAME="kernpro-config"
JOBS=$(nproc)

echo "=== Kernpro Kernel Build Script ==="
echo "Building kernel 6.14.x for x86 development environment"
echo ""

# Function to check prerequisites
check_prerequisites() {
    echo "=== Checking Prerequisites ==="
    
    # Check if kernel source exists
    if [ ! -d "$KERNEL_SRC" ]; then
        echo "✗ Kernel source not found at: $KERNEL_SRC"
        echo "Run ./setup-environment.sh first to clone kernel source"
        exit 1
    fi
    
    # Check if we're in kernel source directory
    if [ ! -f "$KERNEL_SRC/Makefile" ]; then
        echo "✗ Invalid kernel source directory"
        exit 1
    fi
    
    # Check available disk space (at least 20GB recommended)
    AVAILABLE_SPACE=$(df "$KERNEL_SRC" | awk 'NR==2 {print $4}')
    REQUIRED_SPACE=20971520  # 20GB in KB
    
    if [ "$AVAILABLE_SPACE" -lt "$REQUIRED_SPACE" ]; then
        echo "⚠ Warning: Low disk space. Kernel build requires ~20GB"
        echo "Available: $(($AVAILABLE_SPACE / 1024 / 1024))GB"
        echo "Continue anyway? [y/N]"
        read -r response
        if [[ ! "$response" =~ ^[Yy]$ ]]; then
            exit 1
        fi
    fi
    
    # Check essential build tools
    TOOLS=("gcc" "make" "bc" "bison" "flex")
    for tool in "${TOOLS[@]}"; do
        if command -v $tool &> /dev/null; then
            echo "✓ $tool available"
        else
            echo "✗ $tool missing - run ./setup-environment.sh first"
            exit 1
        fi
    done
    
    echo "✓ Prerequisites satisfied"
}

# Function to create optimized config
create_config() {
    echo ""
    echo "=== Creating Build Configuration ==="
    
    cd "$KERNEL_SRC"
    
    # Try to use running kernel's configuration as base
    RUNNING_KERNEL=$(uname -r)
    KERNEL_CONFIG=""
    
    # Look for running kernel config in common locations
    if [ -f "/boot/config-$RUNNING_KERNEL" ]; then
        KERNEL_CONFIG="/boot/config-$RUNNING_KERNEL"
        echo "Using running kernel config: $KERNEL_CONFIG"
        cp "$KERNEL_CONFIG" .config
    elif [ -f "/proc/config.gz" ]; then
        KERNEL_CONFIG="/proc/config.gz"
        echo "Using running kernel config from: $KERNEL_CONFIG"
        zcat /proc/config.gz > .config
    elif [ -f "/boot/config-$(uname -r | cut -d'-' -f1,2)" ]; then
        KERNEL_CONFIG="/boot/config-$(uname -r | cut -d'-' -f1,2)"
        echo "Using kernel config: $KERNEL_CONFIG"
        cp "$KERNEL_CONFIG" .config
    else
        echo "Running kernel config not found, using defconfig as fallback..."
        NATIVE_ARCH=$(detect_architecture)
        make defconfig ARCH=$NATIVE_ARCH
    fi
    
    # Update config for current kernel source version
    NATIVE_ARCH=$(detect_architecture)
    if [ -n "$KERNEL_CONFIG" ]; then
        echo "Updating config for current kernel source (ARCH=$NATIVE_ARCH)..."
        make olddefconfig ARCH=$NATIVE_ARCH
    fi
    
    # Create custom config script for development and security options
    echo "Applying development and security customizations..."
    
    cat > customize_config.sh << 'EOF'
#!/bin/bash
# Disable security certificates for successful build
echo "Disabling security certificate requirements..."
scripts/config --disable CONFIG_SECURITY_LOCKDOWN_LSM
scripts/config --disable CONFIG_SECURITY_LOCKDOWN_LSM_EARLY
scripts/config --disable CONFIG_LOCK_DOWN_KERNEL_FORCE_NONE
scripts/config --disable CONFIG_LOCK_DOWN_KERNEL_FORCE_INTEGRITY
scripts/config --disable CONFIG_LOCK_DOWN_KERNEL_FORCE_CONFIDENTIALITY
scripts/config --disable CONFIG_MODULE_SIG
scripts/config --disable CONFIG_MODULE_SIG_FORCE
scripts/config --disable CONFIG_MODULE_SIG_ALL
scripts/config --disable CONFIG_MODULE_SIG_SHA1
scripts/config --disable CONFIG_MODULE_SIG_SHA224
scripts/config --disable CONFIG_MODULE_SIG_SHA256
scripts/config --disable CONFIG_MODULE_SIG_SHA384
scripts/config --disable CONFIG_MODULE_SIG_SHA512
scripts/config --disable CONFIG_SYSTEM_TRUSTED_KEYRING
scripts/config --disable CONFIG_SYSTEM_TRUSTED_KEYS
scripts/config --disable CONFIG_SYSTEM_EXTRA_CERTIFICATE
scripts/config --disable CONFIG_SECONDARY_TRUSTED_KEYRING
scripts/config --disable CONFIG_SYSTEM_BLACKLIST_KEYRING
scripts/config --set-str CONFIG_SYSTEM_TRUSTED_KEYS ""
scripts/config --set-str CONFIG_SYSTEM_EXTRA_CERTIFICATE ""

# Enable kernel development options
echo "Enabling development and debugging options..."
scripts/config --enable CONFIG_IKCONFIG
scripts/config --enable CONFIG_IKCONFIG_PROC
scripts/config --enable CONFIG_DEBUG_KERNEL
scripts/config --enable CONFIG_DEBUG_INFO
scripts/config --enable CONFIG_DEBUG_INFO_DWARF_TOOLCHAIN_DEFAULT
scripts/config --enable CONFIG_FRAME_POINTER
scripts/config --enable CONFIG_KALLSYMS_ALL
scripts/config --enable CONFIG_DEBUG_FS
scripts/config --enable CONFIG_PROC_KCORE
scripts/config --enable CONFIG_KGDB
scripts/config --enable CONFIG_KGDB_SERIAL_CONSOLE
scripts/config --enable CONFIG_MAGIC_SYSRQ
scripts/config --enable CONFIG_DEBUG_SLAB
scripts/config --enable CONFIG_DEBUG_PAGEALLOC
scripts/config --enable CONFIG_DEBUG_MUTEXES
scripts/config --enable CONFIG_DEBUG_SPINLOCK
scripts/config --enable CONFIG_DEBUG_ATOMIC_SLEEP
scripts/config --enable CONFIG_STACKTRACE
scripts/config --enable CONFIG_DEBUG_BUGVERBOSE

# Enable module development support
echo "Enabling module development support..."
scripts/config --enable CONFIG_MODULES
scripts/config --enable CONFIG_MODULE_UNLOAD
scripts/config --enable CONFIG_MODULE_FORCE_UNLOAD
scripts/config --enable CONFIG_MODVERSIONS
scripts/config --enable CONFIG_MODULE_SRCVERSION_ALL

# Enable useful filesystems and features
echo "Enabling essential filesystem support..."
scripts/config --enable CONFIG_EXT4_FS
scripts/config --enable CONFIG_PROC_FS
scripts/config --enable CONFIG_SYSFS
scripts/config --enable CONFIG_TMPFS
scripts/config --enable CONFIG_DEVTMPFS
scripts/config --enable CONFIG_DEVTMPFS_MOUNT

# Disable heavy debug options for faster build
echo "Disabling heavy debug options for faster compilation..."
scripts/config --disable CONFIG_DEBUG_INFO_BTF
scripts/config --disable CONFIG_GDB_SCRIPTS
scripts/config --disable CONFIG_DEBUG_INFO_SPLIT
scripts/config --disable CONFIG_DEBUG_INFO_REDUCED
EOF

    chmod +x customize_config.sh
    ./customize_config.sh
    rm customize_config.sh
    
    # Final configuration update to resolve any dependencies
    echo "Finalizing configuration..."
    make olddefconfig ARCH=$NATIVE_ARCH
    
    # Save configuration
    cp .config "$CONFIG_NAME"
    echo "✓ Configuration saved as: $CONFIG_NAME"
}

# Function to detect and set native architecture
detect_architecture() {
    local arch=$(uname -m)
    local kernel_arch=""
    
    case "$arch" in
        x86_64|amd64)
            kernel_arch="x86_64"
            ;;
        i386|i686)
            kernel_arch="i386"
            ;;
        aarch64|arm64)
            kernel_arch="arm64"
            ;;
        armv7l|armv6l)
            kernel_arch="arm"
            ;;
        *)
            echo "⚠ Unsupported architecture: $arch"
            echo "Defaulting to x86_64"
            kernel_arch="x86_64"
            ;;
    esac
    
    echo "$kernel_arch"
}

# Function to build kernel
build_kernel() {
    echo ""
    echo "=== Building Kernel ==="
    
    # Detect native architecture
    NATIVE_ARCH=$(detect_architecture)
    echo "🏗️  Build Configuration:"
    echo "   Architecture: $(uname -m) -> $NATIVE_ARCH"
    echo "   Parallel jobs: $JOBS"
    echo "   Build directory: $BUILD_DIR"
    echo "   Estimated time: 30-120 minutes (depending on hardware)"
    echo ""
    
    cd "$KERNEL_SRC"
    
    # Create build output directory
    mkdir -p "$BUILD_DIR"
    
    # Get kernel version for display
    VERSION=$(grep "^VERSION" Makefile | cut -d'=' -f2 | tr -d ' ')
    PATCHLEVEL=$(grep "^PATCHLEVEL" Makefile | cut -d'=' -f2 | tr -d ' ')
    SUBLEVEL=$(grep "^SUBLEVEL" Makefile | cut -d'=' -f2 | tr -d ' ')
    KERNEL_VER="$VERSION.$PATCHLEVEL.$SUBLEVEL"
    
    echo "🐧 Building Linux Kernel $KERNEL_VER"
    echo "⏰ Started at: $(date)"
    echo ""
    
    START_TIME=$(date +%s)
    
    # Stage 1: Build kernel image
    echo "📦 Stage 1/4: Building kernel image (bzImage)..."
    echo "   This creates the compressed bootable kernel image"
    make -j$JOBS ARCH=$NATIVE_ARCH bzImage || {
        echo "✗ Kernel image build failed"
        exit 1
    }
    
    STAGE1_TIME=$(date +%s)
    echo "✓ Stage 1 completed in $(($STAGE1_TIME - $START_TIME))s"
    echo ""
    
    # Stage 2: Build kernel modules
    echo "🔧 Stage 2/4: Building kernel modules..."
    echo "   This compiles all loadable kernel modules (.ko files)"
    make -j$JOBS ARCH=$NATIVE_ARCH modules || {
        echo "✗ Kernel modules build failed"
        exit 1
    }
    
    STAGE2_TIME=$(date +%s)
    echo "✓ Stage 2 completed in $(($STAGE2_TIME - $STAGE1_TIME))s"
    echo ""
    
    # Stage 3: Install headers
    echo "📋 Stage 3/4: Installing kernel headers..."
    echo "   This prepares headers for external module compilation"
    make -j$JOBS ARCH=$NATIVE_ARCH headers_install INSTALL_HDR_PATH="$BUILD_DIR/headers" || {
        echo "✗ Headers installation failed"
        exit 1
    }
    
    STAGE3_TIME=$(date +%s)
    echo "✓ Stage 3 completed in $(($STAGE3_TIME - $STAGE2_TIME))s"
    echo ""
    
    # Stage 4: Package build artifacts
    echo "📁 Stage 4/4: Packaging build artifacts..."
    echo "   Collecting and organizing build outputs"
    
    # Copy kernel image (architecture-aware)
    echo "   → Copying kernel image..."
    case "$NATIVE_ARCH" in
        x86_64|i386)
            cp arch/x86/boot/bzImage "$BUILD_DIR/vmlinuz-kernpro-$KERNEL_VER"
            KERNEL_SIZE=$(du -h arch/x86/boot/bzImage | cut -f1)
            ;;
        arm64)
            if [ -f arch/arm64/boot/Image.gz ]; then
                cp arch/arm64/boot/Image.gz "$BUILD_DIR/vmlinuz-kernpro-$KERNEL_VER"
                KERNEL_SIZE=$(du -h arch/arm64/boot/Image.gz | cut -f1)
            else
                cp arch/arm64/boot/Image "$BUILD_DIR/vmlinuz-kernpro-$KERNEL_VER"
                KERNEL_SIZE=$(du -h arch/arm64/boot/Image | cut -f1)
            fi
            ;;
        arm)
            if [ -f arch/arm/boot/zImage ]; then
                cp arch/arm/boot/zImage "$BUILD_DIR/vmlinuz-kernpro-$KERNEL_VER"
                KERNEL_SIZE=$(du -h arch/arm/boot/zImage | cut -f1)
            else
                cp arch/arm/boot/Image "$BUILD_DIR/vmlinuz-kernpro-$KERNEL_VER"
                KERNEL_SIZE=$(du -h arch/arm/boot/Image | cut -f1)
            fi
            ;;
        *)
            echo "   ⚠ Unknown kernel image location for $NATIVE_ARCH"
            KERNEL_IMAGE=$(find arch/ -name "*Image*" -o -name "vmlinux" | head -1)
            cp "$KERNEL_IMAGE" "$BUILD_DIR/vmlinuz-kernpro-$KERNEL_VER"
            KERNEL_SIZE=$(du -h "$KERNEL_IMAGE" | cut -f1)
            ;;
    esac
    
    echo "   → Copying system map and config..."
    cp System.map "$BUILD_DIR/System.map-kernpro-$KERNEL_VER"
    cp .config "$BUILD_DIR/config-kernpro-$KERNEL_VER"
    
    # Create convenience symlinks
    ln -sf "vmlinuz-kernpro-$KERNEL_VER" "$BUILD_DIR/vmlinuz-kernpro"
    ln -sf "System.map-kernpro-$KERNEL_VER" "$BUILD_DIR/System.map-kernpro"
    ln -sf "config-kernpro-$KERNEL_VER" "$BUILD_DIR/config-kernpro"
    
    echo "   → Installing modules to build directory..."
    make modules_install INSTALL_MOD_PATH="$BUILD_DIR/modules" ARCH=$NATIVE_ARCH
    
    # Generate module dependency information
    echo "   → Generating module dependencies..."
    /sbin/depmod -b "$BUILD_DIR/modules" "$KERNEL_VER" 2>/dev/null || echo "   ⚠ depmod failed (non-critical)"
    
    STAGE4_TIME=$(date +%s)
    BUILD_TIME=$((STAGE4_TIME - START_TIME))
    
    echo "✓ Stage 4 completed in $(($STAGE4_TIME - $STAGE3_TIME))s"
    echo ""
    echo "🎉 Kernel build completed successfully!"
    echo "   Total build time: $(($BUILD_TIME / 60))m $(($BUILD_TIME % 60))s"
    echo "   Kernel version: $KERNEL_VER"
    echo "   Kernel size: $KERNEL_SIZE"
    echo "   Architecture: $NATIVE_ARCH"
    echo "   Build artifacts: $BUILD_DIR"
}

# Function to install kernel to system
install_kernel() {
    local kernel_ver="$1"
    
    echo ""
    echo "=== Kernel Installation to System ==="
    echo ""
    echo "⚠️  WARNING: This will install the kernel to your system!"
    echo "   This operation requires root privileges and modifies system files."
    echo "   Only proceed if you understand the risks and have backups."
    echo ""
    echo "📋 Installation Summary:"
    echo "   Kernel version: $kernel_ver"
    echo "   Architecture: $NATIVE_ARCH"
    echo "   Source: $BUILD_DIR"
    echo "   Target: /boot/ and /lib/modules/"
    echo ""
    echo "🔧 Installation will:"
    echo "   1. Copy kernel image to /boot/"
    echo "   2. Copy System.map and config to /boot/"
    echo "   3. Install kernel modules to /lib/modules/"
    echo "   4. Update initramfs"
    echo "   5. Update GRUB configuration"
    echo ""
    
    # Triple confirmation for safety
    echo "Do you want to install this kernel to your system? [y/N]"
    read -r response1
    if [[ ! "$response1" =~ ^[Yy]$ ]]; then
        echo "Installation cancelled. Build artifacts remain in: $BUILD_DIR"
        return 0
    fi
    
    echo ""
    echo "⚠️  FINAL WARNING: This will modify your boot configuration!"
    echo "Are you absolutely sure you want to proceed? Type 'YES' to continue:"
    read -r response2
    if [[ "$response2" != "YES" ]]; then
        echo "Installation cancelled. Build artifacts remain in: $BUILD_DIR"
        return 0
    fi
    
    echo ""
    echo "🚀 Starting kernel installation..."
    
    # Check if we have root privileges
    if [ "$EUID" -ne 0 ]; then
        echo "Root privileges required for installation. Using sudo..."
        SUDO="sudo"
    else
        SUDO=""
    fi
    
    # Step 1: Install kernel image
    echo ""
    echo "📦 Step 1/5: Installing kernel image..."
    $SUDO cp "$BUILD_DIR/vmlinuz-kernpro-$kernel_ver" "/boot/vmlinuz-$kernel_ver"
    $SUDO chmod 644 "/boot/vmlinuz-$kernel_ver"
    echo "   ✓ Kernel image installed: /boot/vmlinuz-$kernel_ver"
    
    # Step 2: Install System.map
    echo ""
    echo "🗺️  Step 2/5: Installing System.map..."
    $SUDO cp "$BUILD_DIR/System.map-kernpro-$kernel_ver" "/boot/System.map-$kernel_ver"
    $SUDO chmod 644 "/boot/System.map-$kernel_ver"
    echo "   ✓ System.map installed: /boot/System.map-$kernel_ver"
    
    # Step 3: Install config
    echo ""
    echo "⚙️  Step 3/5: Installing kernel config..."
    $SUDO cp "$BUILD_DIR/config-kernpro-$kernel_ver" "/boot/config-$kernel_ver"
    $SUDO chmod 644 "/boot/config-$kernel_ver"
    echo "   ✓ Config installed: /boot/config-$kernel_ver"
    
    # Step 4: Install modules
    echo ""
    echo "🔧 Step 4/5: Installing kernel modules..."
    $SUDO cp -r "$BUILD_DIR/modules/lib/modules/$kernel_ver" "/lib/modules/"
    $SUDO chmod -R 755 "/lib/modules/$kernel_ver"
    
    # Run depmod to update module dependencies
    echo "   → Updating module dependencies..."
    $SUDO /sbin/depmod -a "$kernel_ver"
    echo "   ✓ Modules installed: /lib/modules/$kernel_ver"
    
    # Step 5: Update initramfs
    echo ""
    echo "💾 Step 5/5: Updating initramfs..."
    
    # Detect initramfs tool and update accordingly
    if command -v update-initramfs &> /dev/null; then
        # Ubuntu/Debian
        echo "   → Using update-initramfs (Ubuntu/Debian)..."
        $SUDO update-initramfs -c -k "$kernel_ver"
    elif command -v dracut &> /dev/null; then
        # RHEL/CentOS/Fedora
        echo "   → Using dracut (RHEL/CentOS/Fedora)..."
        $SUDO dracut -f "/boot/initramfs-$kernel_ver.img" "$kernel_ver"
    elif command -v mkinitcpio &> /dev/null; then
        # Arch Linux
        echo "   → Using mkinitcpio (Arch Linux)..."
        $SUDO mkinitcpio -k "$kernel_ver" -g "/boot/initramfs-$kernel_ver.img"
    else
        echo "   ⚠ No initramfs tool found. You may need to create initramfs manually."
    fi
    echo "   ✓ Initramfs updated"
    
    echo ""
    echo "✅ Kernel installation completed successfully!"
    echo ""
    echo "📋 Installed files:"
    echo "   /boot/vmlinuz-$kernel_ver"
    echo "   /boot/System.map-$kernel_ver"
    echo "   /boot/config-$kernel_ver"
    echo "   /boot/initramfs-$kernel_ver.img (if created)"
    echo "   /lib/modules/$kernel_ver/"
    echo ""
}

# Function to update GRUB configuration
update_grub() {
    local kernel_ver="$1"
    
    echo "=== Updating Boot Configuration ==="
    echo ""
    echo "🥾 Updating GRUB to include new kernel..."
    
    # Check if we have root privileges
    if [ "$EUID" -ne 0 ]; then
        SUDO="sudo"
    else
        SUDO=""
    fi
    
    # Detect GRUB update command based on distribution
    if command -v update-grub &> /dev/null; then
        # Ubuntu/Debian
        echo "   → Using update-grub (Ubuntu/Debian)..."
        $SUDO update-grub
    elif command -v grub2-mkconfig &> /dev/null; then
        # RHEL/CentOS/Fedora
        echo "   → Using grub2-mkconfig (RHEL/CentOS/Fedora)..."
        if [ -f /boot/grub2/grub.cfg ]; then
            $SUDO grub2-mkconfig -o /boot/grub2/grub.cfg
        elif [ -f /boot/efi/EFI/*/grub.cfg ]; then
            GRUB_CFG=$(find /boot/efi/EFI -name grub.cfg | head -1)
            $SUDO grub2-mkconfig -o "$GRUB_CFG"
        fi
    elif command -v grub-mkconfig &> /dev/null; then
        # Arch Linux and others
        echo "   → Using grub-mkconfig (Arch Linux)..."
        $SUDO grub-mkconfig -o /boot/grub/grub.cfg
    else
        echo "   ⚠ GRUB configuration tool not found."
        echo "   You may need to update GRUB configuration manually."
        return 1
    fi
    
    echo "   ✓ GRUB configuration updated"
    echo ""
    echo "🎯 Boot Configuration Summary:"
    echo "   New kernel: $kernel_ver"
    echo "   GRUB updated: ✓"
    echo "   Ready for reboot: ✓"
    echo ""
    echo "💡 Next steps:"
    echo "   1. Reboot your system"
    echo "   2. Select the new kernel from GRUB menu"
    echo "   3. Verify kernel with: uname -r"
    echo ""
    echo "⚠️  Keep your old kernel as backup in case of issues!"
}

# Function to show build summary
show_summary() {
    echo ""
    echo "=== Build Summary ==="
    
    cd "$KERNEL_SRC"
    
    # Get kernel version from Makefile
    VERSION=$(grep "^VERSION" Makefile | cut -d'=' -f2 | tr -d ' ')
    PATCHLEVEL=$(grep "^PATCHLEVEL" Makefile | cut -d'=' -f2 | tr -d ' ')
    SUBLEVEL=$(grep "^SUBLEVEL" Makefile | cut -d'=' -f2 | tr -d ' ')
    KERNEL_VER="$VERSION.$PATCHLEVEL.$SUBLEVEL"
    
    echo "Built kernel version: $KERNEL_VER"
    echo "Build directory: $BUILD_DIR"
    echo ""
    echo "Build artifacts:"
    echo "  Kernel image: $BUILD_DIR/vmlinuz-kernpro"
    echo "  System map: $BUILD_DIR/System.map-kernpro"
    echo "  Config file: $BUILD_DIR/config-kernpro"
    echo "  Headers: $BUILD_DIR/headers/"
    echo "  Modules: $BUILD_DIR/modules/"
    echo ""
    echo "Kernel image size: $(du -h "$BUILD_DIR/vmlinuz-kernpro" 2>/dev/null | cut -f1 || echo "N/A")"
    echo "Total build size: $(du -sh "$BUILD_DIR" 2>/dev/null | cut -f1 || echo "N/A")"
    
    echo ""
    echo "Next steps:"
    echo "1. Test module compilation against built kernel:"
    echo "   cd lkms/p1_base && make KERNEL_SRC=$KERNEL_SRC"
    echo "2. Optionally install kernel (advanced users only):"
    echo "   sudo cp $BUILD_DIR/vmlinuz-kernpro /boot/"
    echo "3. Use built headers for external module development"
}

# Function to clean previous builds
clean_build() {
    echo "Cleaning previous build artifacts..."
    cd "$KERNEL_SRC"
    make clean
    [ -d "$BUILD_DIR" ] && rm -rf "$BUILD_DIR"
    echo "✓ Clean completed"
}

# Main function
main() {
    echo "This script will build Linux kernel 6.14.x for development use."
    echo "Build location: $KERNEL_SRC"
    echo "Output directory: $BUILD_DIR"
    echo "Parallel jobs: $JOBS"
    echo ""
    echo "Options:"
    echo "  --clean    Clean previous build artifacts first"
    echo "  --config   Only create configuration, don't build"
    echo ""
    
    # Parse command line arguments
    CLEAN_FIRST=false
    CONFIG_ONLY=false
    
    for arg in "$@"; do
        case $arg in
            --clean)
                CLEAN_FIRST=true
                ;;
            --config)
                CONFIG_ONLY=true
                ;;
            --help|-h)
                echo "Usage: $SCRIPT_NAME [--clean] [--config] [--help]"
                exit 0
                ;;
        esac
    done
    
    echo "Continue with kernel build? [Y/n]"
    read -r response
    if [[ "$response" =~ ^[Nn]$ ]]; then
        echo "Build cancelled."
        exit 0
    fi
    
    # Execute build steps
    check_prerequisites
    
    if [ "$CLEAN_FIRST" = true ]; then
        clean_build
    fi
    
    create_config
    
    if [ "$CONFIG_ONLY" = true ]; then
        echo "Configuration completed. Run without --config to build."
        exit 0
    fi
    
    build_kernel
    show_summary
    
    # Get kernel version for installation prompt
    cd "$KERNEL_SRC"
    VERSION=$(grep "^VERSION" Makefile | cut -d'=' -f2 | tr -d ' ')
    PATCHLEVEL=$(grep "^PATCHLEVEL" Makefile | cut -d'=' -f2 | tr -d ' ')
    SUBLEVEL=$(grep "^SUBLEVEL" Makefile | cut -d'=' -f2 | tr -d ' ')
    KERNEL_VER="$VERSION.$PATCHLEVEL.$SUBLEVEL"
    
    echo ""
    echo "🎉 Kernel build completed successfully!"
    echo ""
    echo "📦 Build Summary:"
    echo "   Kernel version: $KERNEL_VER"
    echo "   Build artifacts: $BUILD_DIR"
    echo "   Ready for: Kernel module development"
    echo ""
    
    # Prompt for system installation
    echo "═══════════════════════════════════════════════════════════════"
    echo "                    KERNEL INSTALLATION OPTION"
    echo "═══════════════════════════════════════════════════════════════"
    echo ""
    echo "Your kernel has been built successfully and is ready for use!"
    echo ""
    echo "📁 Current status:"
    echo "   • Build artifacts are saved in: $BUILD_DIR"
    echo "   • Kernel is ready for module development and testing"
    echo "   • No system files have been modified"
    echo ""
    echo "🚀 Installation options:"
    echo "   1. Keep artifacts only (RECOMMENDED for coursework)"
    echo "      - Safe for learning and module development"
    echo "      - No risk to your current system"
    echo "      - Use built kernel headers for external modules"
    echo ""
    echo "   2. Install to system (ADVANCED - requires reboot)"
    echo "      - Installs kernel to /boot/ and updates GRUB"
    echo "      - Allows booting into your custom kernel"
    echo "      - Requires root privileges and system reboot"
    echo "      - ⚠️  RISK: Could make system unbootable if misconfigured"
    echo ""
    
    echo "Do you want to install this kernel to your system? [y/N]"
    echo "(Press Enter for default: No - keep artifacts only)"
    read -r install_response
    
    if [[ "$install_response" =~ ^[Yy]$ ]]; then
        install_kernel "$KERNEL_VER"
        if [ $? -eq 0 ]; then
            update_grub "$KERNEL_VER"
        fi
    else
        echo ""
        echo "✅ Kernel build completed - artifacts preserved for development"
        echo ""
        echo "💡 Development workflow:"
        echo "   • Use kernel headers: $BUILD_DIR/headers/"
        echo "   • Test modules against built kernel configuration"
        echo "   • Build artifacts remain in: $BUILD_DIR"
        echo ""
        echo "🔧 To install later, run:"
        echo "   sudo cp $BUILD_DIR/vmlinuz-kernpro-$KERNEL_VER /boot/"
        echo "   sudo cp $BUILD_DIR/System.map-kernpro-$KERNEL_VER /boot/"
        echo "   sudo cp -r $BUILD_DIR/modules/lib/modules/$KERNEL_VER /lib/modules/"
        echo "   sudo update-grub  # (or equivalent for your distribution)"
    fi
    
    echo ""
    echo "🎓 Ready for kernel module development with 6.14.x!"
}

# Trap to clean up on interrupt
trap 'echo ""; echo "Build interrupted. Run with --clean to start fresh."; exit 1' INT TERM

# Run main function
main "$@"
