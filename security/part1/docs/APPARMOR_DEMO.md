# AppArmor Overriding Capabilities - Practical Demonstration

This demonstration proves that **AppArmor can block operations even when the program has the necessary capabilities**.

## The Question

> "Can AppArmor override capabilities, like capabilities override credentials?"

**Answer: YES!** This demo proves it.

## What This Demonstrates

We'll create a program that:
1. Has `CAP_DAC_READ_SEARCH` capability (can read ANY file)
2. Gets blocked by AppArmor from reading specific files
3. Shows AppArmor is the FINAL security layer

## Setup

### Step 1: Compile the Program

```bash
cd /home/raghub/raghu/workspace/repos/lkp0725/security/part1
gcc -o apparmor_test apparmor_test.c -lcap
```

### Step 2: Grant Capability

```bash
# Give the program CAP_DAC_READ_SEARCH
sudo setcap cap_dac_read_search=+ep apparmor_test

# Verify capability is set
getcap apparmor_test
# Output: apparmor_test = cap_dac_read_search+ep
```

### Step 3: Test WITHOUT AppArmor (Baseline)

```bash
./apparmor_test
```

**Expected Output:**
```
=== AppArmor vs Capabilities Demonstration ===

=== Security Status ===
UID: 1001
Capabilities: cap_dac_read_search=ep
AppArmor: unconfined
Status: UNCONFINED (no AppArmor restrictions)

=== Test 1: Read /etc/shadow ===
Attempting to read: /etc/shadow
  Result: SUCCESS - First 40 chars: root:!:19068:0:99999:7:::...

=== Test 2: Read /etc/sudoers ===
Attempting to read: /etc/sudoers
  Result: SUCCESS - First 40 chars: #...

=== Test 3: Write to /tmp/apparmor_test.txt ===
Attempting to write: /tmp/apparmor_test.txt
  Result: SUCCESS
```

**Key Observation:**
- ✅ Has `CAP_DAC_READ_SEARCH` capability
- ✅ Can read `/etc/shadow` (capability works!)
- ✅ Can read `/etc/sudoers` (capability works!)
- ✅ AppArmor is NOT active (unconfined)

---

### Step 4: Apply AppArmor Profile

```bash
# Make setup script executable
chmod +x setup_apparmor.sh

# Install and load AppArmor profile (needs root)
sudo ./setup_apparmor.sh
```

**What This Does:**
1. Copies `apparmor_test.profile` to `/etc/apparmor.d/`
2. Loads the profile using `apparmor_parser`
3. Profile contains: `deny /etc/shadow r,` and `deny /etc/sudoers r,`

---

### Step 5: Test WITH AppArmor (The Magic!)

```bash
./apparmor_test
```

**Expected Output:**
```
=== AppArmor vs Capabilities Demonstration ===

=== Security Status ===
UID: 1001
Capabilities: cap_dac_read_search=ep
AppArmor: apparmor_test (enforce)
Status: CONFINED (AppArmor enforcing)

=== Test 1: Read /etc/shadow ===
Attempting to read: /etc/shadow
  Result: DENIED (Permission denied)
  Reason: Permission denied
  This could be:
    - Missing capability (need CAP_DAC_READ_SEARCH)
    - OR AppArmor profile blocking access

=== Test 2: Read /etc/sudoers ===
Attempting to read: /etc/sudoers
  Result: DENIED (Permission denied)
  Reason: Permission denied

=== Test 3: Write to /tmp/apparmor_test.txt ===
Attempting to write: /tmp/apparmor_test.txt
  Result: SUCCESS
```

**KEY OBSERVATION - THE PROOF!**
- ✅ STILL has `CAP_DAC_READ_SEARCH` capability
- ❌ CANNOT read `/etc/shadow` anymore (AppArmor blocked it!)
- ❌ CANNOT read `/etc/sudoers` anymore (AppArmor blocked it!)
- ✅ CAN write to `/tmp` (AppArmor allows it)
- ✅ AppArmor is ACTIVE (enforce mode)

**The capability is still there, but AppArmor overrides it!**

---

## The Proof: Before and After

### Before AppArmor (Capability Works)

```
Program: apparmor_test
UID: 1001 (regular user)
Capability: CAP_DAC_READ_SEARCH ✓
AppArmor: unconfined
Permission check:
  1. UID == 0? NO ❌
  2. Has CAP_DAC_READ_SEARCH? YES ✓
  3. AppArmor allows? (unconfined - not checked)
Result: SUCCESS - Capability bypassed UID check
```

### After AppArmor (Capability Overridden!)

