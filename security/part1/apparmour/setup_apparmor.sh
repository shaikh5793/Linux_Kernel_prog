#!/bin/bash
#
# setup_apparmor.sh - Install and activate AppArmor profile for apparmor_test
#
# This script sets up the AppArmor demonstration

set -e

PROFILE_NAME="apparmor_test"
BINARY_PATH="$(pwd)/apparmor_test"
PROFILE_SRC="apparmor_test.profile"
PROFILE_DEST="/etc/apparmor.d/$PROFILE_NAME"

echo "=== AppArmor Profile Setup ==="
echo ""

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "Error: This script must be run as root (use sudo)"
    exit 1
fi

# Check if AppArmor is available
if ! command -v apparmor_parser &> /dev/null; then
    echo "Error: AppArmor not installed"
    echo "Install with: sudo apt-get install apparmor apparmor-utils"
    exit 1
fi

# Check if binary exists
if [ ! -f "$BINARY_PATH" ]; then
    echo "Error: $BINARY_PATH not found"
    echo "Compile first: gcc -o apparmor_test apparmor_test.c -lcap"
    exit 1
fi

# Check if profile source exists
if [ ! -f "$PROFILE_SRC" ]; then
    echo "Error: $PROFILE_SRC not found"
    exit 1
fi

echo "Installing AppArmor profile..."
echo "  Source: $PROFILE_SRC"
echo "  Destination: $PROFILE_DEST"

# Copy profile to system location
cp "$PROFILE_SRC" "$PROFILE_DEST"

# Update the binary path in the profile
sed -i "s|/home/raghub/raghu/workspace/repos/lkp0725/security/part1/apparmour/apparmor_test|$BINARY_PATH|g" "$PROFILE_DEST"

echo ""
echo "Loading AppArmor profile..."

# Load the profile
apparmor_parser -r "$PROFILE_DEST"

echo ""
echo "✓ AppArmor profile installed and loaded!"
echo ""
echo "Profile status:"
aa-status | grep -A2 "$PROFILE_NAME" || echo "  $PROFILE_NAME (enforce)"
echo ""
echo "=== Test the demonstration ==="
echo "Run: ./apparmor_test"
echo ""
echo "You should see:"
echo "  - AppArmor: $PROFILE_NAME (enforce)"
echo "  - /etc/shadow read: DENIED (blocked by AppArmor!)"
echo "  - /etc/sudoers read: DENIED (blocked by AppArmor!)"
echo ""
echo "Even though the program has CAP_DAC_READ_SEARCH capability!"
echo ""
echo "=== To remove the profile ==="
echo "  sudo aa-disable $PROFILE_NAME"
echo "  sudo rm $PROFILE_DEST"
echo ""
