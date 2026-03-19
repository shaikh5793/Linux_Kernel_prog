/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

/*
 * - Capabilities divide root privileges into ~40 distinct units
 * - Each process has 5 capability sets:
 *   1. Effective: Currently active capabilities (used for permission checks)
 *   2. Permitted: Capabilities that can be made effective
 *   3. Inheritable: Capabilities preserved across exec (if file permits)
 *   4. Bounding: Limit on capabilities that can be acquired
 *   5. Ambient: Auto-inherited capabilities (newer feature)
 *
 * COMMON CAPABILITIES:
 * - CAP_NET_BIND_SERVICE: Bind to ports < 1024
 * - CAP_SYS_ADMIN: Broad administrative capabilities
 * - CAP_DAC_OVERRIDE: Bypass file permission checks
 * - CAP_SETUID/CAP_SETGID: Change UID/GID
 * - CAP_SYS_MODULE: Load/unload kernel modules
 * - CAP_SYS_RAWIO: Raw I/O port access
 *
 * CHECKING CAPABILITIES:
 *   capable(CAP_SYS_ADMIN)  - Does current task have this capability?
 *   ns_capable()            - Namespace-aware capability check
 *   cap_raised()            - Is capability set in a capability set?
 *
 */

#include <linux/module.h>      
#include <linux/kernel.h>      
#include <linux/init.h>        
#include <linux/proc_fs.h>     
#include <linux/seq_file.h>    
#include <linux/cred.h>        
#include <linux/capability.h>  
#include <linux/security.h>   

#define PROC_FILENAME "cap_info"

/*
 * decode_cap_set - Decode and display capabilities in a capability set
 * @m: seq_file for output
 * @name: Name of this capability set (Effective, Permitted, etc.)
 * @cap_set: The capability set to decode
 *
 * Capabilities are stored as a bitmask. Each bit represents one capability.
 * This function decodes the bitmask and prints human-readable capability names.
 *
 * The kernel_cap_t type is a structure containing a u64 value (cap_set.val)
 * with up to 64 bits for different capabilities (currently ~40 are defined).
 */
