#!/bin/bash
#
# Copyright (c) 2024 TECH VEDA(www.techveda.org)
# Author: Raghu Bharadwaj
#
# This software is dual-licensed under the MIT License and GPL v2.
# See the accompanying LICENSE file for the full text.
#
# Master test script for all buggy kernel modules
# This script helps test all the debug features in your kernel
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}╔════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║  Buggy Kernel Modules - Debug Testing Suite      ║${NC}"
echo -e "${BLUE}╔════════════════════════════════════════════════════╗${NC}"
echo ""

show_menu() {
    echo -e "${GREEN}=== Module Categories ===${NC}"
    echo "1) Memory Bug Tests (KASAN, KFENCE, SLUB Debug)"
    echo "2) Locking Bug Tests (Lockdep, Race Conditions)"
    echo "3) Address Translation Tests (Page Faults, MMU)"
    echo "4) Load All Modules"
    echo "5) Unload All Modules"
    echo "6) Show Loaded Modules"
    echo "7) View dmesg logs"
    echo "8) Exit"
    echo ""
}

memory_tests() {
    echo -e "${YELLOW}=== Memory Bug Tests ===${NC}"
    echo "1) Use-After-Free (KASAN/KFENCE)"
    echo "2) Buffer Overflow (KASAN/SLUB)"
    echo "3) Double Free (SLUB Debug)"
    echo "4) Slab Corruption (SLUB Debug)"
    echo "5) Back to main menu"
    echo ""
    read -p "Select test: " choice
    
    case $choice in
        1)
            echo -e "${YELLOW}Testing Use-After-Free...${NC}"
            sudo insmod memory/use_after_free.ko 2>/dev/null || echo "Module may already be loaded"
            echo "trigger" | sudo tee /proc/uaf_trigger
            echo -e "${GREEN}Check dmesg for KASAN/KFENCE reports${NC}"
            ;;
        2)
            echo -e "${YELLOW}Testing Buffer Overflow...${NC}"
            sudo insmod memory/buffer_overflow.ko 2>/dev/null || echo "Module may already be loaded"
            echo "overflow" | sudo tee /proc/overflow_trigger
            echo -e "${GREEN}Check dmesg for KASAN/SLUB reports${NC}"
            ;;
        3)
            echo -e "${YELLOW}Testing Double Free...${NC}"
            sudo insmod memory/double_free.ko 2>/dev/null || echo "Module may already be loaded"
            echo "trigger" | sudo tee /proc/double_free_trigger
            echo -e "${GREEN}Check dmesg for SLUB debug reports${NC}"
            ;;
        4)
            echo -e "${YELLOW}Testing Slab Corruption...${NC}"
            sudo insmod memory/slab_corruption.ko 2>/dev/null || echo "Module may already be loaded"
            echo "corrupt" | sudo tee /proc/slab_trigger
            echo -e "${GREEN}Check dmesg for SLUB corruption reports${NC}"
            ;;
        5)
            return
            ;;
    esac
}

locking_tests() {
    echo -e "${YELLOW}=== Locking Bug Tests ===${NC}"
    echo "1) Deadlock Detection (Lockdep)"
    echo "2) Double Unlock (Lockdep)"
    echo "3) Missing Lock / Race Condition"
    echo "4) Back to main menu"
    echo ""
    read -p "Select test: " choice
    
    case $choice in
        1)
            echo -e "${YELLOW}Testing Deadlock Detection...${NC}"
            sudo insmod locking/deadlock.ko 2>/dev/null || echo "Module may already be loaded"
            echo "trigger" | sudo tee /proc/deadlock_trigger
            echo -e "${GREEN}Check dmesg for lockdep warnings${NC}"
            ;;
        2)
            echo -e "${YELLOW}Testing Double Unlock...${NC}"
            sudo insmod locking/double_unlock.ko 2>/dev/null || echo "Module may already be loaded"
            echo "mutex" | sudo tee /proc/double_unlock_trigger
            echo -e "${GREEN}Check dmesg for lockdep warnings${NC}"
            ;;
        3)
            echo -e "${YELLOW}Testing Race Condition...${NC}"
            sudo insmod locking/missing_lock.ko 2>/dev/null || echo "Module may already be loaded"
            echo "race" | sudo tee /proc/missing_lock_trigger
            sleep 2
            cat /proc/missing_lock_status
            echo -e "${GREEN}If counter != 2000, race condition detected${NC}"
            ;;
        4)
            return
            ;;
    esac
}

