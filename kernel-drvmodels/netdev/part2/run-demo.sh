#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2024 TECH VEDA
#
# Part 2: PCI Driver Skeleton -- Feature Tests
#
# Tests:
#   1. PCI device enumeration (VID/DID match, probe called)
#   2. Network interface creation (alloc_netdev, register_netdev)
#   3. Interface bring-up (ndo_open)
#   4. Interface bring-down (ndo_stop)
#   5. Module unload (remove callback, resource cleanup)

PASS=0; FAIL=0; TOTAL=0

run_test() {
    TOTAL=$((TOTAL + 1))
    local desc="$1"; shift
    printf "  TEST %d: %-50s " "$TOTAL" "$desc"
    if eval "$@" >/dev/null 2>&1; then
        echo "[PASS]"; PASS=$((PASS + 1))
    else
        echo "[FAIL]"; FAIL=$((FAIL + 1))
    fi
}

cleanup() {
    sudo ip link set vnet0 down 2>/dev/null || true
    sudo rmmod vnet_skeleton 2>/dev/null || true
    sudo rmmod vnet_hw_platform 2>/dev/null || true
}
trap cleanup EXIT

echo "=============================================="
echo " Part 2: PCI Driver Skeleton"
echo "=============================================="
echo

# --- Setup ---
[ -f ../vnet-platform/vnet_hw_platform.ko ] || make -C ../vnet-platform >/dev/null 2>&1
[ -f vnet_skeleton.ko ] || make >/dev/null 2>&1

echo "[SETUP] Loading platform + driver modules..."
sudo insmod ../vnet-platform/vnet_hw_platform.ko 2>/dev/null || true
sleep 0.5
sudo insmod vnet_skeleton.ko 2>/dev/null
sleep 0.5
echo

# --- Tests ---
echo "[TESTS] PCI probe and interface creation"

run_test "PCI device visible (VID=1234 DID=5678)" \
    "lspci -d 1234:5678 2>/dev/null | grep -q ."

run_test "Network interface vnet0 created" \
    "ip link show vnet0"

run_test "Interface is DOWN before open" \
    "ip link show vnet0 | grep -q 'state DOWN'"

echo
echo "[TESTS] Interface lifecycle (ndo_open / ndo_stop)"

run_test "Bring interface UP (ndo_open)" \
    "sudo ip link set vnet0 up"

run_test "Interface state is UP after open" \
    "ip link show vnet0 | grep -qE 'UP|UNKNOWN'"

run_test "Bring interface DOWN (ndo_stop)" \
    "sudo ip link set vnet0 down"

echo
echo "[TESTS] Module unload and cleanup"

run_test "Unload driver (remove callback)" \
    "sudo rmmod vnet_skeleton"

run_test "Interface vnet0 gone after remove" \
    "! ip link show vnet0 2>/dev/null"

run_test "Unload platform module" \
    "sudo rmmod vnet_hw_platform"

run_test "PCI device gone after platform unload" \
    "! lspci -d 1234:5678 2>/dev/null | grep -q ."

# --- Summary ---
echo
echo "=============================================="
printf " Results: %d/%d passed" "$PASS" "$TOTAL"
[ "$FAIL" -gt 0 ] && printf ", %d FAILED" "$FAIL"
echo
echo "=============================================="