static void decode_cap_set(struct seq_file *m, const char *name,
			   kernel_cap_t cap_set)
{
	int i;
	int count = 0;

	/* Display capability set as hexadecimal bitmask */
	seq_printf(m, "\n%s: 0x%llx\n", name, cap_set.val);

	if (cap_set.val == 0) {
		seq_printf(m, "  (no capabilities)\n");
		return;
	}

	/*
	 * Check each capability bit
	 * CAP_LAST_CAP is the highest defined capability number
	 * Iterate through all possible capabilities and check if bit is set
	 */
	for (i = 0; i <= CAP_LAST_CAP; i++) {
		/* Test if bit i is set in the capability bitmask */
		if (cap_set.val & (1ULL << i)) {
			seq_printf(m, "  [%d] ", i);

			/*
			 * Print human-readable capability name
			 * Each CAP_* constant represents a specific privilege
			 */
			switch (i) {
			case CAP_CHOWN:           seq_printf(m, "CAP_CHOWN"); break;  /* Change file ownership */
			case CAP_DAC_OVERRIDE:    seq_printf(m, "CAP_DAC_OVERRIDE"); break;  /* Bypass file read/write/exec checks */
			case CAP_DAC_READ_SEARCH: seq_printf(m, "CAP_DAC_READ_SEARCH"); break;  /* Bypass file read and directory search checks */
			case CAP_FOWNER:          seq_printf(m, "CAP_FOWNER"); break;  /* Bypass permission checks on operations requiring file ownership */
			case CAP_FSETID:          seq_printf(m, "CAP_FSETID"); break;  /* Don't clear setuid/setgid on modification */
			case CAP_KILL:            seq_printf(m, "CAP_KILL"); break;  /* Send signals to any process */
			case CAP_SETGID:          seq_printf(m, "CAP_SETGID"); break;  /* Arbitrary GID manipulation */
			case CAP_SETUID:          seq_printf(m, "CAP_SETUID"); break;  /* Arbitrary UID manipulation */
			case CAP_SETPCAP:         seq_printf(m, "CAP_SETPCAP"); break;
			case CAP_LINUX_IMMUTABLE: seq_printf(m, "CAP_LINUX_IMMUTABLE"); break;
			case CAP_NET_BIND_SERVICE:seq_printf(m, "CAP_NET_BIND_SERVICE"); break;
			case CAP_NET_BROADCAST:   seq_printf(m, "CAP_NET_BROADCAST"); break;
			case CAP_NET_ADMIN:       seq_printf(m, "CAP_NET_ADMIN"); break;
			case CAP_NET_RAW:         seq_printf(m, "CAP_NET_RAW"); break;
			case CAP_IPC_LOCK:        seq_printf(m, "CAP_IPC_LOCK"); break;
			case CAP_IPC_OWNER:       seq_printf(m, "CAP_IPC_OWNER"); break;
			case CAP_SYS_MODULE:      seq_printf(m, "CAP_SYS_MODULE"); break;
			case CAP_SYS_RAWIO:       seq_printf(m, "CAP_SYS_RAWIO"); break;
			case CAP_SYS_CHROOT:      seq_printf(m, "CAP_SYS_CHROOT"); break;
			case CAP_SYS_PTRACE:      seq_printf(m, "CAP_SYS_PTRACE"); break;
			case CAP_SYS_PACCT:       seq_printf(m, "CAP_SYS_PACCT"); break;
			case CAP_SYS_ADMIN:       seq_printf(m, "CAP_SYS_ADMIN"); break;
			case CAP_SYS_BOOT:        seq_printf(m, "CAP_SYS_BOOT"); break;
			case CAP_SYS_NICE:        seq_printf(m, "CAP_SYS_NICE"); break;
			case CAP_SYS_RESOURCE:    seq_printf(m, "CAP_SYS_RESOURCE"); break;
			case CAP_SYS_TIME:        seq_printf(m, "CAP_SYS_TIME"); break;
			case CAP_SYS_TTY_CONFIG:  seq_printf(m, "CAP_SYS_TTY_CONFIG"); break;
			case CAP_MKNOD:           seq_printf(m, "CAP_MKNOD"); break;
			case CAP_LEASE:           seq_printf(m, "CAP_LEASE"); break;
			case CAP_AUDIT_WRITE:     seq_printf(m, "CAP_AUDIT_WRITE"); break;
			case CAP_AUDIT_CONTROL:   seq_printf(m, "CAP_AUDIT_CONTROL"); break;
			case CAP_SETFCAP:         seq_printf(m, "CAP_SETFCAP"); break;
			case CAP_MAC_OVERRIDE:    seq_printf(m, "CAP_MAC_OVERRIDE"); break;
			case CAP_MAC_ADMIN:       seq_printf(m, "CAP_MAC_ADMIN"); break;
			case CAP_SYSLOG:          seq_printf(m, "CAP_SYSLOG"); break;
			case CAP_WAKE_ALARM:      seq_printf(m, "CAP_WAKE_ALARM"); break;
			case CAP_BLOCK_SUSPEND:   seq_printf(m, "CAP_BLOCK_SUSPEND"); break;
			case CAP_AUDIT_READ:      seq_printf(m, "CAP_AUDIT_READ"); break;
			default:                  seq_printf(m, "CAP_%d", i); break;
			}
			seq_printf(m, "\n");
			count++;
		}
	}
	
	seq_printf(m, "  Total: %d capabilities\n", count);
}


/*
 * cap_show - Display process capability information
 *
 * Called when: User reads /proc/cap_info
 *
 * Shows all five capability sets for the reading process.
 * This helps understand what privileges the current process has.
 *
 */
static int cap_show(struct seq_file *m, void *v)
{
	const struct cred *cred;

	/* Get current process credentials (read-only) */
	cred = current_cred();

	seq_printf(m, "Process: %s (PID: %d)\n", current->comm, current->pid);
	seq_printf(m, "UID: %u, EUID: %u\n", cred->uid.val, cred->euid.val);

	/*
	 * Decode and display all five capability sets
	 * Each set is stored as a bitmask in the cred structure
	 */
	decode_cap_set(m, "Effective Set", cred->cap_effective);
	decode_cap_set(m, "Permitted Set", cred->cap_permitted);
	decode_cap_set(m, "Inheritable Set", cred->cap_inheritable);
	decode_cap_set(m, "Bounding Set", cred->cap_bset);
	decode_cap_set(m, "Ambient Set", cred->cap_ambient);

	return 0;
}

static int cap_open(struct inode *inode, struct file *file)
{
	return single_open(file, cap_show, NULL);
}

static const struct proc_ops cap_fops = {
	.proc_open = cap_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int __init cap_init(void)
{
	struct proc_dir_entry *entry;

	entry = proc_create(PROC_FILENAME, 0444, NULL, &cap_fops);
	if (!entry)
		return -ENOMEM;

	pr_info("Capabilities: /proc/%s created\n", PROC_FILENAME);
	return 0;
}

static void __exit cap_exit(void)
{
	remove_proc_entry(PROC_FILENAME, NULL);
}

module_init(cap_init);
module_exit(cap_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("Capabilities information");
MODULE_VERSION("1.0");
