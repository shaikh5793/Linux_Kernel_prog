# SECCOMP Modes Explained

## Overview

SECCOMP has **3 modes** for syscall filtering. Each mode provides different levels of restriction.

## Mode Comparison

| Mode | Value | Description | What Happens |
|------|-------|-------------|--------------|
| **DISABLED** | 0 | No filtering (default) | All syscalls allowed (normal permissions apply) |
| **STRICT** | 1 | Ultra restrictive | Only read/write/exit/sigreturn, rest → KILL |
| **FILTER** | 2 | Custom BPF filter | Per-syscall decision with flexible actions |

---

## Mode 0: DISABLED (Default)

```c
/* This is the default for all processes */
prctl(PR_GET_SECCOMP);  // Returns 0
```

**Behavior:**
- No SECCOMP filtering active
- All syscalls allowed (subject to normal checks):
  - UID/GID permissions
  - Capabilities
  - LSM (AppArmor/SELinux)
- Normal Linux behavior

**Use case:**
- Standard processes
- No special isolation needed

---

## Mode 1: STRICT

```c
/* Enable STRICT mode */
prctl(PR_SET_SECCOMP, SECCOMP_MODE_STRICT);
```

**Allowed syscalls (only 4):**
1. `read()` - Read from file descriptor
2. `write()` - Write to file descriptor
3. `exit()` - Terminate process
4. `sigreturn()` - Return from signal handler

**Any other syscall:**
- Process immediately killed with `SIGKILL`
- No exceptions, even for root
- Cannot be undone

**Example:**
```c
prctl(PR_SET_SECCOMP, SECCOMP_MODE_STRICT);

write(STDOUT_FILENO, "OK\n", 3);  // ✓ Works
getpid();  // ✗ KILLED! Process dies here
```

**Use case:**
- Pure computation (no I/O except stdin/stdout)
- Maximum isolation
- Legacy: Original SECCOMP design (2005)

**Limitations:**
- Too restrictive for most programs
- Can't even call `open()`, `close()`, `mmap()`
- Rarely used directly anymore

---

## Mode 2: FILTER

```c
/* Enable FILTER mode with BPF program */
struct sock_fprog prog = { ... };
prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog);
```

**How it works:**
1. Define BPF (Berkeley Packet Filter) program
2. Program examines each syscall
3. Returns action to take

**Available actions:**

| Action | Effect | Use Case |
|--------|--------|----------|
| `ALLOW` | Permit syscall | Safe operations |
| `ERRNO` | Return error code (e.g., EPERM) | Graceful denial |
| `TRAP` | Send SIGSYS signal | Handle violation in code |
| `TRACE` | Notify tracer (ptrace) | Debugging |
| `KILL` | Kill process | Dangerous syscalls |

**Example: Docker's approach**
```c
// Allow most syscalls
Default: ALLOW

// Block specific dangerous ones
reboot()       → ERRNO(EPERM)
mount()        → ERRNO(EPERM)
sethostname()  → ERRNO(EPERM)
ptrace()       → ERRNO(EPERM)
```

**Use cases:**
- **Docker/Podman** - Block container escape syscalls
- **Chrome/Firefox** - Sandbox renderer processes
- **systemd** - `SystemCallFilter=` directive
- **OpenSSH** - Restrict sshd child processes

---

## Comparison Example

### Same program, different SECCOMP modes:

```c
// Program that calls: read(), write(), getpid(), open()

Mode 0 (DISABLED):
  read()    → ✓ SUCCESS
  write()   → ✓ SUCCESS
  getpid()  → ✓ SUCCESS
  open()    → ✓ SUCCESS

Mode 1 (STRICT):
  read()    → ✓ SUCCESS
  write()   → ✓ SUCCESS
  getpid()  → ✗ KILLED (process dies)
  open()    → (never reached)

Mode 2 (FILTER - custom):
  read()    → ✓ ALLOW (allowed in filter)
  write()   → ✓ ALLOW (allowed in filter)
  getpid()  → ✗ ERRNO=EPERM (blocked, returns error)
  open()    → ✗ ERRNO=EPERM (blocked, returns error)
  // Process continues running!
```

