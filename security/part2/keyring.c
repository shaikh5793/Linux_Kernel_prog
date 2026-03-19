/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

/*
 * keyring.c - Linux Kernel Keyring Subsystem Information
 *
 * The Linux kernel keyring subsystem provides secure key management for:
 * - Credentials (passwords, tokens)
 * - Encryption keys (filesystem, network)
 * - Certificates and authentication data
 * - Session-specific secrets
 *
 * KEY CONCEPTS:
 *
 * 1. KEYRING HIERARCHY (from most specific to most general):
 *    - Thread Keyring:  Specific to a single thread
 *    - Process Keyring: Shared by all threads in a process
 *    - Session Keyring: Shared by all processes in a session (default)
 *    - User Keyring:    Persistent across all user's sessions
 *    - User Session:    Per-user default session keyring
 *
 * 2. KEY TYPES:
 *    - user:      General-purpose user data (passwords, tokens)
 *    - logon:     Similar to user but cannot be read from userspace
 *    - keyring:   Container for other keys
 *    - encrypted: Encrypted key stored in kernel
 *    - trusted:   Hardware-backed trusted keys (TPM)
 *
 * 3. KEY SEARCH ORDER:
 *    When looking for a key, kernel searches in this order:
 *    Thread → Process → Session → User keyring
 *    First match wins!
 *
 * 4. KEY PERMISSIONS:
 *    Similar to file permissions but for keys:
 *    - Possessor: Process that has access to the key
 *    - User:      Owner of the key
 *    - Group:     Group owner of the key
 *    - Other:     Everyone else
 *
 *    Each can have: View, Read, Write, Search, Link, SetAttr
 *
 * 5. USE CASES:
 *    - Kerberos tickets
 *    - Filesystem encryption keys (ecryptfs, fscrypt)
 *    - NFS authentication
 *    - DNS resolver cache
 *    - Container secrets
 *
 * USAGE:
 *   sudo insmod keyring.ko
 *   cat /proc/keyring
 *   sudo rmmod keyring
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/key.h>         /* Core key structures */
#include <linux/keyctl.h>      /* Key control operations */
#include <linux/cred.h>        /* Process credentials */
#include <linux/sched.h>       /* Task structures */
#include <keys/user-type.h>    /* User key type */

#define PROC_NAME "keyring"

static int enabled = 1;
module_param(enabled, int, 0644);
MODULE_PARM_DESC(enabled, "Enable/disable keyring display");

/*
 * proc_show - Display current process's keyring state
 *
 * Shows which keyrings are attached to the reading process.
 * This demonstrates the keyring hierarchy from kernel perspective.
 *
 * KEYRING ALLOCATION:
 * - Keyrings are allocated on-demand (lazy allocation)
 * - Thread keyring: Only created when specifically requested
 * - Process keyring: Created when first key is added to it
 * - Session keyring: Usually inherited from parent or init
 *
 * KEY STRUCTURE FIELDS:
 * - key->serial: Unique identifier for this key (like inode number)
 * - key->type: Type of key (user, keyring, logon, etc.)
 * - key->uid/gid: Owner and group
 * - key->perm: Permission bits (similar to file mode)
 * - key->payload: Actual key data
 *
 * The current_cred() function returns the credentials structure
 * which contains pointers to the process's keyrings.
 */
static int proc_show(struct seq_file *m, void *v)
{
	const struct cred *cred;
	struct key *key;

	if (!enabled)
		return 0;

	/*
	 * Get current process credentials
	 * This is a read-only reference - use current_cred() not get_current_cred()
	 * since we don't need to modify or hold a reference
	 */
	cred = current_cred();

	seq_printf(m, "Process: %s (PID: %d)\n", current->comm, current->pid);
	seq_printf(m, "UID: %u, GID: %u\n\n",
		   from_kuid(&init_user_ns, cred->uid),
		   from_kgid(&init_user_ns, cred->gid));

	/*
	 * Thread Keyring (most specific)
	 * - Specific to this thread only
	 * - Not shared with other threads in same process
	 * - Rarely used in practice
	 * - Created only when explicitly requested
	 */
	key = cred->thread_keyring;
	seq_printf(m, "Thread Keyring:  %s",
		   key ? "Present" : "Not allocated");
	if (key)
		seq_printf(m, " (ID: %d)", key->serial);
	seq_printf(m, "\n");

	/*
	 * Process Keyring
	 * - Shared by all threads in this process
	 * - Useful for process-specific secrets
	 * - Not inherited by child processes
	 * - Dies when process exits
	 */
	key = cred->process_keyring;
	seq_printf(m, "Process Keyring: %s",
		   key ? "Present" : "Not allocated");
	if (key)
		seq_printf(m, " (ID: %d)", key->serial);
	seq_printf(m, "\n");

	/*
	 * Session Keyring (most commonly used)
	 * - Shared by all processes in the session
	 * - Inherited by child processes (fork/exec)
	 * - Can be replaced with keyctl(KEYCTL_JOIN_SESSION_KEYRING)
	 * - This is where most user keys live
	 * - Persists until last process in session exits
	 */
	key = cred->session_keyring;
	seq_printf(m, "Session Keyring: %s",
		   key ? "Present" : "Not allocated");
	if (key)
		seq_printf(m, " (ID: %d)", key->serial);
	seq_printf(m, "\n\n");

	seq_printf(m, "=== Userspace Commands ===\n");
	seq_printf(m, "View all keyrings:\n");
	seq_printf(m, "  keyctl show\n\n");
	seq_printf(m, "Add a key to session keyring:\n");
	seq_printf(m, "  keyctl add user mykey \"secret_data\" @s\n\n");
	seq_printf(m, "Search for a key:\n");
	seq_printf(m, "  keyctl search @s user mykey\n");

	return 0;
}

static int proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_show, NULL);
}

static const struct proc_ops proc_fops = {
	.proc_open = proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static struct proc_dir_entry *proc_entry;

static int __init keyring_init(void)
{
	proc_entry = proc_create(PROC_NAME, 0444, NULL, &proc_fops);
	if (!proc_entry)
		return -ENOMEM;

	pr_info("Keyring: /proc/%s created\n", PROC_NAME);
	return 0;
}

static void __exit keyring_exit(void)
{
	if (proc_entry)
		proc_remove(proc_entry);
}

module_init(keyring_init);
module_exit(keyring_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("Keyring subsystem demonstration");
MODULE_VERSION("1.0");

