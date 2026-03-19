#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2024 TECH VEDA
#
# Part 7: ethtool Integration -- Feature Tests
#
# Tests:
#   1. get_driverinfo (ethtool -i: driver name, version, bus_info)
#   2. get_link (ethtool: link detected)
#   3. get_link_ksettings (ethtool: speed, duplex, autoneg)
#   4. get_ethtool_stats (ethtool -S: custom statistics)
#   5. get_regs (ethtool -d: hardware register dump)

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
    sudo rmmod vnet_ethtool 2>/dev/null || true
    sudo rmmod vnet_hw_platform 2>/dev/null || true
}
trap cleanup EXIT

echo "=============================================="
echo " Part 7: ethtool Integration"
echo "=============================================="
echo

# --- Setup ---
[ -f ../vnet-platform/vnet_hw_platform.ko ] || make -C ../vnet-platform >/dev/null 2>&1
[ -f vnet_ethtool.ko ] || make >/dev/null 2>&1

echo "[SETUP] Loading platform + driver modules..."
sudo insmod ../vnet-platform/vnet_hw_platform.ko 2>/dev/null || true
sleep 0.5
sudo insmod vnet_ethtool.ko 2>/dev/null
sleep 0.5

sudo ip link set vnet0 up 2>/dev/null || true
sudo ip addr add 10.99.0.1/24 dev vnet0 2>/dev/null || true
sleep 0.5
echo

# --- Tests ---
echo "[TESTS] get_driverinfo (ethtool -i)"

run_test "ethtool -i returns driver name" \
    "ethtool -i vnet0 2>/dev/null | grep -q 'driver:'"

run_test "bus_info contains PCI address (pci_name)" \
    "ethtool -i vnet0 2>/dev/null | grep 'bus-info' | grep -q '0000:'"

echo
echo "[TESTS] get_link + get_link_ksettings (ethtool)"

run_test "Link detected reported" \
    "ethtool vnet0 2>/dev/null | grep -qi 'link detected'"

run_test "Speed reported (not unknown)" \
    "ethtool vnet0 2>/dev/null | grep 'Speed:' | grep -qv 'Unknown'"

run_test "Duplex reported" \
    "ethtool vnet0 2>/dev/null | grep -qi 'duplex'"

echo
echo "[TESTS] get_ethtool_stats (ethtool -S)"

ping -c 3 -W 1 10.99.0.1 >/dev/null 2>&1 || true

run_test "Custom stats returned (ethtool -S)" \
    "ethtool -S vnet0 2>/dev/null | grep -q ':'"

run_test "tx_packets stat present" \
    "ethtool -S vnet0 2>/dev/null | grep -qi 'tx_packets'"

run_test "rx_packets stat present" \
    "ethtool -S vnet0 2>/dev/null | grep -qi 'rx_packets'"

run_test "napi_polls stat present (driver-specific)" \
    "ethtool -S vnet0 2>/dev/null | grep -qi 'napi_polls'"

echo
echo "[TESTS] get_regs (ethtool -d)"

run_test "Register dump returned (ethtool -d)" \
    "ethtool -d vnet0 2>/dev/null | grep -q ':'"

echo
echo "[TESTS] Cleanup"

run_test "Bring interface DOWN" \
    "sudo ip link set vnet0 down"

run_test "Unload driver module" \
    "sudo rmmod vnet_ethtool"

# --- Summary ---
echo
echo "=============================================="
printf " Results: %d/%d passed" "$PASS" "$TOTAL"
[ "$FAIL" -gt 0 ] && printf ", %d FAILED" "$FAIL"
echo
echo "=============================================="
