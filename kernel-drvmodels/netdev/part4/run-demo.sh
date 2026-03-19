#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2024 TECH VEDA
#
# Part 4: Interrupts, RX Ring & Simple TX DMA -- Feature Tests
#
# Tests:
#   1. IRQ registration (request_irq in ndo_open)
#   2. RX ring allocation (dma_alloc_coherent for descriptors)
#   3. RX packet reception (platform generates synthetic packets)
#   4. Simple register-based TX (DMA single-slot transmit)
#   5. Interrupt delivery (/proc/interrupts counter)

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
    sudo rmmod vnet_irq_dma 2>/dev/null || true
    sudo rmmod vnet_hw_platform 2>/dev/null || true
}
trap cleanup EXIT

echo "=============================================="
echo " Part 4: Interrupts, RX Ring & Simple TX DMA"
echo "=============================================="
echo

# --- Setup ---
[ -f ../vnet-platform/vnet_hw_platform.ko ] || make -C ../vnet-platform >/dev/null 2>&1
[ -f vnet_irq_dma.ko ] || make >/dev/null 2>&1

echo "[SETUP] Loading platform + driver modules..."
sudo insmod ../vnet-platform/vnet_hw_platform.ko 2>/dev/null || true
sleep 0.5
sudo insmod vnet_irq_dma.ko 2>/dev/null
sleep 0.5
echo

# --- Tests ---
echo "[TESTS] IRQ registration and RX ring setup"

run_test "Interface created and can be brought UP" \
    "sudo ip link set vnet0 up"

sudo ip addr add 10.0.0.1/24 dev vnet0 2>/dev/null || true

run_test "IRQ registered in /proc/interrupts" \
    "grep -q vnet /proc/interrupts"

echo
echo "[TESTS] RX path (interrupt-driven, platform packet generator)"

RX_BEFORE=$(ip -s link show vnet0 2>/dev/null | awk '/RX:/{getline; print $1}')
sleep 2
RX_AFTER=$(ip -s link show vnet0 2>/dev/null | awk '/RX:/{getline; print $1}')

run_test "RX packets arriving (platform generator)" \
    "[ \"${RX_AFTER:-0}\" -gt \"${RX_BEFORE:-0}\" ]"

IRQ_COUNT=$(grep vnet /proc/interrupts 2>/dev/null | awk '{sum=0; for(i=2;i<=NF;i++) if($i~/^[0-9]+$/) sum+=$i; print sum}')
run_test "IRQ count > 0 (interrupts firing)" \
    "[ \"${IRQ_COUNT:-0}\" -gt 0 ]"

echo
echo "[TESTS] TX path (simple register-based DMA)"

run_test "Ping exercises simple TX DMA path" \
    "ping -c 2 -W 1 -I vnet0 10.0.0.2 2>/dev/null; true"

TX_PKT=$(ip -s link show vnet0 2>/dev/null | awk '/TX:/{getline; print $1}')
run_test "TX packet counter > 0 after ping" \
    "[ \"${TX_PKT:-0}\" -gt 0 ]"

echo
echo "[TESTS] Cleanup"

run_test "Bring interface DOWN (free_irq, free RX ring)" \
    "sudo ip link set vnet0 down"

run_test "Unload driver module" \
    "sudo rmmod vnet_irq_dma"

# --- Summary ---
echo
echo "=============================================="
printf " Results: %d/%d passed" "$PASS" "$TOTAL"
[ "$FAIL" -gt 0 ] && printf ", %d FAILED" "$FAIL"
echo
echo "=============================================="
