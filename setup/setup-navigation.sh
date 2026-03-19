#!/bin/bash
#
# Kernel Source Navigation Setup Script
# Initializes cscope, ctags, and vim integration for kernel development
# Copyright (c) 2025 Techveda
#
# Licensed under MIT License
#

set -e

SCRIPT_NAME=$(basename $0)
KERNEL_SRC="$HOME/linux-kernel-6.14"
COURSEWORK_DIR="$(pwd)"

echo "=== Kernel Source Navigation Setup ==="
echo "Setting up cscope, ctags, and vim integration for kernel development"
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
    
    # Check required tools
    MISSING_TOOLS=()
    
    if ! command -v cscope &> /dev/null; then
        MISSING_TOOLS+=("cscope")
    fi
    
    if ! command -v ctags &> /dev/null; then
        MISSING_TOOLS+=("ctags")
    fi
    
    if ! command -v vim &> /dev/null; then
        MISSING_TOOLS+=("vim")
    fi
    
    if [ ${#MISSING_TOOLS[@]} -gt 0 ]; then
        echo "✗ Missing required tools: ${MISSING_TOOLS[*]}"
        echo ""
        echo "Install missing tools:"
        echo "Ubuntu/Debian: sudo apt install cscope exuberant-ctags vim"
        echo "RHEL/CentOS:   sudo yum install cscope ctags vim"
        echo "Fedora:        sudo dnf install cscope ctags vim"
        echo "Arch:          sudo pacman -S cscope ctags vim"
        exit 1
    fi
    
    echo "✓ All required tools available"
}

# Function to setup cscope database
setup_cscope() {
    echo ""
    echo "=== Setting Up Cscope Database ==="
    
    cd "$KERNEL_SRC"
    
    # Remove existing cscope files
    rm -f cscope.* ncscope.*
    
    echo "Generating file list for cscope..."
    # Create file list focusing on C/H files and excluding generated/temporary files
    find . -path "./arch/*" -prune -o \
           -path "./tmp/*" -prune -o \
           -path "./Documentation/*" -prune -o \
           -path "./scripts/*" -prune -o \
           -path "./.tmp_versions/*" -prune -o \
           -path "./debian/*" -prune -o \
           -name "*.[chxsS]" -print > cscope.files
    
    # Add architecture-specific files (for current architecture)
    ARCH=$(uname -m)
    case "$ARCH" in
        x86_64|i386|i686)
            find ./arch/x86 -name "*.[chxsS]" >> cscope.files
            ;;
        aarch64|arm64)
            find ./arch/arm64 -name "*.[chxsS]" >> cscope.files
            ;;
        armv7l|armv6l)
            find ./arch/arm -name "*.[chxsS]" >> cscope.files
            ;;
        *)
            echo "Adding x86_64 architecture files as default"
            find ./arch/x86 -name "*.[chxsS]" >> cscope.files
            ;;
    esac
    
    # Add include files
    find ./include -name "*.h" >> cscope.files
    
    echo "Building cscope database (this may take 5-15 minutes)..."
    cscope -b -q -k
    
    echo "✓ Cscope database created successfully"
    echo "  Files indexed: $(wc -l < cscope.files)"
    echo "  Database size: $(du -h cscope.out | cut -f1)"
}

# Function to setup ctags
setup_ctags() {
    echo ""
    echo "=== Setting Up Ctags Database ==="
    
    cd "$KERNEL_SRC"
    
    # Remove existing tags file
    rm -f tags
    
    echo "Generating ctags database..."
    
    # Create comprehensive ctags database
    ctags --c-kinds=+p --fields=+iaS --extra=+q \
          --exclude="*.tmp" \
          --exclude="*.o" \
          --exclude="*.ko" \
          --exclude="*.mod.c" \
          --exclude="Documentation/*" \
          --exclude="scripts/*" \
          --exclude="debian/*" \
          --exclude=".tmp_versions/*" \
          -R .
    
    echo "✓ Ctags database created successfully"
    echo "  Database size: $(du -h tags | cut -f1)"
}

