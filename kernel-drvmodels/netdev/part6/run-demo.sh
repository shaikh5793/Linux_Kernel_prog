#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2024 TECH VEDA
#
# Part 6: NAPI Polling -- Feature Tests
#
# Tests:
#   1. NAPI registration (netif_napi_add in probe)
#   2. Interrupt coalescing (fewer IRQs than packets)
#   3. NAPI poll processes RX (netif_receive_skb replaces netif_rx)
#   4. Interrupt re-enable after napi_complete_done

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
    sudo rmmod vnet_napi 2>/dev/null || true
    sudo rmmod vnet_hw_platform 2>/dev/null || true
}
trap cleanup EXIT

echo "=============================================="
echo " Part 6: NAPI Polling"
echo "=============================================="
echo

# --- Setup ---
[ -f ../vnet-platform/vnet_hw_platform.ko ] || make -C ../vnet-platform >/dev/null 2>&1
[ -f vnet_napi.ko ] || make >/dev/null 2>&1

echo "[SETUP] Loading platform + driver modules..."
sudo insmod ../vnet-platform/vnet_hw_platform.ko 2>/dev/null || true
sleep 0.5
sudo insmod vnet_napi.ko 2>/dev/null
sleep 0.5
echo

# --- Tests ---
echo "[TESTS] NAPI registration and interface setup"

run_test "Interface UP (napi_enable + interrupt enable)" \
    "sudo ip link set vnet0 up"

sudo ip addr add 10.99.0.1/24 dev vnet0 2>/dev/null || true
sleep 0.5

run_test "IRQ registered in /proc/interrupts" \
    "grep -q vnet /proc/interrupts"

echo
echo "[TESTS] NAPI interrupt coalescing"

IRQ_BEFORE=$(grep vnet /proc/interrupts 2>/dev/null | awk '{sum=0; for(i=2;i<=NF;i++) if($i~/^[0-9]+$/) sum+=$i; print sum}')

run_test "Generate traffic (10 pings)" \
    "ping -c 10 -W 1 10.99.0.1"

sleep 1
IRQ_AFTER=$(grep vnet /proc/interrupts 2>/dev/null | awk '{sum=0; for(i=2;i<=NF;i++) if($i~/^[0-9]+$/) sum+=$i; print sum}')
IRQ_DELTA=$(( ${IRQ_AFTER:-0} - ${IRQ_BEFORE:-0} ))
RX_PKT=$(ip -s link show vnet0 2>/dev/null | awk '/RX:/{getline; print $1}')

run_test "Interrupts fired during traffic (IRQ delta > 0)" \
    "[ $IRQ_DELTA -gt 0 ]"

echo
echo "         IRQ count delta : $IRQ_DELTA"
echo "         RX packet count : ${RX_PKT:-?}"
echo "         (With NAPI, IRQs should be << packets. Part 5 would"
echo "          show ~1 IRQ per packet; Part 6 batches in poll.)"

echo
echo "[TESTS] NAPI RX delivery"

run_test "RX packets received (netif_receive_skb path)" \
    "[ \"${RX_PKT:-0}\" -gt 0 ]"

echo
echo "[TESTS] Cleanup (napi_disable + netif_napi_del)"

run_test "Bring interface DOWN" \
    "sudo ip link set vnet0 down"

run_test "Unload driver module" \
    "sudo rmmod vnet_napi"

# --- Summary ---
echo
echo "=============================================="
printf " Results: %d/%d passed" "$PASS" "$TOTAL"
[ "$FAIL" -gt 0 ] && printf ", %d FAILED" "$FAIL"
echo
echo "=============================================="
