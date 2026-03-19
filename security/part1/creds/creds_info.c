/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

/*
 * creds_basic.c - Basic credential structure
 *
 * Every process in Linux has credentials (struct cred) that define its identity.
 * These credentials determine what the process can access and what it can do.
 */

#include <linux/module.h>     
#include <linux/kernel.h>     
#include <linux/proc_fs.h>    
#include <linux/seq_file.h>   
#include <linux/cred.h>       
#include <linux/uidgid.h>    


#define PROC_NAME "creds_basic"

/*
 * proc_show - Display current process credentials
 *
 * This function demonstrates how to:
 * 1. Get current process credentials
 * 2. Access different UID/GID types
 * 3. Check user namespace
 *
 * Called when: User reads /proc/creds_basic
 */
static int proc_show(struct seq_file *m, void *v)
{
	/*
	 * Get current process credentials (read-only)
	 * - current_cred() returns const struct cred*
	 * - Safe to use without locking (RCU protected)
	 * - Never NULL for a running process
	 */
	const struct cred *cred = current_cred();

	/* Show which process is reading this file */
	seq_printf(m, "Process: %s (PID: %d)\n\n", current->comm, current->pid);

	/*
	 * FOUR TYPES OF UIDs:
	 *
	 * 1. Real UID (uid): Who really owns this process
	 *    - Set at login, rarely changes
	 *    - Used for accounting (who to bill CPU time to)
	 *
	 * 2. Effective UID (euid): Permission checks use this
	 *    - This is what matters for access control
	 *    - Can differ from real UID (setuid programs)
	 *
	 * 3. Saved UID (suid): Backup of previous effective UID
	 *    - Allows setuid programs to drop/regain privileges
	 *    - Example: su drops to user, can regain root if needed
	 *
	 * 4. Filesystem UID (fsuid): Used for filesystem operations
	 *    - Usually same as euid
	 *    - Linux-specific extension
	 *    - Allows NFS servers to access files on behalf of users
	 */
	seq_printf(m, "User IDs:\n");
	seq_printf(m, "  Real      (uid):  %u\n", cred->uid.val);
	seq_printf(m, "  Effective (euid): %u\n", cred->euid.val);
	seq_printf(m, "  Saved     (suid): %u\n", cred->suid.val);
	seq_printf(m, "  Filesystem(fsuid):%u\n\n", cred->fsuid.val);

	/*
	 * FOUR TYPES OF GIDs:
	 * - Same concepts as UIDs, but for groups
	 * - Multiple groups possible (supplementary groups)
	 * - Group permissions checked after user permissions
	 */
	seq_printf(m, "Group IDs:\n");
	seq_printf(m, "  Real      (gid):  %u\n", cred->gid.val);
	seq_printf(m, "  Effective (egid): %u\n", cred->egid.val);
	seq_printf(m, "  Saved     (sgid): %u\n", cred->sgid.val);
	seq_printf(m, "  Filesystem(fsgid):%u\n\n", cred->fsgid.val);

	/*
	 * USER NAMESPACES:
	 * - Allow UID/GID mapping between namespaces
	 * - Container processes can have UID 0 inside container
	 *   but different UID on host
	 * - init_user_ns is the global namespace
	 */
	seq_printf(m, "User Namespace: %p\n", cred->user_ns);
	seq_printf(m, "Is init namespace: %s\n",
		   cred->user_ns == &init_user_ns ? "yes" : "no");

	return 0; /* Success */
}

/*
 * proc_open - Called when /proc/creds_basic is opened
 *
 * single_open() is a helper for simple proc files
 * - Allocates buffer automatically
 * - Calls proc_show() once
 * - Handles memory management
 */
static int proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_show, NULL);
}

/*
 * File operations for /proc/creds_basic
 *
 * Uses proc_ops (kernel 5.6+) instead of file_operations
 * - Lighter weight for proc files
 * - Removes unnecessary operations
 */
static const struct proc_ops proc_fops = {
	.proc_open = proc_open,           /* Open file */
	.proc_read = seq_read,            /* Read using seq_file */
	.proc_lseek = seq_lseek,          /* Seek in file */
	.proc_release = single_release,   /* Close and cleanup */
};

/* Proc entry handle - saved for cleanup */
static struct proc_dir_entry *proc_entry;

static int __init creds_basic_init(void)
{
	/* Create the proc file */
	proc_entry = proc_create(PROC_NAME, 0444, NULL, &proc_fops);
	if (!proc_entry)
		return -ENOMEM;  /* Out of memory */

	pr_info("Creds Basic: /proc/%s created\n", PROC_NAME);
	return 0;
}

static void __exit creds_basic_exit(void)
{
	if (proc_entry)
		proc_remove(proc_entry);

	pr_info("Creds Basic: /proc/%s removed\n", PROC_NAME);
}

module_init(creds_basic_init);
module_exit(creds_basic_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("Basic credential structure");
MODULE_VERSION("1.0");

