# Linux Kernel Build Guide (Updated for Kernel 6.14+)

---
**Author**: Raghu Bharadwaj  
**Organization**: TechVeda  
**License**: MIT License  
**Repository**: Linux Kernel Programming Training Materials  
---

## Table of Contents
1. [Prerequisites](#prerequisites)
2. [Tool Installation](#tool-installation)
3. [Kernel Source Setup](#kernel-source-setup)
4. [Kernel Configuration](#kernel-configuration)
5. [Build Process](#build-process)
6. [Installation](#installation)
7. [Important Build Tools in scripts/](#important-build-tools-in-scripts)
8. [Module Development](#module-development)
9. [Cross-compilation](#cross-compilation)
10. [Debugging and Testing](#debugging-and-testing)
11. [Troubleshooting](#troubleshooting)
12. [References](#references)

## Prerequisites

Before building the Linux kernel, ensure you have:
- At least 20GB of free disk space
- 8GB+ RAM (16GB+ recommended for parallel builds)
- Git installed for source code management
- Understanding of your target architecture (x86_64, ARM64, etc.)

## Tool Installation

### Ubuntu/Debian (22.04+ LTS)
```bash
sudo apt update
sudo apt install build-essential libncurses-dev libssl-dev git \
    exuberant-ctags cscope flex bison bc libelf-dev dwarves \
    python3-dev python3-setuptools rsync kmod cpio
```

### Fedora/RHEL (38+)
```bash
sudo dnf groupinstall "Development Tools"
sudo dnf install git ctags ncurses-devel openssl-devel flex bison \
    bc elfutils-libelf-devel dwarves python3-devel rsync kmod
```

### Arch Linux
```bash
sudo pacman -S base-devel git ncurses openssl flex bison bc \
    libelf pahole python rsync kmod cpio
```

## Kernel Source Setup

### 1. Clone Latest Stable Kernel (6.14+)
```bash
mkdir ~/kernel-sources
cd ~/kernel-sources
git clone https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git
cd linux
```

### 2. Select Kernel Version
Find the latest stable version:
```bash
git tag -l | grep -E "^v6\.(1[4-9]|[2-9][0-9])\." | tail -10
```

Checkout the latest stable release:
```bash
git checkout -b mybuild v6.14.7  # Replace with latest version
```

### 3. Clean Previous Builds (if any)
```bash
make mrproper
```

## Kernel Configuration

### 1. Base Configuration Options

**Use distribution config as starting point:**
```bash
cp /boot/config-$(uname -r) .config
make olddefconfig
```

**Or create default config:**
```bash
make defconfig        # Minimal default config
make allmodconfig     # Build most features as modules
```

### 2. Interactive Configuration
```bash
make menuconfig       # Text-based interface
make xconfig         # Qt-based GUI (requires qt5-dev)
make gconfig         # GTK-based GUI (requires libgtk-3-dev)
```

### 3. Disable Security Certificates (for custom builds)
```bash
scripts/config --disable SYSTEM_TRUSTED_KEYS
scripts/config --disable SYSTEM_REVOCATION_KEYS
scripts/config --disable MODULE_SIG_KEY
```

### 4. Common Performance/Debug Options
```bash
# Performance optimizations
scripts/config --enable CONFIG_CC_OPTIMIZE_FOR_PERFORMANCE
scripts/config --disable CONFIG_DEBUG_INFO

# Development/debugging (increases build time and size)
scripts/config --enable CONFIG_DEBUG_KERNEL
scripts/config --enable CONFIG_DEBUG_INFO_DWARF_TOOLCHAIN_DEFAULT
scripts/config --enable CONFIG_FRAME_POINTER
```

## Build Process

### 1. Determine Build Parallelism
```bash
JOBS=$(nproc)  # Use all CPU cores
# Or use fewer cores to leave system responsive
JOBS=$(($(nproc) - 2))
```

### 2. Full Kernel Build
```bash
make -j${JOBS}
```

### Build Process Overview:
1. **Configuration Processing**: Parse `.config` file
2. **Dependency Resolution**: Identify source files for selected features
3. **Compilation**: Compile source files to object files (`.o`) using kbuild
4. **Static Linking**: Link built-in objects into `vmlinux` (raw ELF kernel)
5. **Architecture Processing**: Create bootable image using arch-specific Makefile:
   - Append architecture-specific bootstrap code
   - Enable CPU MMU and set up page tables
   - Package into bootloader format (bzImage/uImage/zImage)
6. **Module Building**: Build loadable modules (`.ko`) from `=m` config entries

### Generated Files:
- `vmlinux`: Raw ELF kernel image with symbols (source root)
- `vmlinuz`: Compressed kernel image (arch/$ARCH/boot/)
- `bzImage/uImage/zImage`: Bootable kernel image (arch/$ARCH/boot/)
- `*.ko`: Loadable kernel modules (throughout source tree)


### 3. Build Only Modules
```bash
make -j${JOBS} modules
```

### 4. Build Specific Module
```bash
make -j${JOBS} M=drivers/usb/storage
```

## Installation

### 1. Install Kernel Modules
```bash
sudo make modules_install
```
This installs modules to `/lib/modules/$(kernel-version)/`

### 2. Install Kernel Image
```bash
sudo make install
```
This:
- Copies kernel image and related files to `/boot/`
- Updates bootloader configuration (GRUB/systemd-boot)
- Creates initramfs if needed

### 3. Update Bootloader (if needed)
```bash
# For GRUB
sudo update-grub

# For systemd-boot
sudo bootctl update
```

### Kernel Build Process Diagram:

```
                    Linux Kernel Build Process Flow
                   ====================================

    .config          Kconfig          defconfig/menuconfig
       |                |                      |
       v                v                      v
   ┌─────────────────────────────────────────────────────────┐
   │            Configuration Processing                     │
   │     Parse .config, resolve dependencies & conflicts    │
   └─────────────────────┬───────────────────────────────────┘
                         │
                         v
   ┌─────────────────────────────────────────────────────────┐
   │          Dependency Resolution (Kbuild)                │
   │   scripts/kconfig/* → identify source files needed     │
   │   Generate include/generated/autoconf.h                │
   └─────────────────────┬───────────────────────────────────┘
                         │
          ┌──────────────┴──────────────┐
          │                             │
          v                             v
  ┌─────────────────┐         ┌─────────────────┐
  │ Built-in (=y)   │         │ Modules (=m)    │
  │ Compilation     │         │ Compilation     │
  │                 │         │                 │
  │ .c → .o files   │         │ .c → .o files   │
  │ (via gcc/clang) │         │ (with -DMODULE) │
  └─────────┬───────┘         └─────────┬───────┘
            │                           │
            v                           │
  ┌─────────────────────────────────────┐│
  │     Static Linking (ld)             ││
  │  Link all built-in objects          ││
  │  *.o → vmlinux (raw ELF)            ││
  │  + System.map (symbol table)        ││
  └─────────────────┬───────────────────┘│
                    │                    │
                    v                    │
  ┌─────────────────────────────────────┐│
  │   Architecture-Specific Processing  ││
  │                                     ││
  │ arch/*/boot/Makefile:               ││
  │ ├─ Add bootstrap code               ││
  │ ├─ Setup MMU/page tables            ││
  │ ├─ Compress vmlinux → vmlinuz       ││
  │ └─ Create bootloader format:        ││
  │    • x86: bzImage                   ││
  │    • ARM: uImage/zImage             ││
  │    • RISC-V: Image.gz               ││
  └─────────────────┬───────────────────┘│
                    │                    │
                    v                    v
  ┌─────────────────────────────────────────────────────────┐
  │                Final Outputs                            │
  │                                                         │
  │ Kernel Images:          Loadable Modules:               │
  │ ├─ vmlinux (raw ELF)    ├─ drivers/**/*.ko              │
  │ ├─ vmlinuz (compressed) ├─ fs/**/*.ko                   │
  │ └─ bzImage (bootable)   ├─ net/**/*.ko                  │
  │                         └─ crypto/**/*.ko               │
  │ Support Files:                                          │
  │ ├─ System.map          Module Dependencies:             │
  │ ├─ .config             ├─ modules.dep                   │
  │ └─ Module.symvers      └─ modules.symbols               │
  └─────────────────────────────────────────────────────────┘

                          Installation Phase
                         ==================
                                 │
               ┌─────────────────┴─────────────────┐
               │                                   │
               v                                   v
    ┌─────────────────────┐            ┌─────────────────────┐
    │ make modules_install│            │    make install     │
    │                     │            │                     │
    │ Copy *.ko files to: │            │ Copy to /boot/:     │
    │ /lib/modules/       │            │ ├─ vmlinuz-X.Y.Z    │
    │ $(kernel-version)/  │            │ ├─ System.map-X.Y.Z │
    │                     │            │ ├─ config-X.Y.Z     │
    │ Generate:           │            │ └─ initramfs        │
    │ ├─ modules.dep      │            │                     │
    │ ├─ modules.symbols  │            │ Update bootloader:  │
    │ └─ modules.alias    │            │ └─ GRUB/systemd-boot│
    └─────────────────────┘            └─────────────────────┘

    Legend:
    ======
    =y : Built into kernel (vmlinux)    .o  : Object files
    =m : Built as loadable module       .ko : Kernel module
    =n : Not built                      ELF : Executable & Linkable Format
```





## Integrating Custom Services into Kernel Tree

This section outlines the steps needed to integrate a custom kernel service or subsystem into the Linux kernel build system and source tree structure.

### Key Integration Steps Summary

1. **Plan and Design**: Define service architecture, choose integration location, and identify dependencies
2. **Organize Source Code**: Create directory structure and organize implementation files
3. **Configure Build System**: Add Kconfig entries and integrate with Makefile system

### System Integration

1. **Manage Headers**: Organize public and private header files appropriately
2. **Integrate with Kernel**: Hook into initialization sequence and implement interfaces
3. **Export Symbols**: Make functions available to other kernel components and modules
4. **Document and Test**: Create comprehensive documentation and test coverage
5. **Ensure Quality**: Follow coding standards and conduct thorough reviews
6. **Plan Maintenance**: Establish long-term support and community integration
7. **Validate Integration**: Complete final testing and checklist verification

---

### 1. Planning and Design Phase

#### 1.1 Define Service Architecture
- **Identify service purpose**: Define what your service does and why it belongs in kernel space
- **Determine subsystem category**: Decide whether it's a driver, filesystem, network protocol, security module, etc.
- **Plan kernel interfaces**: Define syscalls, proc/sysfs interfaces, or other kernel APIs
- **Consider dependencies**: Identify required kernel subsystems and configuration options
- **Design data structures**: Plan kernel data structures, locking mechanisms, and memory management

#### 1.2 Choose Integration Location
- **Core kernel services**: `kernel/` directory for core functionality
- **Device drivers**: `drivers/` with appropriate subdirectory (char, block, net, etc.)
- **Filesystems**: `fs/` directory with dedicated subdirectory
- **Network protocols**: `net/` directory structure
- **Architecture-specific**: `arch/*/` for architecture-dependent code
- **Security modules**: `security/` directory for security frameworks

### 2. Source Code Organization

#### 2.1 Create Directory Structure
- **Main service directory**: Create primary directory under appropriate subsystem
- **Header files**: Organize public headers in `include/linux/` and private headers locally
- **Documentation**: Prepare documentation for `Documentation/` tree
- **Test files**: Create test cases in `tools/testing/selftests/` if applicable

#### 2.2 Source File Organization
- **Core implementation**: Main service logic and data structures
- **Interface layer**: System call interfaces, proc/sysfs handlers
- **Platform support**: Architecture-specific implementations if needed
- **Helper functions**: Utility functions and common operations
- **Module interface**: If service can be built as module

### 3. Kconfig Integration

#### 3.1 Configuration Definition
- **Create Kconfig file**: Add `Kconfig` file in service directory
- **Define config options**: Create `CONFIG_YOUR_SERVICE` option with dependencies
- **Set tristate options**: Allow built-in (`y`), module (`m`), or disabled (`n`)
- **Add help text**: Provide comprehensive help describing the service
- **Define dependencies**: Specify required kernel features and architectures

#### 3.2 Parent Kconfig Integration
- **Update parent Kconfig**: Add `source "path/to/your/Kconfig"` in parent directory
- **Create menu structure**: Organize options in logical menu hierarchy
- **Set default values**: Define sensible defaults for different configurations
- **Add expert options**: Hide advanced options behind `CONFIG_EXPERT`

### 4. Makefile Integration

#### 4.1 Local Makefile Creation
- **Object file specification**: Define `obj-$(CONFIG_YOUR_SERVICE) += your-service.o`
- **Multi-file objects**: Use `your-service-y += file1.o file2.o` for multiple files
- **Conditional compilation**: Add conditional objects based on config options
- **Subdirectory handling**: Use `obj-$(CONFIG_OPTION) += subdir/` for subdirectories
- **Build flags**: Set specific CFLAGS or compiler options if needed

#### 4.2 Parent Makefile Updates
- **Add subdirectory**: Include `obj-$(CONFIG_YOUR_SERVICE) += your-service/` in parent Makefile
- **Build ordering**: Ensure proper build order with dependencies
- **Link requirements**: Specify any special linking requirements

### 5. Header File Integration

#### 5.1 Public Header Files
- **Create include files**: Add headers to `include/linux/` for public interfaces
- **API definitions**: Define function prototypes, data structures, and constants
- **Conditional compilation**: Use `#ifdef CONFIG_YOUR_SERVICE` guards
- **Documentation**: Add kernel-doc comments for all public functions
- **Version compatibility**: Plan for API evolution and compatibility

#### 5.2 Internal Headers
- **Private definitions**: Keep internal structures in local header files
- **Module interfaces**: Define interfaces between service components
- **Architecture abstraction**: Create architecture-independent interfaces

### 6. System Integration Points

#### 6.1 Kernel Initialization
- **Early initialization**: Add service initialization to appropriate init sequence
- **Subsystem registration**: Register with relevant kernel subsystems
- **Resource allocation**: Initialize memory pools, caches, and data structures
- **Error handling**: Implement proper cleanup on initialization failure

#### 6.2 Interface Implementation
- **System calls**: Register new system calls if needed (update syscall tables)
- **Proc filesystem**: Create entries in `/proc` for runtime information
- **Sysfs integration**: Add sysfs entries for configuration and status
- **Device nodes**: Create character or block device interfaces if applicable
- **Network protocols**: Register with network stack if providing protocol support

### 7. Build System Integration

#### 7.1 Symbol Exports
- **Export symbols**: Use `EXPORT_SYMBOL()` for functions used by modules
- **Symbol versioning**: Consider `EXPORT_SYMBOL_GPL()` for GPL-only interfaces
- **Module dependencies**: Ensure proper module loading dependencies

#### 7.2 Build Testing
- **Configuration testing**: Test with `allyesconfig`, `allnoconfig`, and `allmodconfig`
- **Cross-compilation**: Verify builds on different architectures
- **Module building**: Test both built-in and module configurations
- **Dependency resolution**: Ensure all dependencies are properly declared

### 8. Documentation and Testing

#### 8.1 Documentation Requirements
- **Kernel documentation**: Add RST files to `Documentation/` tree
- **API documentation**: Document all public interfaces with kernel-doc
- **Configuration help**: Provide comprehensive Kconfig help text
- **Usage examples**: Include usage examples and best practices

#### 8.2 Testing Integration
- **Self-tests**: Create tests in `tools/testing/selftests/`
- **Unit tests**: Implement kernel unit tests if applicable
- **Integration tests**: Test interaction with other kernel subsystems
- **Performance tests**: Benchmark critical paths and resource usage

### 9. Code Quality and Compliance

#### 9.1 Coding Standards
- **Follow kernel style**: Adhere to Linux kernel coding style guidelines
- **Use checkpatch**: Run `scripts/checkpatch.pl` on all code
- **Sparse checking**: Use sparse for additional static analysis
- **Coccinelle**: Run semantic patches to catch common issues

#### 9.2 Review Process
- **Internal review**: Conduct thorough internal code review
- **Community review**: Prepare for kernel community review process
- **Maintainer coordination**: Work with relevant subsystem maintainers
- **Patch series**: Organize changes into logical patch series

### 10. Maintenance Considerations

#### 10.1 Long-term Support
- **API stability**: Design stable APIs for userspace interfaces
- **Backward compatibility**: Plan for maintaining compatibility
- **Performance monitoring**: Include performance metrics and monitoring
- **Error reporting**: Implement comprehensive error reporting and logging

#### 10.2 Community Integration
- **Maintainer assignment**: Designate maintainers for the new service
- **Mailing list**: Subscribe to relevant kernel mailing lists
- **Bug tracking**: Set up bug reporting and tracking mechanisms
- **Release planning**: Coordinate with kernel release cycles

### Final Integration Checklist

- [ ] **Architecture defined**: Service purpose and design documented
- [ ] **Directory structure**: Source code organized in appropriate kernel tree location
- [ ] **Kconfig entries**: Configuration options defined with proper dependencies
- [ ] **Makefile integration**: Build system properly integrated at all levels
- [ ] **Headers organized**: Public and private headers properly separated
- [ ] **Initialization**: Service initialization integrated into kernel boot sequence
- [ ] **Interfaces implemented**: System calls, proc/sysfs, or device interfaces created
- [ ] **Symbols exported**: Required symbols exported for module usage
- [ ] **Documentation written**: Comprehensive documentation in Documentation/ tree
- [ ] **Tests created**: Self-tests and validation tests implemented
- [ ] **Code quality**: checkpatch, sparse, and coccinelle checks passed
- [ ] **Build testing**: Tested with various kernel configurations
- [ ] **Review completed**: Code review process completed
- [ ] **Maintainer assigned**: Long-term maintenance plan established
