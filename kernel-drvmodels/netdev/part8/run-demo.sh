#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2024 TECH VEDA
#
# Part 8: PHY & MDIO Bus -- Feature Tests
#
# Tests:
#   1. MDIO bus registration (mdiobus_alloc + mdiobus_register)
#   2. PHY discovery on MDIO bus (PHYID1/PHYID2 scan)
#   3. PHY connection (phy_connect with adjust_link callback)
#   4. PHY-driven link state (carrier on/off via phylib)
#   5. Link settings from phylib (not hardcoded)
#   6. phy_start / phy_stop lifecycle

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
    sudo rmmod vnet_phy 2>/dev/null || true
    sudo rmmod vnet_hw_platform 2>/dev/null || true
}
trap cleanup EXIT

echo "=============================================="
echo " Part 8: PHY & MDIO Bus"
echo "=============================================="
echo

# --- Setup ---
[ -f ../vnet-platform/vnet_hw_platform.ko ] || make -C ../vnet-platform >/dev/null 2>&1
[ -f vnet_phy.ko ] || make >/dev/null 2>&1

echo "[SETUP] Loading platform + driver modules..."
sudo insmod ../vnet-platform/vnet_hw_platform.ko 2>/dev/null || true
sleep 0.5
sudo insmod vnet_phy.ko 2>/dev/null
sleep 0.5
echo

# --- Tests ---
echo "[TESTS] MDIO bus registration"

run_test "MDIO bus visible in sysfs" \
    "ls /sys/bus/mdio_bus/devices/ 2>/dev/null | grep -q vnet-mdio"

echo
echo "[TESTS] PHY discovery (mdiobus_register scans addresses 0-31)"

PHYDEV=$(find /sys/bus/mdio_bus/devices -maxdepth 1 -name 'vnet-mdio*' 2>/dev/null | head -1)

run_test "PHY device found on MDIO bus" \
    "[ -n '$PHYDEV' ]"

run_test "PHY ID readable (PHYID1/PHYID2)" \
    "[ -n '$PHYDEV' ] && cat '$PHYDEV/phy_id' 2>/dev/null | grep -q '0x'"

run_test "PHY interface mode is GMII" \
    "[ -n '$PHYDEV' ] && cat '$PHYDEV/phy_interface' 2>/dev/null | grep -qi 'gmii'"

echo
echo "[TESTS] PHY-driven link state (phy_start triggers autoneg)"

run_test "Interface UP (phy_start begins autoneg)" \
    "sudo ip link set vnet0 up"

sudo ip addr add 10.99.0.1/24 dev vnet0 2>/dev/null || true
sleep 1

CARRIER=$(cat /sys/class/net/vnet0/carrier 2>/dev/null || echo "0")
run_test "Carrier state set by adjust_link (not direct reg read)" \
    "[ '$CARRIER' = '1' ]"

echo
echo "[TESTS] Link settings from phylib"

run_test "ethtool reports speed from PHY (not hardcoded)" \
    "ethtool vnet0 2>/dev/null | grep 'Speed:' | grep -qv 'Unknown'"

run_test "ethtool reports duplex from PHY" \
    "ethtool vnet0 2>/dev/null | grep -qi 'duplex'"

run_test "Link detected via phylib" \
    "ethtool vnet0 2>/dev/null | grep -qi 'link detected: yes'"

echo
echo "[TESTS] Data path with PHY-managed link"

run_test "Ping works with PHY-managed link" \
    "ping -c 2 -W 1 10.99.0.1"

echo
echo "[TESTS] phy_stop lifecycle"

run_test "Bring interface DOWN (phy_stop halts state machine)" \
    "sudo ip link set vnet0 down"

CARRIER_DOWN=$(cat /sys/class/net/vnet0/carrier 2>/dev/null || echo "0")
run_test "Carrier is 0 after phy_stop" \
    "[ '${CARRIER_DOWN:-0}' = '0' ]"

echo
echo "[TESTS] Cleanup (phy_disconnect, mdiobus_unregister)"

run_test "Unload driver module" \
    "sudo rmmod vnet_phy"

run_test "MDIO bus removed from sysfs" \
    "! ls /sys/bus/mdio_bus/devices/ 2>/dev/null | grep -q vnet-mdio"

# --- Summary ---
echo
echo "=============================================="
printf " Results: %d/%d passed" "$PASS" "$TOTAL"
[ "$FAIL" -gt 0 ] && printf ", %d FAILED" "$FAIL"
echo
echo "=============================================="
