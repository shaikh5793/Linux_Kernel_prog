#!/bin/bash
#
# Copyright (c) 2024 techveda(www.techveda.org)
# Author: Raghu Bharadwaj
#
# This software is licensed under the MIT License.
# See the accompanying LICENSE file for the full text.
#
# VMA Inspector Test Suite Runner
#
# This script runs all VMA inspector tests in sequence to demonstrate
# different aspects of virtual memory area management and behavior.
#
# Usage: sudo ./run_all_tests.sh
# Requires: inspectvma module and compiled test programs

echo "=== VMA Inspector Complete Test Suite ==="
echo ""

# Check if we're running as root
if [ "$EUID" -ne 0 ]; then
    echo "Error: Tests require root access. Run with sudo."
    echo "Usage: sudo ./run_all_tests.sh"
    exit 1
fi

# Check if debugfs is mounted
if [ ! -d "/sys/kernel/debug" ]; then
    echo "Error: DebugFS not mounted. Try: sudo mount -t debugfs none /sys/kernel/debug"
    exit 1
fi

# Check if module is loaded
if [ ! -e "/sys/kernel/debug/inspectvma/vma_details" ]; then
    echo "Error: VMA inspector module not loaded."
    echo "Loading module..."
    if [ -f "inspectvma.ko" ]; then
        insmod inspectvma.ko
        echo "Module loaded successfully"
        sleep 1
    else
        echo "Module file not found. Run 'make' first."
        exit 1
    fi
fi

# Check if test programs exist
missing_tests=()
for test in test_vma test_stack test_malloc test_mmap test_mmap1 test_mmap2; do
    if [ ! -x "./$test" ]; then
        missing_tests+=("$test")
    fi
done

if [ ${#missing_tests[@]} -ne 0 ]; then
    echo "Error: Missing test programs: ${missing_tests[*]}"
    echo "Run 'make' to build all tests."
    exit 1
fi

echo "All prerequisites checked. Starting test suite..."
echo "========================================================="
echo ""

# Test 1: Basic VMA inspection
echo ">>> TEST 1: Basic VMA Details <<<"
echo "This test shows basic VMA inspection and memory layout overview"
echo ""
./test_vma
echo ""
echo "Press Enter to continue to next test..."
read

# Test 2: Stack growth analysis
echo ">>> TEST 2: Stack Growth Analysis <<<"
echo "This test demonstrates stack VMA behavior and growth patterns"
echo ""
./test_stack
echo ""
echo "Press Enter to continue to next test..."
read

# Test 3: malloc and heap behavior
echo ">>> TEST 3: malloc() and Heap Analysis <<<"
echo "This test shows how malloc() affects heap VMAs and allocation strategies"
echo ""
./test_malloc
echo ""
echo "Press Enter to continue to final test..."
read

# Test 4: mmap and VMA management
echo ">>> TEST 4: mmap() and VMA Management <<<"
echo "This test demonstrates mmap() operations and VMA creation/modification"
echo ""
./test_mmap
echo ""

echo "========================================================="
echo "=== All Tests Completed ==="
echo ""
echo "Summary of tests run:"
echo "1. ✓ Basic VMA inspection and memory layout overview"
echo "2. ✓ Stack growth behavior and recursive function analysis"
echo "3. ✓ malloc() heap management and allocation strategies"
echo "4. ✓ mmap() VMA management and memory mapping operations"
echo ""
echo "Key learning points covered:"
echo "• Virtual vs physical memory allocation"
echo "• Lazy allocation and demand paging"
echo "• Different types of VMAs (heap, stack, anonymous, file-backed)"
echo "• Memory mapping techniques and use cases"
echo "• Kernel VMA management and address space layout"
echo ""
echo "Additional specialized tests available:"
echo "• ./test_mmap1 - Basic anonymous and file-backed mmap tests"
echo "• ./test_mmap2 - Advanced mprotect and VMA splitting tests"
echo ""
echo "Next steps:"
echo "• Compare results with /proc/self/maps"
echo "• Experiment with different allocation patterns"
echo "• Explore memory pressure and swapping behavior"
echo "• Study performance implications of different mapping strategies"
