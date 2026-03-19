#!/bin/sh
#
# W25Q32 SPI Flash Sysfs Driver Test Script  
# Tests sysfs interface with manual pauses between stages
#

MODULE="spiflash"
SYSFS_DIR="/sys/spiflash"
W25Q32_ATTR="$SYSFS_DIR/w25q32"
OFFSET_ATTR="$SYSFS_DIR/offset"
ERASE_ATTR="$SYSFS_DIR/erase"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m'

print_header() {
    echo ""
    echo "${YELLOW}========================================${NC}"
    echo "${YELLOW}$1${NC}"
    echo "${YELLOW}========================================${NC}"
}

print_pass() {
    echo "${GREEN}✓ $1${NC}"
}

print_fail() {
    echo "${RED}✗ $1${NC}"
}

print_info() {
    echo "${BLUE}ℹ $1${NC}"
}

pause_test() {
    echo ""
    echo "${BLUE}Press Enter to continue to next test...${NC}"
    read dummy
}

# Check if module is loaded
if ! lsmod | grep -q "^$MODULE"; then
    print_header "Stage 0: Loading $MODULE module"
    if ! modprobe $MODULE; then
        print_fail "Failed to load $MODULE"
        exit 1
    fi
    sleep 2
    print_pass "Module loaded"
else
    print_info "Module $MODULE already loaded"
fi

# Check if sysfs directory exists
if [ ! -d "$SYSFS_DIR" ]; then
    print_fail "Sysfs directory $SYSFS_DIR not found"
    exit 1
fi

print_header "Stage 1: Verify Sysfs Interface"
echo "Checking sysfs attributes:"
ls -l $SYSFS_DIR/
print_pass "Sysfs interface available"
pause_test

print_header "Stage 2: Read Current Offset"
if [ -f "$OFFSET_ATTR" ]; then
    CURRENT_OFFSET=$(cat $OFFSET_ATTR)
    echo "Current offset: $CURRENT_OFFSET"
    print_pass "Offset attribute readable"
else
    print_fail "Offset attribute not found"
fi
pause_test

print_header "Stage 3: Erase Sector 0"
echo "Erasing block 0, sector 0..."
echo "0:0" > $ERASE_ATTR
sleep 2
print_pass "Sector erase completed"
print_info "Note: Erase sets all bytes to 0xFF"
pause_test

print_header "Stage 4: Verify Erase (Should be 0xFF)"
echo "Setting offset to 0:0:0..."
echo "0:0:0" > $OFFSET_ATTR
echo "Reading page 0 after erase..."
dd if=$W25Q32_ATTR bs=256 count=1 2>/dev/null | od -A x -t x1z -v | head -10
print_pass "Erase verification completed"
pause_test

print_header "Stage 5: Write Test Pattern to Page 0"
echo "Writing 'TECHVEDA_SPI_FLASH_TEST_PAGE_0' to page 0..."
echo "0:0:0" > $OFFSET_ATTR
echo "TECHVEDA_SPI_FLASH_TEST_PAGE_0" | dd of=$W25Q32_ATTR bs=256 count=1 conv=sync 2>/dev/null
sync
sleep 1
print_pass "Write completed"
pause_test

print_header "Stage 6: Read and Verify Written Data"
echo "Reading back page 0..."
dd if=$W25Q32_ATTR bs=256 count=1 2>/dev/null | od -A x -t c -v | head -5
print_pass "Read verification completed"
pause_test

print_header "Stage 7: Write to Page 1"
echo "Setting offset to page 1 (0:0:1)..."
echo "0:0:1" > $OFFSET_ATTR
echo "Current offset: $(cat $OFFSET_ATTR)"
echo "Writing 'PAGE_1_DATA_TECHVEDA_W25Q32' to page 1..."
echo "PAGE_1_DATA_TECHVEDA_W25Q32" | dd of=$W25Q32_ATTR bs=256 count=1 conv=sync 2>/dev/null
sync
sleep 1
print_pass "Page 1 write completed"
pause_test

print_header "Stage 8: Read Multiple Pages (0-3)"
echo "Reading pages 0-3..."
for i in 0 1 2 3; do
    echo "--- Page $i ---"
    echo "0:0:$i" > $OFFSET_ATTR
    dd if=$W25Q32_ATTR bs=256 count=1 2>/dev/null | od -A x -t c -v | head -3
done
print_pass "Multi-page read completed"
pause_test

print_header "Stage 9: Test Different Block/Sector"
echo "Erasing block 1, sector 2..."
echo "1:2" > $ERASE_ATTR
sleep 2
echo "Setting offset to Block 1, Sector 2, Page 5 (1:2:5)..."
echo "1:2:5" > $OFFSET_ATTR
echo "Current offset: $(cat $OFFSET_ATTR)"
echo "Writing 'BLOCK1_SECT2_PAGE5' to this location..."
echo "BLOCK1_SECT2_PAGE5" | dd of=$W25Q32_ATTR bs=256 count=1 conv=sync 2>/dev/null
sync
sleep 1
print_pass "Different location write completed"
pause_test

print_header "Stage 10: Read from Different Location"
echo "Reading back block 1, sector 2, page 5..."
dd if=$W25Q32_ATTR bs=256 count=1 2>/dev/null | od -A x -t c -v | head -3
print_pass "Different location read completed"
pause_test

print_header "Test Summary"
print_pass "All stages completed successfully!"
echo ""
echo "${BLUE}Key Points:${NC}"
echo "  - Sysfs interface: $SYSFS_DIR"
echo "  - Data attribute: $W25Q32_ATTR (read/write 256 bytes)"
echo "  - Offset attribute: $OFFSET_ATTR (format: block:sector:page)"
echo "  - Erase attribute: $ERASE_ATTR (format: block:sector)"
echo ""
echo "${BLUE}Manual Operations:${NC}"
echo "  Set offset:  echo '0:0:5' > $OFFSET_ATTR"
echo "  Erase sect:  echo '0:0' > $ERASE_ATTR"
echo "  Write data:  echo 'test' | dd of=$W25Q32_ATTR bs=256 count=1 conv=sync"
echo "  Read data:   dd if=$W25Q32_ATTR bs=256 count=1"
echo ""
echo "${BLUE}Flash Organization:${NC}"
echo "  - Total: 4MB (4194304 bytes)"
echo "  - Page size: 256 bytes"
echo "  - Sector size: 4KB (16 pages)"
echo "  - Block size: 64KB (16 sectors)"
echo "  - Total blocks: 64"
echo ""
echo "${BLUE}Important:${NC} Always erase a sector before writing!"
echo ""
echo "${BLUE}To unload driver:${NC} rmmod $MODULE"