address_tests() {
    echo -e "${YELLOW}=== Address Translation Tests ===${NC}"
    echo -e "${RED}WARNING: These tests WILL cause kernel oops!${NC}"
    echo "1) Null Pointer Dereference"
    echo "2) Invalid Memory Access"
    echo "3) Back to main menu"
    echo ""
    read -p "Select test (WARNING - will crash): " choice
    
    case $choice in
        1)
            echo -e "${RED}Testing Null Pointer Dereference...${NC}"
            sudo insmod address/null_deref.ko 2>/dev/null || echo "Module may already be loaded"
            echo -e "${RED}This WILL cause a kernel oops. Continue? (y/N)${NC}"
            read -p "> " confirm
            if [[ "$confirm" == "y" ]]; then
                echo "read" | sudo tee /proc/null_deref_trigger
            fi
            ;;
        2)
            echo -e "${RED}Testing Invalid Memory Access...${NC}"
            sudo insmod address/invalid_access.ko 2>/dev/null || echo "Module may already be loaded"
            echo -e "${RED}This WILL cause a kernel oops. Continue? (y/N)${NC}"
            read -p "> " confirm
            if [[ "$confirm" == "y" ]]; then
                echo "kernel" | sudo tee /proc/invalid_access_trigger
            fi
            ;;
        3)
            return
            ;;
    esac
}

load_all_modules() {
    echo -e "${YELLOW}Loading all buggy modules...${NC}"
    
    # Memory modules
    sudo insmod memory/use_after_free.ko 2>/dev/null || true
    sudo insmod memory/buffer_overflow.ko 2>/dev/null || true
    sudo insmod memory/double_free.ko 2>/dev/null || true
    sudo insmod memory/slab_corruption.ko 2>/dev/null || true
    
    # Locking modules
    sudo insmod locking/deadlock.ko 2>/dev/null || true
    sudo insmod locking/double_unlock.ko 2>/dev/null || true
    sudo insmod locking/missing_lock.ko 2>/dev/null || true
    
    # Note: Not loading address modules by default as they cause oops
    echo -e "${GREEN}Memory and locking modules loaded${NC}"
    echo -e "${YELLOW}Address modules NOT loaded (they cause oops)${NC}"
    lsmod | grep -E "(use_after|overflow|double|slab|deadlock|unlock|missing_lock)"
}

unload_all_modules() {
    echo -e "${YELLOW}Unloading all buggy modules...${NC}"
    
    # Unload in reverse order
    sudo rmmod invalid_access 2>/dev/null || true
    sudo rmmod null_deref 2>/dev/null || true
    sudo rmmod missing_lock 2>/dev/null || true
    sudo rmmod double_unlock 2>/dev/null || true
    sudo rmmod deadlock 2>/dev/null || true
    sudo rmmod slab_corruption 2>/dev/null || true
    sudo rmmod double_free 2>/dev/null || true
    sudo rmmod buffer_overflow 2>/dev/null || true
    sudo rmmod use_after_free 2>/dev/null || true
    
    echo -e "${GREEN}All modules unloaded${NC}"
}

show_loaded() {
    echo -e "${YELLOW}=== Loaded Buggy Modules ===${NC}"
    lsmod | head -1
    lsmod | grep -E "(use_after|overflow|double|slab|deadlock|unlock|missing_lock|null_deref|invalid_access)" || echo "No buggy modules loaded"
}

view_dmesg() {
    echo -e "${YELLOW}=== Recent kernel messages (last 50 lines) ===${NC}"
    dmesg | tail -50
    echo ""
    echo -e "${YELLOW}=== Filter for specific patterns? ===${NC}"
    echo "1) KASAN reports"
    echo "2) Lockdep warnings"
    echo "3) Oops/BUG messages"
    echo "4) All buggy module messages"
    echo "5) Back"
    read -p "Select: " choice
    
    case $choice in
        1) dmesg | grep -i kasan | tail -30 ;;
        2) dmesg | grep -i lockdep | tail -30 ;;
        3) dmesg | grep -E "(Oops|BUG|Unable to handle)" | tail -30 ;;
        4) dmesg | grep -E "(use_after|overflow|double|slab|deadlock|unlock|missing_lock|null_deref|invalid_access)" | tail -30 ;;
        5) return ;;
    esac
}

# Main loop
while true; do
    show_menu
    read -p "Select option: " choice
    
    case $choice in
        1) memory_tests ;;
        2) locking_tests ;;
        3) address_tests ;;
        4) load_all_modules ;;
        5) unload_all_modules ;;
        6) show_loaded ;;
        7) view_dmesg ;;
        8) 
            echo -e "${GREEN}Exiting...${NC}"
            exit 0
            ;;
        *)
            echo -e "${RED}Invalid option${NC}"
            ;;
    esac
    
    echo ""
    read -p "Press Enter to continue..."
    clear
done