# Function to setup vim configuration
setup_vim_config() {
    echo ""
    echo "=== Setting Up Vim Integration ==="
    
    # Create vim configuration for kernel development
    VIM_CONFIG="$HOME/.vimrc.kernel"
    
    cat > "$VIM_CONFIG" << 'EOF'
" Kernel Development Vim Configuration
" Add this to your ~/.vimrc or source it: :source ~/.vimrc.kernel

" Cscope settings
if has("cscope")
    " Use both cscope and ctag for 'ctrl-]', ':ta', and 'vim -t'
    set cscopetag
    
    " Check cscope for definition of a symbol before checking ctags
    set csto=0
    
    " Add cscope database if it exists
    if filereadable("cscope.out")
        cs add cscope.out
    endif
    
    " Show msg when any other cscope db added
    set cscopeverbose
    
    " Cscope key mappings
    nmap <C-\>s :cs find s <C-R>=expand("<cword>")<CR><CR>
    nmap <C-\>g :cs find g <C-R>=expand("<cword>")<CR><CR>
    nmap <C-\>c :cs find c <C-R>=expand("<cword>")<CR><CR>
    nmap <C-\>t :cs find t <C-R>=expand("<cword>")<CR><CR>
    nmap <C-\>e :cs find e <C-R>=expand("<cword>")<CR><CR>
    nmap <C-\>f :cs find f <C-R>=expand("<cfile>")<CR><CR>
    nmap <C-\>i :cs find i ^<C-R>=expand("<cfile>")<CR>$<CR>
    nmap <C-\>d :cs find d <C-R>=expand("<cword>")<CR><CR>
endif

" Kernel coding style
set tabstop=8
set shiftwidth=8
set softtabstop=8
set noexpandtab
set textwidth=80
set colorcolumn=80

" Show line numbers
set number
set relativenumber

" Highlight search results
set hlsearch
set incsearch

" Enable syntax highlighting
syntax on

" Show matching brackets
set showmatch

" File type detection
filetype plugin indent on

" Status line
set laststatus=2
set statusline=%F%m%r%h%w\ [FORMAT=%{&ff}]\ [TYPE=%Y]\ [POS=%l,%v][%p%%]\ %{strftime(\"%d/%m/%y\ -\ %H:%M\")}

" Quick navigation shortcuts
" F3: Toggle highlighting
map <F3> :set hlsearch!<CR>
" F4: Show/hide line numbers
map <F4> :set number!<CR>
" F5: Refresh cscope database
map <F5> :!cscope -Rb<CR>:cs reset<CR>

" Tag navigation
" Ctrl-] : Jump to tag
" Ctrl-T : Jump back
" gd     : Go to local declaration
" gD     : Go to global declaration

echo "Kernel development vim configuration loaded"
EOF

    echo "✓ Vim configuration created: $VIM_CONFIG"
    
    # Create a project-specific vimrc in coursework directory
    PROJ_VIMRC="$COURSEWORK_DIR/.vimrc"
    cat > "$PROJ_VIMRC" << EOF
" Project-specific vim configuration for Kernpro coursework
" This file is automatically loaded when you start vim in this directory

" Source kernel development configuration
source $VIM_CONFIG

" Set cscope database path to kernel source
if filereadable("$KERNEL_SRC/cscope.out")
    cs add $KERNEL_SRC/cscope.out $KERNEL_SRC
endif

" Set tags file path to kernel source
set tags+=$KERNEL_SRC/tags

" Add kernel source to path for file finding
set path+=$KERNEL_SRC
set path+=$KERNEL_SRC/include
set path+=$KERNEL_SRC/arch/x86/include

echo "Kernpro project configuration loaded"
echo "Kernel source: $KERNEL_SRC"
echo "Use Ctrl-\\ + key for cscope commands"
echo "Use Ctrl-] to jump to definitions"
EOF

    echo "✓ Project vim configuration created: $PROJ_VIMRC"
}

# Function to create helper scripts
create_helper_scripts() {
    echo ""
    echo "=== Creating Helper Scripts ==="
    
    # Create update script for refreshing databases
    cat > "$COURSEWORK_DIR/update-navigation.sh" << EOF
#!/bin/bash
# Quick script to update cscope and ctags databases
echo "Updating navigation databases..."
cd "$KERNEL_SRC"
echo "Rebuilding cscope database..."
cscope -Rb -q -k
echo "Rebuilding ctags database..."
ctags --c-kinds=+p --fields=+iaS --extra=+q \\
      --exclude="*.tmp" --exclude="*.o" --exclude="*.ko" \\
      --exclude="*.mod.c" --exclude="Documentation/*" \\
      --exclude="scripts/*" -R .
echo "✓ Navigation databases updated"
EOF
    
    chmod +x "$COURSEWORK_DIR/update-navigation.sh"
    echo "✓ Update script created: $COURSEWORK_DIR/update-navigation.sh"
    
    # Create search helper script
    cat > "$COURSEWORK_DIR/kernel-search.sh" << 'EOF'
#!/bin/bash
# Helper script for kernel source searching

KERNEL_SRC="$HOME/linux-kernel-6.14"

if [ $# -eq 0 ]; then
    echo "Usage: $0 <search_term> [search_type]"
    echo ""
    echo "Search types:"
    echo "  symbol  (s) - Find symbol definition"
    echo "  global  (g) - Find global definition"
    echo "  calls   (c) - Find functions calling this function"  
    echo "  called  (d) - Find functions called by this function"
    echo "  text    (t) - Find text string"
    echo "  file    (f) - Find file"
    echo "  include (i) - Find files including this file"
    echo ""
    echo "Examples:"
    echo "  $0 kmalloc symbol"
    echo "  $0 schedule global"
    echo "  $0 \"memory allocation\" text"
    exit 1
fi

TERM="$1"
TYPE="${2:-symbol}"

cd "$KERNEL_SRC"

case "$TYPE" in
    s|symbol)
        echo "Searching for symbol: $TERM"
        cscope -d -L -0 "$TERM"
        ;;
    g|global)
        echo "Searching for global definition: $TERM"
        cscope -d -L -1 "$TERM"
        ;;
    c|calls)
        echo "Finding functions calling: $TERM"
        cscope -d -L -3 "$TERM"
        ;;
    d|called)
        echo "Finding functions called by: $TERM"
        cscope -d -L -2 "$TERM"
        ;;
    t|text)
        echo "Searching for text: $TERM"
        cscope -d -L -4 "$TERM"
        ;;
    f|file)
        echo "Searching for file: $TERM"
        cscope -d -L -7 "$TERM"
        ;;
    i|include)
        echo "Finding files including: $TERM"
        cscope -d -L -8 "$TERM"
        ;;
    *)
        echo "Unknown search type: $TYPE"
        exit 1
        ;;
