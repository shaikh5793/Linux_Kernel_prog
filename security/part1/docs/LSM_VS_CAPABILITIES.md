# LSM (AppArmor) vs Capabilities: The Security Hierarchy

## Your Question

> "Like capabilities override creds, do LSMs like AppArmor override caps?"

**Answer: YES! LSMs are the FINAL arbiter and can deny even root with all capabilities.**

## The Complete Security Stack

```
                    Permission Check Flow
                    =====================

User tries to access /etc/shadow
            ↓
    ┌───────────────────┐
    │ Layer 1: UID/GID  │
    │ Check: UID == 0?  │
    └────────┬──────────┘
             │ ✓ Pass (root)
             ↓
    ┌───────────────────────────┐
    │ Layer 2: Capabilities     │
    │ Check: CAP_DAC_READ_SEARCH?│
    └────────┬──────────────────┘
             │ ✓ Pass (has capability)
             ↓
    ┌────────────────────────────┐
    │ Layer 3: LSM (AppArmor)    │
    │ Check: Profile allows?     │
    └────────┬───────────────────┘
             │ ✗ DENY!
             ↓
         DENIED
```

## Key Concept: LSM is the Final Gate

Even if you pass layers 1 and 2, LSM can still block you.

### Example 1: Firefox with Root Capabilities

```bash
# Firefox running as root (hypothetically)
UID: 0 (root) ✓
Capabilities: ALL ✓
AppArmor Profile: /snap/firefox/firefox (enforce)

# Try to read SSH keys
cat ~/.ssh/id_rsa
```

**Result:**
```
Permission denied
```

**Why?** AppArmor profile for Firefox says:
```
# Firefox is NOT allowed to read SSH keys
deny /home/*/.ssh/** r,
```

LSM blocked it even though process is root with all capabilities!

### Example 2: Your cap_demo with AppArmor

If we create an AppArmor profile for `cap_demo`:

```bash
# /etc/apparmor.d/cap_demo
profile cap_demo /path/to/cap_demo {
  # Deny reading /etc/shadow even with capabilities
  deny /etc/shadow r,

  # Allow network
  network inet stream,
}
```

Then:
```bash
# Grant ALL capabilities
sudo setcap cap_net_bind_service,cap_dac_read_search=+ep cap_demo

# Load AppArmor profile
sudo apparmor_parser -r /etc/apparmor.d/cap_demo

# Run
./cap_demo
```

**Result:**
- Port 80 binding: ✓ SUCCESS (AppArmor allows network)
- Read /etc/shadow: ✗ DENIED (AppArmor denies despite CAP_DAC_READ_SEARCH!)

## Comparison Table

| Scenario | UID | Capabilities | AppArmor | Can Read /etc/shadow? |
|----------|-----|--------------|----------|---------------------|
| Regular user | 1001 | None | unconfined | ❌ (UID check fails) |
| Root | 0 | All | unconfined | ✅ (passes all checks) |
| Root | 0 | All | deny /etc/shadow | ❌ (LSM blocks!) |
| cap_demo | 1001 | CAP_DAC_READ_SEARCH | unconfined | ✅ (capability bypasses UID) |
| cap_demo | 1001 | CAP_DAC_READ_SEARCH | deny /etc/shadow | ❌ (LSM blocks!) |

## What apparmor_info.c Shows

The module displays:

### 1. Current Process Confinement
```
=== CURRENT PROCESS CONFINEMENT ===
Process: cat (PID: 12345)
AppArmor Profile: unconfined
Status: UNCONFINED (No restrictions)
```

If running under confinement:
```
Process: firefox (PID: 67890)
AppArmor Profile: snap.firefox.firefox (enforce)
Status: ENFORCING (Violations blocked)
```

### 2. Loaded Profiles
Shows all active AppArmor profiles on your system:
```
=== LOADED APPARMOR PROFILES ===
Profile Name                              Mode
------------                              ----
/usr/sbin/cupsd                           enforce
/usr/sbin/mysqld                          enforce
snap.firefox.firefox                      enforce
```

