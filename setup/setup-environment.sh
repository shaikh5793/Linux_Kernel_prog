#!/bin/bash
#
# Comprehensive Environment Setup for Kernpro Coursework
# Installs prerequisites and clones kernel 6.14 source
# Copyright (c) 2025 Techveda
#
# Licensed under MIT License
#

set -e

SCRIPT_NAME=$(basename $0)
KERNEL_VERSION="6.14"
CLONE_DIR="$HOME/linux-kernel-6.14"

echo "=== Kernpro Environment Setup ==="
echo "Setting up complete development environment for kernel 6.14.x programming..."
echo ""

# Function to detect distribution
detect_distro() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        echo "$NAME"
    else
        echo "Unknown"
    fi
}

# Function to install packages based on distribution
install_packages() {
    local os="$1"
    
    echo "Installing packages for $os..."
    
    if [[ "$os" == *"Ubuntu"* ]] || [[ "$os" == *"Debian"* ]]; then
        sudo apt update
        
        # Essential build tools
        sudo apt install -y \
            build-essential \
            gcc \
            make \
            git \
            wget \
            curl \
            vim \
            tree \
            bc \
            bison \
            flex \
            libssl-dev \
            libelf-dev \
            libncurses5-dev \
            dwarves \
            rsync \
            kmod \
            cpio \
            linux-headers-$(uname -r) \
            gdb \
            strace \
            binutils \
            sparse
            
    elif [[ "$os" == *"Red Hat"* ]] || [[ "$os" == *"CentOS"* ]] || [[ "$os" == *"Fedora"* ]]; then
        if command -v dnf &> /dev/null; then
            PKG_MGR="dnf"
        else
            PKG_MGR="yum"
        fi
        
        sudo $PKG_MGR groupinstall -y "Development Tools"
        sudo $PKG_MGR install -y \
            kernel-devel \
            kernel-headers \
            git \
            wget \
            curl \
            vim \
            tree \
            bc \
            bison \
            flex \
            openssl-devel \
            elfutils-libelf-devel \
            ncurses-devel \
            dwarves \
            rsync \
            gdb \
            strace \
            binutils \
            sparse
            
    elif [[ "$os" == *"Arch"* ]]; then
        sudo pacman -Syu --noconfirm
        sudo pacman -S --noconfirm \
            base-devel \
            linux-headers \
            git \
            wget \
            curl \
            vim \
            tree \
            bc \
            bison \
            flex \
            openssl \
            libelf \
            ncurses \
            pahole \
            rsync \
            gdb \
            strace \
            binutils \
            sparse
    else
        echo "Unsupported distribution: $os"
        echo "Please install development tools manually and re-run this script"
        exit 1
    fi
}

# Function to clone kernel source
clone_kernel_source() {
    echo ""
    echo "=== Cloning Kernel 6.14 Source ==="
    
    if [ -d "$CLONE_DIR" ]; then
        echo "Kernel source directory already exists at: $CLONE_DIR"
        echo "Do you want to update it? [y/N]"
        read -r response
        if [[ "$response" =~ ^[Yy]$ ]]; then
            cd "$CLONE_DIR"
            echo "Updating existing kernel source..."
            git fetch --all --tags
            git checkout v$KERNEL_VERSION 2>/dev/null || {
                echo "Kernel $KERNEL_VERSION not found, checking out latest stable..."
                git checkout $(git tag -l "v6.14*" | sort -V | tail -1)
            }
        fi
    else
        echo "Cloning stable kernel source to: $CLONE_DIR"
        echo "(This may take 10-30 minutes depending on connection speed)"
        
        # Clone with shallow history for faster download
        git clone --depth 1 --branch v$KERNEL_VERSION \
            https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git \
            "$CLONE_DIR" 2>/dev/null || {
            
            # If specific version not found, clone latest and checkout
            echo "Specific version not found, cloning latest stable..."
            git clone --depth 50 \
                https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git \
                "$CLONE_DIR"
            
            cd "$CLONE_DIR"
            # Find and checkout latest 6.14.x version
            LATEST_614=$(git tag -l "v6.14*" | sort -V | tail -1)
            if [ -n "$LATEST_614" ]; then
                echo "Checking out latest 6.14.x: $LATEST_614"
                git checkout "$LATEST_614"
            else
                echo "No 6.14.x version found, staying on current branch"
            fi
        }
        
        cd "$CLONE_DIR"
        echo "Creating development branch: kernpro-dev"
        git checkout -b kernpro-dev 2>/dev/null || git checkout kernpro-dev
    fi
    
    echo "Kernel source ready at: $CLONE_DIR"
}