esac
EOF
    
    chmod +x "$COURSEWORK_DIR/kernel-search.sh"
    echo "✓ Search helper created: $COURSEWORK_DIR/kernel-search.sh"
}

# Function to show usage summary
show_summary() {
    echo ""
    echo "=== Setup Complete ==="
    echo "Kernel source navigation is now configured!"
    echo ""
    echo "📁 Kernel source: $KERNEL_SRC"
    echo "🔍 Cscope database: $KERNEL_SRC/cscope.out"
    echo "🏷️  Ctags database: $KERNEL_SRC/tags"
    echo "⚙️  Vim config: $HOME/.vimrc.kernel"
    echo "📂 Project config: $COURSEWORK_DIR/.vimrc"
    echo ""
    echo "🚀 Getting Started:"
    echo ""
    echo "1. Start vim in coursework directory:"
    echo "   cd $COURSEWORK_DIR && vim"
    echo ""
    echo "2. Navigate kernel code with cscope (Ctrl-\\ + key):"
    echo "   Ctrl-\\s  - Find symbol"
    echo "   Ctrl-\\g  - Find global definition"
    echo "   Ctrl-\\c  - Find functions calling this function"
    echo "   Ctrl-\\d  - Find functions called by this function"
    echo ""
    echo "3. Use ctags navigation:"
    echo "   Ctrl-]   - Jump to definition"
    echo "   Ctrl-T   - Jump back"
    echo ""
    echo "4. Command line tools:"
    echo "   ./kernel-search.sh kmalloc symbol"
    echo "   ./update-navigation.sh  # Refresh databases"
    echo ""
    echo "💡 Tips:"
    echo "   - Place cursor on function/symbol and press Ctrl-] to jump"
    echo "   - Use :cs help in vim for more cscope commands"
    echo "   - F5 in vim refreshes cscope database"
    echo "   - Databases update automatically when kernel source changes"
}

# Main function
main() {
    echo "This script will set up kernel source navigation tools:"
    echo "• Generate cscope database for symbol lookup"
    echo "• Generate ctags database for quick navigation"
    echo "• Configure vim for kernel development"
    echo "• Create helper scripts for searching"
    echo ""
    echo "Target kernel source: $KERNEL_SRC"
    echo "Coursework directory: $COURSEWORK_DIR"
    echo ""
    echo "Continue? [Y/n]"
    read -r response
    if [[ "$response" =~ ^[Nn]$ ]]; then
        echo "Setup cancelled."
        exit 0
    fi
    
    check_prerequisites
    setup_cscope
    setup_ctags
    setup_vim_config
    create_helper_scripts
    show_summary
    
    echo ""
    echo "🎉 Navigation setup completed successfully!"
    echo "Happy kernel hacking! 🐧"
}

# Handle interruption
trap 'echo ""; echo "Setup interrupted. Run again to complete setup."; exit 1' INT TERM

# Run main function
main "$@"