```
Program: apparmor_test
UID: 1001 (regular user)
Capability: CAP_DAC_READ_SEARCH ✓ (still there!)
AppArmor: apparmor_test (enforce)
Permission check:
  1. UID == 0? NO ❌
  2. Has CAP_DAC_READ_SEARCH? YES ✓
  3. AppArmor allows? NO ❌ ← FINAL DECISION!
Result: DENIED - AppArmor overrides capability!
```

---

## What the AppArmor Profile Does

```
profile apparmor_test /path/to/apparmor_test {
  # Explicitly DENY these files
  deny /etc/shadow r,
  deny /etc/sudoers r,

  # Allow other operations
  /tmp/* w,
}
```

This creates a **mandatory access control** policy that cannot be bypassed by capabilities!

---

## Verification

### Check AppArmor Status

```bash
sudo aa-status | grep apparmor_test
```

Output:
```
   apparmor_test (enforce)
```

### View AppArmor Denials

```bash
sudo dmesg | grep -i apparmor | tail -10
```

You should see messages like:
```
apparmor="DENIED" operation="open" profile="apparmor_test"
  name="/etc/shadow" comm="apparmor_test" requested_mask="r"
```

This proves AppArmor is blocking the access!

---

## The Security Hierarchy (Proven!)

```
WEAKEST PROTECTION
        ↓
┌─────────────────────────────────────┐
│ 1. Credentials (UID/GID)            │
│    Regular user cannot read shadow  │
└─────────────────────────────────────┘
        ↓ CAN OVERRIDE
┌─────────────────────────────────────┐
│ 2. Capabilities                     │
│    CAP_DAC_READ_SEARCH bypasses UID │
│    ✓ Can now read shadow            │
└─────────────────────────────────────┘
        ↓ CAN OVERRIDE
┌─────────────────────────────────────┐
│ 3. LSM (AppArmor)                   │
│    Profile denies /etc/shadow       │
│    ✗ BLOCKED despite capability!    │
└─────────────────────────────────────┘
        ↓
STRONGEST PROTECTION
```

---

## Real-World Examples

This is exactly how:

### 1. Snap Applications Work
```bash
# Firefox snap has NO special capabilities
# But is restricted by AppArmor

snap list firefox
cat /proc/$(pidof firefox)/attr/current
# Output: snap.firefox.firefox (enforce)

# Firefox CANNOT read your SSH keys even if it somehow got capabilities
ls ~/.ssh/id_rsa  # You can read it
# But Firefox cannot, AppArmor blocks it!
```

### 2. Docker Containers
```bash
# Container runs as root (UID 0) inside
# Has capabilities
# But AppArmor restricts what it can do on host
```

### 3. Systemd Services
```bash
# Service might run as root with capabilities
# But AppArmor profile limits damage if compromised
```

---

## Cleanup

### Disable AppArmor Profile

```bash
# Switch to complain mode (logs but doesn't block)
sudo aa-complain apparmor_test

# Run test again - should work now
./apparmor_test

# Completely disable
sudo aa-disable apparmor_test
```

### Remove Profile Completely

```bash
sudo aa-disable apparmor_test
sudo rm /etc/apparmor.d/apparmor_test
```

### Remove Capability

```bash
sudo setcap -r apparmor_test
```

---

## Summary: The Complete Picture

| Component | Purpose | Can Override |
|-----------|---------|--------------|
| **UID/GID** | Identity | - |
| **Capabilities** | Fine-grained privileges | UID/GID checks |
| **AppArmor** | Mandatory access control | UID/GID + Capabilities! |

**Formula:**
```c
Access = (UID matches OR Capability allows) AND AppArmor allows
                                                 ^^^^^^^^^^^^
                                                 FINAL DECISION
```

---

## Connection to Earlier Examples

### creds_kthread.ko
- UID 65534, but ALL capabilities
- **Unconfined** by AppArmor
- Result: Can read /etc/shadow (capabilities work)

### cap_demo
- UID 1001, CAP_DAC_READ_SEARCH
- **Unconfined** by AppArmor
- Result: Can read /etc/shadow (capability works)

### apparmor_test (THIS DEMO!)
- UID 1001, CAP_DAC_READ_SEARCH
- **CONFINED** by AppArmor
- Result: CANNOT read /etc/shadow (AppArmor blocks!)

**Same capability, different result based on AppArmor!**

---

## Key Takeaway

**Even root with all capabilities can be restricted by AppArmor!**

This is **defense in depth**:
- If credentials are compromised → Capabilities still protect
- If capabilities are bypassed → AppArmor still protects
- Multiple layers of security, each independent

LSMs like AppArmor are the **ultimate security gate** in the Linux kernel.
