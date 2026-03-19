# Linux Keyring Subsystem Examples

This directory contains examples demonstrating the Linux kernel keyring subsystem for secure key management.

## Overview

The Linux keyring subsystem provides a secure mechanism for storing and managing:
- Credentials (passwords, tokens)
- Encryption keys (filesystem, network)
- Certificates and authentication data
- Session-specific secrets

## Example Order

Follow these examples in order for the best learning experience:

### 1. keyring.c - Kernel Module (Start Here)
**Purpose**: Understand the keyring subsystem from kernel perspective

**What you'll learn**:
- Keyring hierarchy (thread → process → session → user)
- Key types (user, logon, keyring, encrypted, trusted)
- Key search order
- Permission model
- Real-world use cases

**Why first**: Provides foundational understanding of how keyrings work at the kernel level before manipulating them from userspace.

**Usage**:
```bash
cd /home/raghub/raghu/workspace/repos/lkp0725/security/part2
make                    # Build the kernel module
sudo insmod keyring.ko  # Load module
cat /proc/keyring       # View keyring information
sudo rmmod keyring      # Unload module
```

---

### 2. key_ops.c - Basic Userspace Operations
**Purpose**: Learn fundamental keyring operations from userspace

**What you'll learn**:
- Creating keys in different keyrings
- Reading key payload data
- Searching for keys by description
- Listing keyring contents
- Inspecting key metadata

**Why second**: Natural progression from observing (keyring.c) to performing basic operations.

**Usage**:
```bash
gcc -o key_ops key_ops.c -lkeyutils
./key_ops
```

**Key operations demonstrated**:
- `add_key()` - Create/update keys
- `keyctl_read()` - Read key payloads
- `keyctl_search()` - Find keys by description
- `keyctl_describe()` - Get key metadata

---

### 3. keyctl.c - Advanced Operations
**Purpose**: Master advanced key management features

**What you'll learn**:
- Key timeouts and automatic expiration
- Permission management (possessor/user/group/other)
- Ownership changes (chown)
- Linking keys between keyrings
- Unlinking keys from keyrings
- Key revocation and invalidation

**Why third**: Builds on basic operations with complex lifecycle management and security features.

**Usage**:
```bash
gcc -o keyctl keyctl.c -lkeyutils
./keyctl
```

**Advanced features**:
- `keyctl_set_timeout()` - Auto-expire keys
- `keyctl_setperm()` - Set permissions
- `keyctl_chown()` - Change ownership
- `keyctl_link()` / `keyctl_unlink()` - Manage key references
- `keyctl_revoke()` - Invalidate keys permanently

---

### 4. test_keyring.c - Test Suite (Validation)
**Purpose**: Verify keyring operations work correctly

**What you'll learn**:
- Automated testing patterns
- Error handling best practices
- Operation validation
- Edge case handling

**Why last**: Validates understanding and serves as reference implementation.

**Usage**:
```bash
gcc -o test_keyring test_keyring.c -lkeyutils
./test_keyring
```

**Tests covered**:
- Key creation
- Key reading
- Key searching
- Key updating
- Permission checks
- Key linking
- Key revocation

---

## Key Concepts

### Keyring Hierarchy
```
Thread Keyring    (most specific - single thread)
    ↓
Process Keyring   (all threads in process)
    ↓
Session Keyring   (all processes in session) ← Most commonly used
    ↓
User Keyring      (persistent across sessions)
    ↓
User Session      (per-user default)
```

### Key Types
- **user**: General-purpose, payload readable from userspace
- **logon**: Like user but payload not readable (more secure)
- **keyring**: Container for other keys
- **encrypted**: Encrypted key stored in kernel
- **trusted**: Hardware-backed keys (TPM)

### Key Search Order
When searching for a key, the kernel searches in this order:
1. Thread keyring
2. Process keyring
3. Session keyring
4. User keyring

**First match wins!**

### Permission Model
Keys use 4-level permissions (similar to file permissions):
- **Possessor**: Process that currently possesses the key
- **User**: Owner (UID) of the key
- **Group**: Group (GID) of the key
- **Other**: Everyone else