# Function to setup vim with markdown support
setup_vim_markdown() {
    echo ""
    echo "=== Setting Up Vim Markdown Support ==="
    
    # Check if vim is installed
    if ! command -v vim &> /dev/null; then
        echo "✗ Vim not found, skipping vim setup"
        return 1
    fi
    
    echo "Configuring vim with markdown plugin support..."
    
    # Create vim directories if they don't exist
    mkdir -p ~/.vim/autoload ~/.vim/plugged
    
    # Install vim-plug if not already installed
    if [ ! -f ~/.vim/autoload/plug.vim ]; then
        echo "Installing vim-plug plugin manager..."
        curl -fLo ~/.vim/autoload/plug.vim --create-dirs \
            https://raw.githubusercontent.com/junegunn/vim-plug/master/plug.vim
        echo "✓ vim-plug installed"
    else
        echo "✓ vim-plug already installed"
    fi
    
    # Backup existing .vimrc if it exists
    if [ -f ~/.vimrc ]; then
        echo "Backing up existing .vimrc to ~/.vimrc.backup"
        cp ~/.vimrc ~/.vimrc.backup
    fi
    
    # Create .vimrc with markdown support
    echo "Creating .vimrc with markdown plugin configuration..."
    cat > ~/.vimrc << 'EOF'
" Plugin management with vim-plug
call plug#begin('~/.vim/plugged')

" Color schemes
Plug 'joshdick/onedark.vim'
Plug 'morhetz/gruvbox'
Plug 'danilo-augusto/vim-afterglow'

" Markdown support with GitHub-style formatting
Plug 'preservim/vim-markdown'

call plug#end()

" Markdown plugin configuration (automatic GitHub-style formatting)
let g:vim_markdown_folding_disabled = 1
let g:vim_markdown_frontmatter = 1
let g:vim_markdown_math = 1
let g:vim_markdown_strikethrough = 1
let g:vim_markdown_auto_insert_bullets = 0
let g:vim_markdown_new_list_item_indent = 0
let g:vim_markdown_emphasis_multiline = 0

" Ensure markdown files get proper syntax highlighting
autocmd BufNewFile,BufRead *.md set filetype=markdown
autocmd FileType markdown syntax on

" Disable line numbers to prevent copying them with text
set nonumber

" Other useful settings
set autoindent
set tabstop=4
set shiftwidth=4
set expandtab
set textwidth=80
set formatoptions+=t

" Enable syntax highlighting
syntax enable
filetype plugin on

" Show matching brackets
set showmatch

" Enable mouse support and clipboard
set clipboard=unnamedplus

" Enable 24-bit RGB color support
if (has("nvim"))
  let $NVIM_TUI_ENABLE_TRUE_COLOR=1
endif
if (has("termguicolors"))
  set termguicolors
endif

" Set background to dark
set background=dark

" Set Afterglow color scheme (if available)
silent! colorscheme afterglow
EOF
    
    echo "✓ .vimrc configured with markdown support"
    
    # Install plugins
    echo "Installing vim plugins (this may take a moment)..."
    vim +PlugInstall +qall 2>/dev/null || {
        echo "Note: Plugin installation may complete in background"
    }
    
    echo "✓ Vim markdown setup complete"
    echo "  • Open any .md file with 'vim filename.md' to see automatic formatting"
    echo "  • Reference documentation: vim reference/kbuild.md"
}

# Function to verify installation
verify_installation() {
    echo ""
    echo "=== Verifying Installation ==="
    
    # Check kernel version
    KERNEL_VER=$(uname -r)
    echo "Running kernel: $KERNEL_VER"
    
    # Check for kernel headers
    if [ -d "/lib/modules/$KERNEL_VER/build" ]; then
        echo "✓ Kernel headers found"
    else
        echo "✗ Kernel headers missing for $KERNEL_VER"
    fi
    
    # Check essential tools
    TOOLS=("gcc" "make" "git" "flex" "bison" "bc")
    for tool in "${TOOLS[@]}"; do
        if command -v $tool &> /dev/null; then
            echo "✓ $tool installed"
        else
            echo "✗ $tool missing"
        fi
    done
    
    # Check kernel source
    if [ -d "$CLONE_DIR" ]; then
        echo "✓ Kernel source available at $CLONE_DIR"
        if [ -f "$CLONE_DIR/Makefile" ]; then
            KERNEL_SRC_VER=$(grep "^VERSION\|^PATCHLEVEL\|^SUBLEVEL" "$CLONE_DIR/Makefile" | head -3 | cut -d'=' -f2 | tr -d ' ' | tr '\n' '.' | sed 's/.$//')
            echo "  Source version: $KERNEL_SRC_VER"
        fi
    else
        echo "✗ Kernel source not found"
    fi
}

# Main execution
main() {
    echo "This script will:"
    echo "1. Install development packages and kernel headers"
    echo "2. Clone Linux kernel 6.14.x source code to $CLONE_DIR"
    echo "3. Set up development branch for coursework"
    echo "4. Configure vim with markdown support for reference docs"
    echo ""
    echo "Continue? [Y/n]"
    read -r response
    if [[ "$response" =~ ^[Nn]$ ]]; then
        echo "Setup cancelled."
        exit 0
    fi
    
    # Step 1: Install packages
    echo ""
    echo "=== Step 1: Installing Prerequisites ==="
    OS=$(detect_distro)
    echo "Detected OS: $OS"
    install_packages "$OS"
    
    # Step 2: Clone kernel source
    echo ""
    echo "=== Step 2: Setting Up Kernel Source ==="
    clone_kernel_source
    
    # Step 3: Setup vim markdown
    echo ""
    echo "=== Step 3: Setting Up Vim Markdown Support ==="
    setup_vim_markdown
    
    # Step 4: Verify
    verify_installation
    
    echo ""
    echo "=== Setup Complete ==="
    echo "Environment ready for kernel 6.14.x development!"
    echo ""
    echo "Next steps:"
    echo "1. Run ./build-kernel.sh to build the kernel (optional)"
    echo "2. Test module compilation: cd lkms/p1_base && make"
    echo "3. View reference docs: vim reference/kbuild.md (with markdown formatting)"
    echo "4. Explore coursework modules in various directories"
    echo ""
    echo "Kernel source location: $CLONE_DIR"
    echo "Use 'cd $CLONE_DIR' to work with kernel source directly"
    echo "Vim configured with markdown support for reference documentation"
}

# Run main function
main "$@"