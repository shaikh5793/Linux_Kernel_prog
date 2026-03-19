#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2024 TECH VEDA
#
# Part 3: Net Device Ops -- Feature Tests
#
# Tests:
#   1. ndo_open / ndo_stop (interface up/down)
#   2. ndo_set_mac_address (MAC address change)
#   3. ndo_change_mtu (MTU change)
#   4. ndo_start_xmit (packet transmission)
#   5. ndo_get_stats64 (interface statistics)

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
    sudo rmmod vnet_basic 2>/dev/null || true
    sudo rmmod vnet_hw_platform 2>/dev/null || true
}
trap cleanup EXIT

echo "=============================================="
echo " Part 3: Net Device Ops & Calling Context"
echo "=============================================="
echo

# --- Setup ---
[ -f ../vnet-platform/vnet_hw_platform.ko ] || make -C ../vnet-platform >/dev/null 2>&1
[ -f vnet_basic.ko ] || make >/dev/null 2>&1

echo "[SETUP] Loading platform + driver modules..."
sudo insmod ../vnet-platform/vnet_hw_platform.ko 2>/dev/null || true
sleep 0.5
sudo insmod vnet_basic.ko 2>/dev/null
sleep 0.5
echo

# --- Tests ---
echo "[TESTS] ndo_open / ndo_stop"

run_test "Bring interface UP (ndo_open)" \
    "sudo ip link set vnet0 up"

run_test "Interface reports UP state" \
    "ip link show vnet0 | grep -qE 'UP|UNKNOWN'"

echo
echo "[TESTS] ndo_set_mac_address"

run_test "Change MAC to 02:de:ad:be:ef:01" \
    "sudo ip link set vnet0 address 02:de:ad:be:ef:01"

run_test "MAC address updated in interface" \
    "ip link show vnet0 | grep -qi '02:de:ad:be:ef:01'"

echo
echo "[TESTS] ndo_change_mtu"

OLD_MTU=$(cat /sys/class/net/vnet0/mtu 2>/dev/null)
run_test "Change MTU to 9000 (jumbo frames)" \
    "sudo ip link set vnet0 mtu 9000"

run_test "MTU reads 9000 from sysfs" \
    "[ \"$(cat /sys/class/net/vnet0/mtu 2>/dev/null)\" = '9000' ]"

echo
echo "[TESTS] ndo_start_xmit"

sudo ip addr add 10.99.0.1/24 dev vnet0 2>/dev/null || true
run_test "Ping loopback exercises ndo_start_xmit" \
    "ping -c 2 -W 1 10.99.0.1"

echo
echo "[TESTS] ndo_get_stats64"

TX_PKT=$(ip -s link show vnet0 2>/dev/null | awk '/TX:/{getline; print $1}')
run_test "TX packet counter > 0 after traffic" \
    "[ \"${TX_PKT:-0}\" -gt 0 ]"

echo
echo "[TESTS] ndo_stop + cleanup"

run_test "Bring interface DOWN (ndo_stop)" \
    "sudo ip link set vnet0 down"

run_test "Unload driver module" \
    "sudo rmmod vnet_basic"

# --- Summary ---
echo
echo "=============================================="
printf " Results: %d/%d passed" "$PASS" "$TOTAL"
[ "$FAIL" -gt 0 ] && printf ", %d FAILED" "$FAIL"
echo
echo "=============================================="