---

## Real-World Examples

### Docker Default Profile

```yaml
# Docker blocks ~40 syscalls with FILTER mode
Blocked syscalls:
  - mount, umount2        # Filesystem operations
  - reboot, sethostname   # System operations
  - ptrace, process_vm_*  # Process manipulation
  - keyctl, add_key       # Kernel keyring
  - perf_event_open       # Performance monitoring
  - bpf                   # BPF programs

Action: ERRNO (EPERM) - graceful denial
```

### Chrome Renderer Sandbox

```yaml
# Chrome allows only ~20 syscalls out of 300+
Allowed syscalls:
  - read, write, close    # Basic I/O
  - mmap, munmap          # Memory management
  - getpid, gettid        # Process info
  - futex, nanosleep      # Synchronization
  - exit, exit_group      # Termination

Everything else: KILL
```

### systemd Service

```ini
# /etc/systemd/system/myservice.service
[Service]
SystemCallFilter=@basic-io @network-io
SystemCallFilter=~@privileged @resources

# Translated to FILTER mode:
# - Allow: basic I/O and network syscalls
# - Block: privileged operations, resource management
```

---

## Testing the Modes

```bash
# Compile the interactive program
gcc -o seccomp_modes seccomp_modes.c

# Run interactive menu
./seccomp_modes

# Menu:
# 1) Demonstrate DISABLED mode
# 2) Demonstrate STRICT mode (kills process!)
# 3) Demonstrate FILTER mode
# 4) Exit
```

---

## Key Differences

### STRICT vs FILTER

| Aspect | STRICT | FILTER |
|--------|--------|--------|
| Flexibility | Fixed (4 syscalls) | Custom per syscall |
| Action | Always KILL | ALLOW/ERRNO/TRAP/KILL |
| Usability | Very limited | Production-ready |
| Configuration | None needed | Requires BPF program |
| When to use | Pure computation | Sandboxing, containers |

### Cannot Be Removed

```c
/* Once enabled, SECCOMP cannot be disabled! */
prctl(PR_SET_SECCOMP, SECCOMP_MODE_STRICT);

/* This will fail: */
prctl(PR_SET_SECCOMP, SECCOMP_MODE_DISABLED);  // ✗ Not allowed!

/* Even this fails: */
prctl(PR_SET_NO_NEW_PRIVS, 0);  // ✗ Cannot undo!
```

**Reason:** Security feature - prevents malware from removing restrictions.

---

## Inheritance

```c
/* Parent enables SECCOMP */
prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog);

fork();  /* Child inherits parent's filter */

/* Child also has SECCOMP enabled */
/* Child cannot remove it */
/* Child's children inherit it too */
```

**All descendants are restricted!**

---

## Checking SECCOMP Mode

### From userspace:
```bash
# Check process status
grep Seccomp /proc/<PID>/status

# Output:
#   Seccomp: 0  (DISABLED)
#   Seccomp: 1  (STRICT)
#   Seccomp: 2  (FILTER)

# Check your shell:
grep Seccomp /proc/self/status
```

### From kernel module:
```c
/* In kernel code */
struct task_struct *task = current;
int mode = task->seccomp.mode;

if (mode == SECCOMP_MODE_FILTER) {
    /* Has BPF filter */
    struct seccomp_filter *filter = task->seccomp.filter;
}
```

---

## Summary

**Choose SECCOMP mode based on needs:**

- **Mode 0 (DISABLED)**: Normal processes, no restrictions needed
- **Mode 1 (STRICT)**: Pure computation, minimal I/O, maximum security
- **Mode 2 (FILTER)**: Modern approach, used by Docker/Chrome/systemd

**Mode 2 (FILTER) is the practical choice** for real-world sandboxing!

---

## Further Reading

- `man 2 seccomp` - Detailed syscall documentation
- `man 2 prctl` - PR_SET_SECCOMP options
- Docker seccomp profile: `/usr/share/docker/seccomp.json`
- Chrome sandbox: `chrome://sandbox`
- systemd: `man systemd.exec` (SystemCallFilter)
