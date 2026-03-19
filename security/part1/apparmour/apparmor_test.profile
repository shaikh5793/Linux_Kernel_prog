# AppArmor profile for apparmor_test
#
# This profile demonstrates how AppArmor can block access even when
# the program has CAP_DAC_READ_SEARCH capability.
#
# Install: Copy to /etc/apparmor.d/apparmor_test
# Load:    sudo apparmor_parser -r /etc/apparmor.d/apparmor_test

#include <tunables/global>

profile apparmor_test /home/raghub/raghu/workspace/repos/lkp0725/security/part1/apparmour/apparmor_test {
  #include <abstractions/base>

  # Allow reading the binary itself
  /home/raghub/raghu/workspace/repos/lkp0725/security/part1/apparmour/apparmor_test mr,

  # Allow reading shared libraries
  /lib/x86_64-linux-gnu/** mr,
  /usr/lib/x86_64-linux-gnu/** mr,

  # Allow reading /proc for capability and AppArmor checks
  /proc/self/attr/current r,
  /proc/sys/kernel/cap_last_cap r,

  # CRITICAL: Explicitly DENY reading these files
  # Even though program has CAP_DAC_READ_SEARCH, AppArmor blocks it!
  deny /etc/shadow r,
  deny /etc/sudoers r,

  # Allow reading other /etc files (for comparison)
  /etc/hostname r,
  /etc/os-release r,

  # Allow writing to /tmp
  /tmp/* w,

  # Deny everything else by default (implicit)
}
