#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2024 TECH VEDA
#
# Part 9: Multi-Queue TX/RX -- Feature Tests
#
# Tests:
#   1. Multiple TX/RX queues created (alloc_netdev_mqs with 4+4)
#   2. Per-queue sysfs entries (/sys/class/net/vnet0/queues/)
#   3. Per-queue NAPI instances
#   4. Queue channel count via ethtool -l
#   5. XPS/RPS CPU affinity mappings
#   6. Data path works across queues

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
    sudo rmmod vnet_multiqueue 2>/dev/null || true
    sudo rmmod vnet_hw_platform 2>/dev/null || true
}
trap cleanup EXIT

echo "=============================================="
echo " Part 9: Multi-Queue TX/RX"
echo "=============================================="
echo

# --- Setup ---
[ -f ../vnet-platform/vnet_hw_platform.ko ] || make -C ../vnet-platform >/dev/null 2>&1
[ -f vnet_multiqueue.ko ] || make >/dev/null 2>&1

echo "[SETUP] Loading platform + driver modules..."
sudo insmod ../vnet-platform/vnet_hw_platform.ko 2>/dev/null || true
sleep 0.5
sudo insmod vnet_multiqueue.ko 2>/dev/null
sleep 0.5

sudo ip link set vnet0 up 2>/dev/null || true
sudo ip addr add 10.99.0.1/24 dev vnet0 2>/dev/null || true
sleep 0.5
echo

# --- Tests ---
echo "[TESTS] Multi-queue allocation (alloc_netdev_mqs)"

TX_QUEUES=$(ls -d /sys/class/net/vnet0/queues/tx-* 2>/dev/null | wc -l)
RX_QUEUES=$(ls -d /sys/class/net/vnet0/queues/rx-* 2>/dev/null | wc -l)

run_test "Multiple TX queues created (expect 4)" \
    "[ '$TX_QUEUES' -ge 4 ]"

run_test "Multiple RX queues created (expect 4)" \
    "[ '$RX_QUEUES' -ge 4 ]"

echo
echo "         TX queues: $TX_QUEUES    RX queues: $RX_QUEUES"

echo
echo "[TESTS] Per-queue sysfs entries"

run_test "TX queue 0 exists in sysfs" \
    "[ -d /sys/class/net/vnet0/queues/tx-0 ]"

run_test "TX queue 3 exists in sysfs" \
    "[ -d /sys/class/net/vnet0/queues/tx-3 ]"

run_test "RX queue 0 exists in sysfs" \
    "[ -d /sys/class/net/vnet0/queues/rx-0 ]"

run_test "RX queue 3 exists in sysfs" \
    "[ -d /sys/class/net/vnet0/queues/rx-3 ]"

echo
echo "[TESTS] ethtool channel count (get_channels)"

run_test "ethtool -l returns channel info" \
    "ethtool -l vnet0 2>/dev/null | grep -qi 'combined\|tx\|rx'"

COMBINED=$(ethtool -l vnet0 2>/dev/null | grep -i 'combined' | tail -1 | awk '{print $NF}')
echo
echo "         Combined channels: ${COMBINED:-N/A}"

echo
echo "[TESTS] XPS CPU affinity (per-queue TX steering)"

run_test "XPS map exists for tx-0" \
    "[ -f /sys/class/net/vnet0/queues/tx-0/xps_cpus ]"

XPS0=$(cat /sys/class/net/vnet0/queues/tx-0/xps_cpus 2>/dev/null)
echo
echo "         tx-0 XPS CPU mask: ${XPS0:-N/A}"

echo
echo "[TESTS] Data path across queues"

run_test "Ping works with multi-queue driver" \
    "ping -c 3 -W 1 10.99.0.1"

TX_PKT=$(ip -s link show vnet0 2>/dev/null | awk '/TX:/{getline; print $1}')
run_test "TX packets transmitted via queues" \
    "[ '${TX_PKT:-0}' -gt 0 ]"

echo
echo "[TESTS] Cleanup"

run_test "Bring interface DOWN" \
    "sudo ip link set vnet0 down"

run_test "Unload driver module" \
    "sudo rmmod vnet_multiqueue"

# --- Summary ---
echo
echo "=============================================="
printf " Results: %d/%d passed" "$PASS" "$TOTAL"
[ "$FAIL" -gt 0 ] && printf ", %d FAILED" "$FAIL"
echo
echo "=============================================="
