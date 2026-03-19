# Kernpro - Linux Kernel Programming Coursework

Source code and utilities for Techveda online Linux kernel programming training sessions.

**Target Kernel Version: 6.14.x**

---

**Author:** Raghu Bharadwaj  
**Organization:** TechVeda  
**Contact:** < raghu-at-techveda-dot-org >  
**Copyright:** © 2020-2025 TechVeda. All rights reserved.

---

## Getting Started

### Step 1: Clone the Repository

```bash
# Clone with your GitLab credentials
git clone https://gitlab.com/techvedalive/lkp0725.git
cd lkp0725

# Create your working branch for exercises
git checkout -b my-work
```

### Step 2: Environment Setup

```bash
# Run comprehensive setup (installs packages + clones kernel 6.14 source)
./setup/setup-environment.sh

# Setup kernel source navigation (cscope, ctags, vim integration)
./setup/setup-navigation.sh

# Optional: Build custom kernel with development config (30-120 minutes)
./setup/build-kernel.sh
```

### Step 3: Test Basic Module

```bash
# Verify everything works
cd lkms/p1_base
make
sudo insmod hello.ko
dmesg | tail
sudo rmmod hello
```

## Git Workflow for Students

**Important:** Always work on your personal branch to avoid conflicts with course updates.

```bash
# Create and work on your branch
git checkout -b my-work              # Create working branch
git add .                            # Stage your changes
git commit -m "Add my solutions"     # Commit your work

# Get course updates periodically
git checkout main                    # Switch to main branch
git pull origin main                 # Pull latest updates
git checkout my-work                 # Back to your branch
git merge main                       # Merge updates (resolve conflicts if any)
```

**Tips:**
- Keep `main` branch clean for receiving updates
- Use `my-work` branch for all exercises and modifications
- Commit frequently to save your progress
- Pull updates weekly to get latest fixes/content

## Repository Structure

This coursework covers hands-on kernel programming with:

- **Loadable Kernel Modules** - Module basics, parameters, build systems
- **Memory Management** - Page allocation, caches, DMA, virtual memory  
- **Synchronization** - Locks, semaphores, RCU, concurrent programming
- **Scheduling & Timing** - Process scheduling, timers, delays
- **Debugging Tools** - KASAN, OOPS analysis, debugging techniques
- **System Interfaces** - proc/sys filesystems, user-kernel communication

## Prerequisites

- Linux system with kernel 6.14.x compatible headers
- Build tools: gcc, make, git, kernel development packages
- Root/sudo access for module loading
- GitLab account for repository access

## Safety Guidelines

**⚠️ CRITICAL: BACKUP YOUR SYSTEM BEFORE TESTING**

1. **Use Virtual Machines**: Always test kernel modules in VMs first
2. **Non-Production Systems**: Never test on critical production machines  
3. **Incremental Testing**: Test one module at a time
4. **Recovery Plan**: Know how to boot from rescue media

## Setup Scripts

Located in the `setup/` directory, these foundational scripts prepare your development environment:

- **`setup/setup-environment.sh`** - Complete environment setup (packages + kernel source)
- **`setup/setup-navigation.sh`** - Kernel source navigation (cscope, ctags, vim integration)
- **`setup/build-kernel.sh`** - Advanced kernel build with optional system installation
- **Module Makefiles** - Individual module compilation in each directory

### Kernel Build Process

The `setup/build-kernel.sh` script provides a comprehensive kernel build experience:

#### Build Stages (with detailed progress)
1. **📦 Stage 1**: Kernel image compilation (bzImage)
2. **🔧 Stage 2**: Kernel modules compilation (.ko files)
3. **📋 Stage 3**: Headers installation for external modules
4. **📁 Stage 4**: Build artifacts packaging and organization

#### Installation Options
- **Option 1: Development Only (Recommended for coursework)**
  - Keeps build artifacts in `~/linux-kernel-6.14/build-output/`
  - Safe for learning and module development
  - No system modifications or risks
  - Use built headers for external module compilation