Each level can have:
- VIEW (0x01) - See that key exists
- READ (0x02) - Read key payload
- WRITE (0x04) - Update key payload
- SEARCH (0x08) - Find key in searches
- LINK (0x10) - Link key into keyrings
- SETATTR (0x20) - Change key attributes

### Keyring Specifiers (@-notation)
- `@s` = Session keyring
- `@p` = Process keyring
- `@t` = Thread keyring
- `@u` = User keyring
- `@us` = User session keyring

---

## Practical Use Cases

1. **Kerberos Tickets**: Store authentication tickets
2. **Filesystem Encryption**: Keys for ecryptfs, fscrypt
3. **NFS Authentication**: Secure NFS mount credentials
4. **DNS Resolver Cache**: Cache DNS credentials
5. **Container Secrets**: Isolated secrets for containers
6. **Session Tokens**: OAuth tokens with auto-expiration

---

## Building All Examples

```bash
# Build kernel module
make

# Build userspace programs
gcc -o key_ops key_ops.c -lkeyutils
gcc -o keyctl keyctl.c -lkeyutils
gcc -o test_keyring test_keyring.c -lkeyutils
```

---

## Useful Commands

### View all keyrings and keys
```bash
keyctl show
```

### Add a key to session keyring
```bash
keyctl add user mykey "secret_data" @s
```

### Search for a key
```bash
keyctl search @s user mykey
```

### Read a key (by ID)
```bash
keyctl print <key_id>
```

### Describe a key
```bash
keyctl describe <key_id>
```

### Set key timeout (seconds)
```bash
keyctl timeout <key_id> 60
```

### Revoke a key
```bash
keyctl revoke <key_id>
```

### Clear all keys from session keyring
```bash
keyctl clear @s
```

### Unlink a key from keyring
```bash
keyctl unlink <key_id> @s
```

---

## Prerequisites

### Kernel Configuration
Ensure your kernel has keyring support enabled:
```
CONFIG_KEYS=y
```

### Required Packages
```bash
# Debian/Ubuntu
sudo apt-get install keyutils libkeyutils-dev

# RHEL/CentOS/Fedora
sudo yum install keyutils keyutils-libs-devel
```

---

## Security Considerations

1. **Non-swappable Memory**: Keys are stored in kernel memory that cannot be swapped to disk
2. **Timeouts**: Use timeouts for temporary credentials to limit exposure window
3. **Permissions**: Use restrictive permissions (possessor-only when possible)
4. **Logon vs User**: Use "logon" type for sensitive data that shouldn't be read back
5. **Revocation**: Revoke compromised keys immediately - it's irreversible
6. **Cleanup**: Always unlink/revoke keys when no longer needed

---

## Learning Path

```
1. Read KEYRING_GUIDE.md for detailed concepts
2. Run keyring.c kernel module to observe system state
3. Work through key_ops.c for basic operations
4. Study keyctl.c for advanced features
5. Run test_keyring.c to validate understanding
6. Review TESTING.md for testing strategies
```

---

## Additional Resources

- **KEYRING_GUIDE.md**: Comprehensive guide to keyring concepts
- **TESTING.md**: Testing procedures and examples
- **Kernel Documentation**: `Documentation/security/keys/core.rst`
- **Man Pages**: `man keyrings`, `man keyctl`, `man add_key`

---

## Common Issues

### "Operation not permitted" errors
- Check key permissions with `keyctl describe <key_id>`
- Verify you own the key or have appropriate permissions
- Some operations require root privileges

### Keys not found in searches
- Check search order (thread → process → session → user)
- Verify key is linked into the keyring you're searching
- Ensure key description matches exactly (case-sensitive)

### Module loading fails
- Verify kernel has CONFIG_KEYS enabled
- Check `dmesg` for error messages
- Ensure you have CAP_SYS_MODULE capability (usually root)

---

## Author

Raghu Bharadwaj (TECH VEDA - www.techveda.org)

## License

This software is dual-licensed under the MIT License and GPL v2.
