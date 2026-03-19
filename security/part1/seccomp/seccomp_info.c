/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

/*
 * seccomp_info.c - SECCOMP information from kernel perspective
 *
 * Shows which processes are using SECCOMP filtering and their mode.
 * SECCOMP is the 4th security layer after credentials, capabilities, and LSM.
 *
 * SECCOMP MODES:
 *   0 - DISABLED: No filtering
 *   1 - STRICT: Only read/write/exit/sigreturn allowed
 *   2 - FILTER: BPF filter deciding per syscall
 *
 * USAGE:
 *   sudo insmod seccomp_info.ko
 *   cat /proc/seccomp_info
 *   sudo rmmod seccomp_info
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/seccomp.h>

#define PROC_NAME "seccomp_info"

static int proc_show(struct seq_file *m, void *v)
{
    struct task_struct *task;
    int count = 0;

    seq_printf(m, "=== SECCOMP Process Information ===\n\n");
    seq_printf(m, "Current Process: %s (PID %d)\n", current->comm, current->pid);
    seq_printf(m, "SECCOMP mode: %d ", current->seccomp.mode);

    switch (current->seccomp.mode) {
        case SECCOMP_MODE_DISABLED:
            seq_printf(m, "(DISABLED)\n\n");
            break;
        case SECCOMP_MODE_STRICT:
            seq_printf(m, "(STRICT)\n\n");
            break;
        case SECCOMP_MODE_FILTER:
            seq_printf(m, "(FILTER)\n\n");
            break;
        default:
            seq_printf(m, "(UNKNOWN)\n\n");
    }

    /* Show processes using SECCOMP */
    seq_printf(m, "=== Processes Using SECCOMP ===\n");
    seq_printf(m, "%-20s %8s %8s %s\n", "Name", "PID", "Mode", "Filter");
    seq_printf(m, "%-20s %8s %8s %s\n", "----", "---", "----", "------");

    rcu_read_lock();
    for_each_process(task) {
        if (task->seccomp.mode != SECCOMP_MODE_DISABLED) {
            seq_printf(m, "%-20s %8d %8d ", task->comm, task->pid, task->seccomp.mode);

            if (task->seccomp.mode == SECCOMP_MODE_FILTER) {
                seq_printf(m, "YES\n");
            } else {
                seq_printf(m, "NO\n");
            }

            count++;

            if (count >= 20) {
                seq_printf(m, "... (showing first 20)\n");
                break;
            }
        }
    }
    rcu_read_unlock();

    if (count == 0) {
        seq_printf(m, "(No processes using SECCOMP)\n");
    } else {
        seq_printf(m, "\nTotal: %d processes\n", count);
    }

    seq_printf(m, "\n=== SECCOMP Modes ===\n");
    seq_printf(m, "0 - DISABLED: No syscall filtering\n");
    seq_printf(m, "1 - STRICT: Only read/write/exit allowed\n");
    seq_printf(m, "2 - FILTER: Custom BPF filter active\n");

    seq_printf(m, "\n=== Common SECCOMP Users ===\n");
    seq_printf(m, "systemd services: SystemCallFilter= option\n");
    seq_printf(m, "Docker containers: Default seccomp profile\n");
    seq_printf(m, "Chrome/Firefox: Sandbox processes\n");
    seq_printf(m, "SSH: Privilege separation\n");

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

static int __init seccomp_info_init(void)
{
    proc_entry = proc_create(PROC_NAME, 0444, NULL, &proc_fops);
    if (!proc_entry)
        return -ENOMEM;

    pr_info("SECCOMP Info: /proc/%s created\n", PROC_NAME);
    return 0;
}

static void __exit seccomp_info_exit(void)
{
    if (proc_entry)
        proc_remove(proc_entry);

    pr_info("SECCOMP Info: /proc/%s removed\n", PROC_NAME);
}

module_init(seccomp_info_init);
module_exit(seccomp_info_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("SECCOMP information");
MODULE_VERSION("1.0");