- **Option 2: System Installation (Advanced users)**
  - Installs kernel to `/boot/` and updates GRUB
  - Requires root privileges and system reboot
  - Updates initramfs and boot configuration
  - ⚠️ **Risk**: Could affect system boot if misconfigured

#### Build Features
```bash
# Build with verbose progress tracking
./setup/build-kernel.sh

# Available options:
./setup/build-kernel.sh --clean    # Clean previous builds first
./setup/build-kernel.sh --config   # Create configuration only
./setup/build-kernel.sh --help     # Show usage information
```

#### Multi-Distribution Support
- **Package detection**: Ubuntu/Debian, RHEL/CentOS/Fedora, Arch Linux
- **Native architecture**: Automatic detection (x86_64, ARM64, ARM, etc.)
- **Initramfs tools**: update-initramfs, dracut, mkinitcpio
- **GRUB configuration**: Distribution-specific update commands

## Development Tools

After running `./setup/setup-navigation.sh`, you'll have powerful kernel source navigation:

### Vim Navigation
```bash
# Start vim with kernel navigation configured
cd /path/to/coursework && vim

# Key bindings:
Ctrl-\s     # Find symbol definition
Ctrl-\g     # Find global definition  
Ctrl-\c     # Find functions calling this function
Ctrl-\d     # Find functions called by this function
Ctrl-]      # Jump to definition (ctags)
Ctrl-T      # Jump back
F5          # Refresh cscope database
```

### Command Line Tools
```bash
# Search kernel source
./kernel-search.sh kmalloc symbol
./kernel-search.sh schedule global
./kernel-search.sh "memory allocation" text

# Update navigation databases
./update-navigation.sh
```

## Troubleshooting

**Common Issues:**
- Missing kernel headers: Run `sudo apt install linux-headers-$(uname -r)`
- Permission denied: Use `sudo` for module operations (`insmod`, `rmmod`)
- Build failures: Ensure all prerequisites installed via setup script
- Git conflicts: Work on `my-work` branch, merge `main` carefully
- Kernel build errors: Check certificate signing is disabled in config
- Boot issues: Keep original kernel as backup, use GRUB recovery

**Kernel Build Troubleshooting:**
- **Build fails with certificate errors**: Script automatically disables module signing
- **Long build times**: Use `./build-kernel.sh --clean` for fresh start
- **Architecture mismatch**: Script auto-detects native architecture
- **GRUB not updating**: Check distribution-specific GRUB tools are installed
- **Module loading fails**: Ensure `depmod` ran successfully during installation

**Getting Help:**
- Check module-specific documentation in each directory
- Review build logs for specific error messages
- Ensure kernel version compatibility (6.14.x recommended)
- For kernel build issues, check `~/linux-kernel-6.14/build-output/` for artifacts

## Legal Notice

**Licensing:**
- **Source code**: Dual MIT/GPL v2 License (choose either) - See [LICENSE](LICENSE) file
- **Courseware PDFs**: Confidential, non-distributable, for licensed participants only
- **Training materials**: Proprietary content of TechVeda

**Disclaimer:**
This code is provided "AS IS" without warranty. These kernel modules are experimental and designed for educational purposes. Before using any source code in commercial projects or products, consult your organization's legal staff for compliance verification.

**Safety Notice:**
Kernel-level programming can affect system stability. Always test in virtual machines or non-production environments. Maintain system backups before installing custom kernels.

---

## About the Author

**Raghu Bharadwaj** is a Linux kernel developer and trainer specializing in system-level programming education. He conducts online and in-person training sessions on Linux kernel internals, device drivers, and embedded systems programming.

*"When you don't create things, you become defined by your tastes rather than ability. Your tastes only narrow & exclude people. So create."*

---

**Copyright © 2020-2025 TechVeda. All rights reserved.**

**Contact:** < raghu-at-techveda-dot-org >  
**Organization:** [TechVeda](https://techveda.org)  
**Repository:** Kernpro Linux Kernel Programming Coursework
