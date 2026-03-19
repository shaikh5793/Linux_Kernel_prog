/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

/*
 * apparmor_info.c - AppArmor LSM information
 *
 * - enforce: Denies access and logs violations
 * - complain: Allows access but logs what would be denied
 * - unconfined: No restrictions
 *
 * CHECKING APPARMOR:
 *   cat /sys/kernel/security/lsm                    # Check active LSMs
 *   aa-status                                       # AppArmor status
 *   cat /sys/kernel/security/apparmor/profiles      # Loaded profiles
 *
 * EXAMPLE PROFILE:
 *   /etc/apparmor.d/usr.bin.firefox - Restricts Firefox browser
 *   Defines which files Firefox can read, write, execute
 */

#include <linux/module.h>     
#include <linux/kernel.h>     
#include <linux/proc_fs.h>    
#include <linux/seq_file.h>   
#include <linux/fs.h>         

#define PROC_NAME "apparmor_info"

/*
 * Read a file into a buffer with size limit
 * Returns number of bytes read, or negative on error
 */
static ssize_t read_file_content(const char *path, char *buf, size_t size)
{
	struct file *fp;
	ssize_t len;
	loff_t pos = 0;

	fp = filp_open(path, O_RDONLY, 0);
	if (IS_ERR(fp))
		return PTR_ERR(fp);

	len = kernel_read(fp, buf, size - 1, &pos);
	if (len > 0)
		buf[len] = '\0';

	filp_close(fp, NULL);
	return len;
}

/*
 * Parse and display loaded AppArmor profiles with modes
 * Format in /sys/kernel/security/apparmor/profiles:
 *   profile_name (mode)
 * where mode is: enforce, complain, kill, or unconfined
 */
static void show_loaded_profiles(struct seq_file *m)
{
	char *buf, *line, *next;
	ssize_t len;
	int count = 0;

	buf = kmalloc(4096, GFP_KERNEL);
	if (!buf)
		return;

	len = read_file_content("/sys/kernel/security/apparmor/profiles", buf, 4096);
	if (len <= 0) {
		kfree(buf);
		return;
	}

	seq_printf(m, "\n=== LOADED APPARMOR PROFILES ===\n");
	seq_printf(m, "%-40s %s\n", "Profile Name", "Mode");
	seq_printf(m, "%-40s %s\n", "------------", "----");

	line = buf;
	while (line && *line && count < 20) {  /* Show first 20 profiles */
		next = strchr(line, '\n');
		if (next)
			*next = '\0';

		if (strlen(line) > 0) {
			seq_printf(m, "%s\n", line);
			count++;
		}

		line = next ? next + 1 : NULL;
	}

	if (count == 0)
		seq_printf(m, "  (No profiles loaded)\n");
	else
		seq_printf(m, "\nTotal profiles shown: %d\n", count);

	kfree(buf);
}

/*
 * Get current process's AppArmor confinement status
 * Reads from /proc/self/attr/current which shows the profile
 * for the current task
 */
static void show_current_profile(struct seq_file *m)
{
	char *buf;
	struct file *fp;
	ssize_t len;
	loff_t pos = 0;
	char path[64];

	buf = kmalloc(256, GFP_KERNEL);
	if (!buf)
		return;

	/* Read current process's AppArmor profile */
	snprintf(path, sizeof(path), "/proc/%d/attr/current", current->pid);
	fp = filp_open(path, O_RDONLY, 0);

	seq_printf(m, "\n=== CURRENT PROCESS CONFINEMENT ===\n");
	seq_printf(m, "Process: %s (PID: %d)\n", current->comm, current->pid);

	if (!IS_ERR(fp)) {
		len = kernel_read(fp, buf, 255, &pos);
		if (len > 0) {
			buf[len] = '\0';
			/* Remove trailing newline */
			if (buf[len-1] == '\n')
				buf[len-1] = '\0';

			seq_printf(m, "AppArmor Profile: %s\n", buf);

			/* Analyze confinement status */
			if (strstr(buf, "unconfined"))
				seq_printf(m, "Status: UNCONFINED (No restrictions)\n");
			else if (strstr(buf, "enforce"))
				seq_printf(m, "Status: ENFORCING (Violations blocked)\n");
			else if (strstr(buf, "complain"))
				seq_printf(m, "Status: COMPLAIN MODE (Logging only)\n");
			else
				seq_printf(m, "Status: CONFINED\n");
		}
		filp_close(fp, NULL);
	} else {
		seq_printf(m, "AppArmor Profile: Not available\n");
	}

	kfree(buf);
}

/*
 * Display AppArmor kernel module parameters and features
 */