### 3. AppArmor Features
```
=== APPARMOR FEATURES ===
Enabled: Y
Active LSMs: lockdown,landlock,yama,apparmor
  -> AppArmor is ACTIVE in LSM stack
```

## Real-World Example: Snap Applications

Snaps use AppArmor extensively:

```bash
# Install snap firefox
sudo snap install firefox

# Firefox runs confined
ps aux | grep firefox
# Shows: /snap/firefox/current/firefox

# Check profile
cat /proc/$(pidof firefox)/attr/current
# Shows: snap.firefox.firefox (enforce)
```

**What this means:**
- Firefox runs with YOUR UID (not root)
- Firefox has NO special capabilities
- Firefox is CONFINED by AppArmor

AppArmor profile restricts Firefox to only:
- Read its own config files
- Access network
- Read files in Downloads folder
- **CANNOT** read SSH keys, /etc/shadow, other users' files

**Even if you `sudo` run Firefox as root, AppArmor still restricts it!**

## Testing AppArmor

Let's test the apparmor_info module:

```bash
# Load the module
sudo insmod apparmor_info.ko

# View AppArmor info
cat /proc/apparmor_info

# Compare with system tool
sudo aa-status

# Unload
sudo rmmod apparmor_info
```

## The Hierarchy (Summary)

```
LEAST RESTRICTIVE
        ↓
┌─────────────────────┐
│  No restrictions    │  Regular user, UID 1001, no caps
│  (Regular user)     │  ✗ Cannot bind port 80
└─────────────────────┘  ✗ Cannot read /etc/shadow
        ↓
┌─────────────────────┐
│  + Capabilities     │  UID 1001 + CAP_NET_BIND_SERVICE
│  (Selective powers) │  ✓ CAN bind port 80
└─────────────────────┘  ✗ Cannot read /etc/shadow
        ↓
┌─────────────────────┐
│  + All Capabilities │  UID 0 (root), all capabilities
│  (Root)             │  ✓ CAN bind port 80
└─────────────────────┘  ✓ CAN read /etc/shadow
        ↓
┌─────────────────────┐
│  - AppArmor Profile │  Root + all caps + AppArmor deny
│  (LSM Confinement)  │  ✓/✗ Depends on profile
└─────────────────────┘  ✗ CANNOT if profile denies!
        ↓
MOST RESTRICTIVE
```

## Key Takeaway

**Security layers are cumulative, but LSM has the final say:**

1. **Credentials (UID/GID)**: Basic identity
2. **Capabilities**: Can OVERRIDE credential checks
3. **LSMs (AppArmor)**: Can OVERRIDE everything including capabilities!

**Formula:**
```
Access Granted = UID/GID check
                 OR Capability bypass
                 AND LSM allows
```

The `AND LSM allows` is the critical part - LSM can veto any decision!

## Connection to Your Examples

### creds_kthread.ko
- Kernel thread: UID 65534
- Has: ALL capabilities (inherited from kthreadd)
- LSM: unconfined (kernel threads typically unconfined)
- Result: Can read /etc/shadow (capabilities override UID, LSM doesn't block)

### cap_demo
- User program: UID 1001
- Has: CAP_DAC_READ_SEARCH (via file capability)
- LSM: unconfined (regular program, no profile)
- Result: Can read /etc/shadow (capability overrides UID, LSM doesn't block)

### Firefox (snap)
- User program: UID 1001
- Has: NO capabilities
- LSM: **confined by AppArmor profile**
- Result: Cannot read /etc/shadow (LSM blocks it!)

Even if we gave Firefox CAP_DAC_READ_SEARCH, AppArmor would still block it!

## Further Reading

- `man 7 apparmor` - AppArmor overview
- `/etc/apparmor.d/` - Profile directory
- `aa-genprof` - Generate profiles
- `aa-logprof` - Update profiles from logs
