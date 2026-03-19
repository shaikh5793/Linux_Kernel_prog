# Testing Real vs Effective UID

## Understanding the Difference

### Real UID (uid)
- The **actual user** who owns the process
- Set when you log in, rarely changes
- Used for accounting (who to charge for CPU time)

### Effective UID (euid)
- Used for **permission checks**
- Can be different from real UID (via setuid bit)
- This is what the kernel checks for access control

### Saved UID (suid)
- Backup of the previous effective UID
- Allows setuid programs to drop/regain privileges

## Why `sudo` doesn't work for testing

When you run `sudo cat /proc/creds_basic`:
```
Real UID:      0 (changed to root)
Effective UID: 0 (root)
Saved UID:     0 (root)
```

**All UIDs become 0** because `sudo` completely switches to root.

## Proper Testing with Setuid Binary

### Step 1: Compile the test program
```bash
cd /home/raghub/raghu/workspace/repos/lkp0725/security/part1
gcc -o test_creds test_creds.c
```

### Step 2: Make it a setuid binary
```bash
# Make root the owner
sudo chown root:root test_creds

# Set the setuid bit (u+s)
sudo chmod u+s test_creds

# Verify the 's' bit is set
ls -l test_creds
# Should show: -rwsr-xr-x ... root root ... test_creds
#                 ^ this 's' means setuid bit is set
```

### Step 3: Run as regular user
```bash
./test_creds
```

### Expected Output
```
=== User Space Credentials ===
Real UID:      1001 (your actual UID)
Effective UID: 0    (root, due to setuid bit)
Saved UID:     0    (backup of euid)

Real GID:      1001
Effective GID: 1001
Saved GID:     1001

=== Kernel Space Credentials ===
Process: test_creds (PID: 12345)

User IDs:
  Real      (uid):  1001  <- YOUR user
  Effective (euid): 0     <- ROOT privileges
  Saved     (suid): 0
  Filesystem(fsuid):0

Group IDs:
  Real      (gid):  1001
  Effective (egid): 1001
  Saved     (sgid): 1001
  Filesystem(fsgid):1001
```

## Key Observations

1. **Real UID = 1001**: Shows you (the actual user) ran the program
2. **Effective UID = 0**: The program has root privileges for permission checks
3. **This is how tools like `passwd` work**: They run as root to modify `/etc/shadow`, but still know who the real user is

## Real-World Examples of Setuid Programs

```bash
# Find setuid programs on your system
find /usr/bin -perm -4000 -ls 2>/dev/null | head -5
```

Common setuid programs:
- `/usr/bin/passwd` - needs root to modify password files
- `/usr/bin/sudo` - needs root to run commands as root
- `/usr/bin/su` - needs root to switch users
- `/usr/bin/ping` - needs root for raw sockets (some systems)

## Cleanup

When done testing:
```bash
# Remove setuid bit (important for security!)
sudo chmod u-s test_creds

# Or delete the test program
rm test_creds
```

## Security Note

⚠️ **Never leave setuid programs lying around unless necessary!**
They can be security risks if they have vulnerabilities.
