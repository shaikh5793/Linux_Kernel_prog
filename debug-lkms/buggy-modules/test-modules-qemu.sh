#!/bin/sh
#
# Copyright (c) 2024 TECH VEDA(www.techveda.org)
# Author: Raghu Bharadwaj
#
# This software is dual-licensed under the MIT License and GPL v2.
# See the accompanying LICENSE file for the full text.
#
# Test script for buggy modules on ARM VExpress QEMU
# Run this script ON the QEMU target
#

echo "╔════════════════════════════════════════════════════╗"
echo "║  Buggy Kernel Modules - QEMU Test Suite          ║"
echo "╚════════════════════════════════════════════════════╝"
echo ""

show_menu() {
    echo "=== Test Categories ==="
    echo "1) Memory Bug Tests"
    echo "2) Locking Bug Tests"
    echo "3) Address Tests (WARNING: causes oops)"
    echo "4) Load all safe modules"
    echo "5) Unload all modules"
    echo "6) Show module status"
    echo "7) View dmesg"
    echo "8) Exit"
    echo ""
}

memory_menu() {
    echo "=== Memory Bug Tests ==="
    echo "1) Use-After-Free"
    echo "2) Buffer Overflow"
    echo "3) Double Free"
    echo "4) Slab Corruption"
    echo "5) Back"
    read -p "Select: " choice
    
    case $choice in
        1)
            echo "Loading use_after_free.ko..."
            insmod use_after_free.ko || true
            echo "Triggering use-after-free..."
            echo "trigger" > /proc/uaf_trigger
            echo "Check dmesg for KASAN/KFENCE reports"
            dmesg | tail -10
            ;;
        2)
            echo "Loading buffer_overflow.ko..."
            insmod buffer_overflow.ko || true
            echo "Triggering buffer overflow..."
            echo "overflow" > /proc/overflow_trigger
            echo "Check dmesg for KASAN/SLUB reports"
            dmesg | tail -10
            ;;
        3)
            echo "Loading double_free.ko..."
            insmod double_free.ko || true
            echo "Triggering double free..."
            echo "trigger" > /proc/double_free_trigger
            echo "Check dmesg for SLUB reports"
            dmesg | tail -10
            ;;
        4)
            echo "Loading slab_corruption.ko..."
            insmod slab_corruption.ko || true
            echo "Triggering slab corruption..."
            echo "corrupt" > /proc/slab_trigger
            echo "Check dmesg for SLUB reports"
            dmesg | tail -10
            ;;
    esac
}

locking_menu() {
    echo "=== Locking Bug Tests ==="
    echo "1) Deadlock Detection"
    echo "2) Double Unlock"
    echo "3) Missing Lock / Race"
    echo "4) Back"
    read -p "Select: " choice
    
    case $choice in
        1)
            echo "Loading deadlock.ko..."
            insmod deadlock.ko || true
            echo "Triggering deadlock scenario..."
            echo "trigger" > /proc/deadlock_trigger
            echo "Check dmesg for lockdep warnings"
            dmesg | tail -15
            ;;
        2)
            echo "Loading double_unlock.ko..."
            insmod double_unlock.ko || true
            echo "Triggering double unlock..."
            echo "mutex" > /proc/double_unlock_trigger
            echo "Check dmesg for lockdep warnings"
            dmesg | tail -10
            ;;
        3)
            echo "Loading missing_lock.ko..."
            insmod missing_lock.ko || true
            echo "Triggering race condition..."
            echo "race" > /proc/missing_lock_trigger
            echo "Waiting for threads to finish..."
            sleep 3
            echo ""
            echo "Race test results:"
            cat /proc/missing_lock_status
            ;;
    esac
}

address_menu() {
    echo "=== Address Translation Tests ==="
    echo "⚠️  WARNING: These tests WILL cause kernel oops!"
    echo "1) Null Pointer Dereference"
    echo "2) Invalid Memory Access"
    echo "3) Back"
    read -p "Select (will crash!): " choice
    
    case $choice in
        1)
            echo "Loading null_deref.ko..."
            insmod null_deref.ko || true
            echo "⚠️  This WILL crash! Continue? (y/N)"
            read confirm
            if [ "$confirm" = "y" ]; then
                echo "read" > /proc/null_deref_trigger
            fi
            ;;
        2)
            echo "Loading invalid_access.ko..."
            insmod invalid_access.ko || true
            echo "⚠️  This WILL crash! Continue? (y/N)"
            read confirm
            if [ "$confirm" = "y" ]; then
                echo "kernel" > /proc/invalid_access_trigger
            fi
            ;;
    esac
}

load_all() {
    echo "Loading all safe modules..."
    insmod use_after_free.ko 2>/dev/null || true
    insmod buffer_overflow.ko 2>/dev/null || true
    insmod double_free.ko 2>/dev/null || true
    insmod slab_corruption.ko 2>/dev/null || true
    insmod deadlock.ko 2>/dev/null || true
    insmod double_unlock.ko 2>/dev/null || true
    insmod missing_lock.ko 2>/dev/null || true
    echo "Modules loaded (address modules excluded)"
    lsmod | head -10
}

unload_all() {
    echo "Unloading all modules..."
    rmmod invalid_access 2>/dev/null || true
    rmmod null_deref 2>/dev/null || true
    rmmod missing_lock 2>/dev/null || true
    rmmod double_unlock 2>/dev/null || true
    rmmod deadlock 2>/dev/null || true
    rmmod slab_corruption 2>/dev/null || true
    rmmod double_free 2>/dev/null || true
    rmmod buffer_overflow 2>/dev/null || true
    rmmod use_after_free 2>/dev/null || true
    echo "All modules unloaded"
}

show_status() {
    echo "=== Module Status ==="
    lsmod | head -1
    lsmod | grep -E "(use_after|overflow|double|slab|deadlock|unlock|missing|null_deref|invalid_access)" || echo "No buggy modules loaded"
    echo ""
    echo "=== Available proc entries ==="
    ls -l /proc/*trigger* 2>/dev/null || echo "No trigger files available"
}

view_dmesg() {
    echo "=== Recent kernel messages ==="
    dmesg | tail -30
}

# Main loop
while true; do
    show_menu
    read -p "Select option: " choice
    
    case $choice in
        1) memory_menu ;;
        2) locking_menu ;;
        3) address_menu ;;
        4) load_all ;;
        5) unload_all ;;
        6) show_status ;;
        7) view_dmesg ;;
        8) exit 0 ;;
        *) echo "Invalid option" ;;
    esac
    
    echo ""
    echo "Press Enter to continue..."
    read dummy
    clear 2>/dev/null || echo ""
done
