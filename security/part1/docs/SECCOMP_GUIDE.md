# SECCOMP - The 4th Security Layer

## Overview

SECCOMP (Secure Computing Mode) is the **final** security layer in Linux that filters which system calls a process can execute.

```
Security Layers (Complete):
├── 1. Credentials (UID/GID)     ✅
├── 2. Capabilities              ✅
├── 3. LSM (AppArmor)           ✅
└── 4. SECCOMP                  ← This!
```

## Key Concept

**SECCOMP can block system calls even for root with all capabilities and no AppArmor profile!**

## Examples

### 1. seccomp_basic.c - SECCOMP Strict Mode

The simplest form of SECCOMP: only allows `read()`, `write()`, `exit()`, and `sigreturn()`.

```bash
gcc -o seccomp_basic seccomp_basic.c
./seccomp_basic
```

**What happens:**
```
=== SECCOMP Strict Mode Test ===

=== Status ===
PID: 13388
SECCOMP mode: 0 (DISABLED)

=== Before SECCOMP ===
getpid(): 13388 (ALLOWED)
getuid(): 1001 (ALLOWED)

=== Enabling SECCOMP STRICT mode ===
Only read/write/exit/sigreturn will be allowed
All other syscalls will KILL the process

SECCOMP enabled successfully

Attempting getpid()...
Killed
```

**The process is KILLED!** Even though `getpid()` is harmless, SECCOMP strict mode blocks it.

### 2. seccomp_info.ko - Kernel Module

Shows which processes are using SECCOMP:

```bash
sudo insmod seccomp_info.ko
cat /proc/seccomp_info
sudo rmmod seccomp_info
```

**Example output:**
```
=== SECCOMP Process Information ===

Current Process: cat (PID 14230)
SECCOMP mode: 0 (DISABLED)

=== Processes Using SECCOMP ===
Name                      PID     Mode Filter
----                      ---     ---- ------
systemd-resolved        823        2 YES
systemd-timesyncd       897        2 YES
sshd                    1245       2 YES
```

## The Complete Security Picture

### Permission Check Order

```c
/* Kernel permission check for a syscall */

1. Check if SECCOMP allows this syscall
   ↓ (if blocked → KILL or return -EPERM)

2. Check LSM (AppArmor/SELinux)
   ↓ (if denied → return -EACCES)

3. Check Capabilities
   ↓ (if missing → return -EPERM)

4. Check UID/GID
   ↓ (if no match → return -EACCES)

5. Allow operation
```

**SECCOMP is checked FIRST! It's the outermost layer.**

### Example: Root Process in Docker

```
Docker container process:
├── UID: 0 (root inside container)
├── Capabilities: ALL
├── AppArmor: docker-default profile
└── SECCOMP: docker default profile
```

Even though it's "root", Docker's SECCOMP profile blocks ~40 syscalls:
- `mount()` - blocked
- `reboot()` - blocked
- `sethostname()` - blocked
- `ptrace()` - blocked
- `keyctl()` - blocked

**Result:** Can't escape container despite being root!

## Real-World Usage

### 1. Docker Containers

```bash
# Check Docker's default seccomp profile
docker run -it ubuntu bash
cat /proc/self/status | grep Seccomp
# Seccomp: 2  (FILTER mode active)
```

### 2. systemd Services

```ini
# /etc/systemd/system/myservice.service
[Service]
SystemCallFilter=@basic-io @network-io
SystemCallFilter=~@privileged @resources

# Only allows basic I/O and network syscalls
# Blocks all privileged and resource management syscalls
```

### 3. Chrome Browser

```bash
# Chrome sandboxes each tab with SECCOMP
ps aux | grep chrome
cat /proc/<chrome-pid>/status | grep Seccomp
# Seccomp: 2

# Chrome renderers can only make ~20 syscalls out of 300+!
```

### 4. SSH Privilege Separation

```bash
# sshd spawns unprivileged processes with SECCOMP
ps aux | grep sshd
cat /proc/<sshd-pid>/status | grep Seccomp
# Seccomp: 2
```

## Comparison with Previous Layers

| Layer | Can Block Root? | Can Block with All Caps? | Can Block with No AppArmor? |
|-------|-----------------|--------------------------|----------------------------|
| Credentials | ❌ No | ❌ No | ❌ No |
| Capabilities | ✅ Yes | ❌ No | ❌ No |
| AppArmor | ✅ Yes | ✅ Yes | ❌ No |
| **SECCOMP** | ✅ Yes | ✅ Yes | ✅ **YES!** |

**SECCOMP is the ONLY layer that applies even if:**
- You're root
- You have all capabilities
- No AppArmor profile is active

## How to Check SECCOMP on a Process

```bash
# Check SECCOMP mode
grep Seccomp /proc/<PID>/status

# Output:
#   Seccomp: 0  → DISABLED
#   Seccomp: 1  → STRICT (only read/write/exit)
#   Seccomp: 2  → FILTER (custom BPF filter)

# For current shell:
grep Seccomp /proc/self/status
```

## Installing libseccomp (Optional)

For more advanced SECCOMP tests with custom filters:

```bash
sudo apt-get install libseccomp-dev

# Then you can compile seccomp_test.c:
gcc -o seccomp_test seccomp_test.c -lseccomp
./seccomp_test
```

## Summary

**The 4-Layer Security Model (Complete):**

```
┌─────────────────────────────────────────────┐
│ 4. SECCOMP                                  │
│    Q: Can I even make this syscall?         │
│    Applies to: Everyone (even root!)        │
└─────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────┐
│ 3. LSM (AppArmor/SELinux)                   │
│    Q: Am I allowed to access this resource? │
│    Can override: Credentials + Capabilities │
└─────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────┐
│ 2. Capabilities                             │
│    Q: Do I have special powers?             │
│    Can override: Credentials                │
└─────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────┐
│ 1. Credentials (UID/GID)                    │
│    Q: Who am I?                             │
│    Basic Unix permissions                   │
└─────────────────────────────────────────────┘
```

**Each layer adds protection. SECCOMP is the ultimate gate!**

## Key Takeaways

1. **SECCOMP filters syscalls** - Most restrictive layer
2. **Cannot be removed** - Once enabled, process can't disable it
3. **Inherited by children** - Child processes inherit parent's filter
4. **Used everywhere** - Docker, Chrome, systemd, SSH all use it
5. **Defense in depth** - Even if all other layers fail, SECCOMP protects

You now understand all 4 security layers of Linux!