static void show_apparmor_features(struct seq_file *m)
{
	char buf[256];
	ssize_t len;

	seq_printf(m, "\n=== APPARMOR FEATURES ===\n");

	/* Check if AppArmor is enabled */
	len = read_file_content("/sys/module/apparmor/parameters/enabled", buf, sizeof(buf));
	if (len > 0) {
		buf[strcspn(buf, "\n")] = '\0';
		seq_printf(m, "Enabled: %s\n", buf);
	}

	/* Check audit mode */
	len = read_file_content("/sys/module/apparmor/parameters/audit", buf, sizeof(buf));
	if (len > 0) {
		buf[strcspn(buf, "\n")] = '\0';
		seq_printf(m, "Audit Mode: %s\n", buf);
	}

	/* Check which LSMs are active */
	len = read_file_content("/sys/kernel/security/lsm", buf, sizeof(buf));
	if (len > 0) {
		buf[strcspn(buf, "\n")] = '\0';
		seq_printf(m, "Active LSMs: %s\n", buf);

		if (strstr(buf, "apparmor"))
			seq_printf(m, "  -> AppArmor is ACTIVE in LSM stack\n");
		else
			seq_printf(m, "  -> AppArmor NOT in LSM stack\n");
	}
}

/*
 * proc_show - Display comprehensive AppArmor information
 *
 * Called when: User reads /proc/apparmor_info
 *
 * Shows:
 * 1. Current process confinement status
 * 2. Loaded profiles with modes
 * 3. AppArmor features and configuration
 *
 * DEMONSTRATION VALUE:
 * - Shows practical AppArmor enforcement in action
 * - Lists actual confined processes on the system
 * - Displays profile modes (enforce vs complain)
 */
static int proc_show(struct seq_file *m, void *v)
{
	struct file *fp;

	seq_printf(m, "╔══════════════════════════════════════════════════════╗\n");
	seq_printf(m, "║         APPARMOR MANDATORY ACCESS CONTROL            ║\n");
	seq_printf(m, "╚══════════════════════════════════════════════════════╝\n");

	/*
	 * First check if AppArmor securityfs is available
	 */
	fp = filp_open("/sys/kernel/security/apparmor", O_RDONLY | O_DIRECTORY, 0);
	if (IS_ERR(fp)) {
		seq_printf(m, "  - AppArmor not compiled in kernel\n");
		seq_printf(m, "  - AppArmor disabled in kernel cmdline\n");
		seq_printf(m, "  - securityfs not mounted\n");
		seq_printf(m, "\nTo enable:\n");
		seq_printf(m, "  - Check: cat /sys/kernel/security/lsm\n");
		seq_printf(m, "  - Enable: Add 'security=apparmor' to kernel cmdline\n");
		return 0;
	}
	filp_close(fp, NULL);

	seq_printf(m, "\n✓ AppArmor: ENABLED\n");

	/* Show current process confinement */
	show_current_profile(m);

	/* Show loaded profiles */
	show_loaded_profiles(m);

	/* Show features */
	show_apparmor_features(m);

	seq_printf(m, "\n=== USEFUL COMMANDS ===\n");
	seq_printf(m, "aa-status              # Detailed status\n");
	seq_printf(m, "aa-enforce <profile>   # Set profile to enforce mode\n");
	seq_printf(m, "aa-complain <profile>  # Set profile to complain mode\n");
	seq_printf(m, "aa-disable <profile>   # Disable a profile\n");
	seq_printf(m, "aa-logprof             # Review logs and update profiles\n");
	seq_printf(m, "\nProfile locations:\n");
	seq_printf(m, "  /etc/apparmor.d/     # Profile definitions\n");
	seq_printf(m, "  /var/log/audit/      # Audit logs (denials)\n");

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

static struct proc_dir_entry *proc_entry;  /* Handle for /proc file */

/*
 * apparmor_info_init - Module initialization
 *
 * Called when: Module is loaded with insmod/modprobe
 *
 * Creates /proc/apparmor_info to check AppArmor LSM status.
 * Useful for understanding if mandatory access control is enforced.
 *
 * USAGE:
 *   sudo insmod apparmor_info.ko
 *   cat /proc/apparmor_info          # Check AppArmor status
 *   sudo aa-status                   # Compare with aa-status output
 */
static int __init apparmor_info_init(void)
{
	proc_entry = proc_create(PROC_NAME, 0444, NULL, &proc_fops);
	if (!proc_entry)
		return -ENOMEM;

	pr_info("AppArmor Info: /proc/%s created\n", PROC_NAME);
	return 0;
}

/*
 * apparmor_info_exit - Module cleanup
 *
 * Called when: Module is removed with rmmod
 * Removes the /proc entry created during init
 */
static void __exit apparmor_info_exit(void)
{
	if (proc_entry)
		proc_remove(proc_entry);
}

module_init(apparmor_info_init);
module_exit(apparmor_info_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("AppArmor LSM information");
MODULE_VERSION("1.0");

