#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2024 TECH VEDA
#
# Part 5: TX Ring Buffer & Full Duplex Rings -- Feature Tests
#
# Tests:
#   1. TX + RX ring allocation (dma_alloc_coherent for both)
#   2. Ring-based TX (descriptor ring with head/tail, replaces Part 4 simple TX)
#   3. TX completion (walk ring, check OWN flag, unmap, free skb)
#   4. RX still works (carried forward from Part 4)
#   5. Symmetric full duplex operation (both TX and RX via rings)

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
    sudo rmmod vnet_ring 2>/dev/null || true
    sudo rmmod vnet_hw_platform 2>/dev/null || true
}
trap cleanup EXIT

echo "=============================================="
echo " Part 5: TX Ring Buffer & Full Duplex Rings"
echo "=============================================="
echo

# --- Setup ---
[ -f ../vnet-platform/vnet_hw_platform.ko ] || make -C ../vnet-platform >/dev/null 2>&1
[ -f vnet_ring.ko ] || make >/dev/null 2>&1

echo "[SETUP] Loading platform + driver modules..."
sudo insmod ../vnet-platform/vnet_hw_platform.ko 2>/dev/null || true
sleep 0.5
sudo insmod vnet_ring.ko 2>/dev/null
sleep 0.5
echo

# --- Tests ---
echo "[TESTS] Ring allocation (TX ring is NEW in Part 5)"

run_test "Interface UP (allocates TX + RX descriptor rings)" \
    "sudo ip link set vnet0 up"

sudo ip addr add 10.99.0.1/24 dev vnet0 2>/dev/null || true
sleep 0.5

echo
echo "[TESTS] TX path via descriptor ring (replaces Part 4 simple TX)"

TX_BEFORE=$(ip -s link show vnet0 2>/dev/null | awk '/TX:/{getline; print $1}')

run_test "Ping sends packets via TX ring + DMA doorbell" \
    "ping -c 5 -W 1 10.99.0.1"

TX_AFTER=$(ip -s link show vnet0 2>/dev/null | awk '/TX:/{getline; print $1}')
run_test "TX counter incremented (ring-based TX completed)" \
    "[ \"${TX_AFTER:-0}\" -gt \"${TX_BEFORE:-0}\" ]"

echo
echo "[TESTS] RX path via ring (carried forward from Part 4)"

RX_BEFORE=$(ip -s link show vnet0 2>/dev/null | awk '/RX:/{getline; print $1}')
sleep 2
RX_AFTER=$(ip -s link show vnet0 2>/dev/null | awk '/RX:/{getline; print $1}')

run_test "RX packets arriving via RX ring" \
    "[ \"${RX_AFTER:-0}\" -gt \"${RX_BEFORE:-0}\" ]"

echo
echo "[TESTS] Full duplex (symmetric TX + RX rings)"

TX_FINAL=$(ip -s link show vnet0 2>/dev/null | awk '/TX:/{getline; print $1}')
RX_FINAL=$(ip -s link show vnet0 2>/dev/null | awk '/RX:/{getline; print $1}')
run_test "Both TX and RX counters > 0 (full duplex)" \
    "[ \"${TX_FINAL:-0}\" -gt 0 ] && [ \"${RX_FINAL:-0}\" -gt 0 ]"

echo
echo "[TESTS] Cleanup (frees both TX + RX descriptor rings)"

run_test "Bring interface DOWN" \
    "sudo ip link set vnet0 down"

run_test "Unload driver module" \
    "sudo rmmod vnet_ring"

# --- Summary ---
echo
echo "=============================================="
printf " Results: %d/%d passed" "$PASS" "$TOTAL"
[ "$FAIL" -gt 0 ] && printf ", %d FAILED" "$FAIL"
echo
echo "=============================================="
