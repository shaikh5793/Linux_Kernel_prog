/*
 * die_notifier.c - Minimal die (oops) notifier example
 *
 * Focused example that logs basic information when a kernel oops occurs.
 * This avoids overlap with the dedicated panic and reboot notifier examples
 * in the same folder.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/notifier.h>
#include <linux/kdebug.h>
#include <asm/ptrace.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("Minimal die (oops) notifier");
MODULE_VERSION("1.1");

static unsigned long oops_count = 0;

/*
 * What triggers this notifier?
 * - Kernel oops/exceptions (non-fatal by default), such as:
 *   - NULL dereference in kernel, invalid instruction, traps
 *   - Testing in VM: echo c > /proc/sysrq-trigger (may panic directly)
 *     or load an oops trigger module
 * Notes:
 * - Die notifiers run during oops handling; system may be unstable.
 *   Keep work minimal and avoid complex operations.
 */

/* Die notifier callback - called on kernel oops (not panic). */
static int die_event_handler(struct notifier_block *nb,
                             unsigned long val, void *data)
{
    struct die_args *args = (struct die_args *)data;

    oops_count++;

    pr_emerg("\n==== DIE NOTIFIER: kernel oops ====%s\n",
             val == DIE_OOPS ? " (DIE_OOPS)" : "");

    if (args) {
        pr_emerg("trapnr=%d signr=%d\n", (int)args->trapnr, args->signr);
        if (args->regs)
            pr_emerg("rip=0x%lx\n", instruction_pointer(args->regs));
    }

    pr_emerg("oops_count=%lu\n", oops_count);

    /* Continue with normal oops handling */
    return NOTIFY_DONE;
}

/* Notifier block structure */
static struct notifier_block die_notifier_block = {
    .notifier_call = die_event_handler,
    .priority = 0,
};

static int __init die_notifier_init(void)
{
    int ret;

    pr_info("die_notifier: loading (logs oops only; see panic/reboot examples separately)\n");

    ret = register_die_notifier(&die_notifier_block);
    if (ret) {
        pr_err("die_notifier: register failed: %d\n", ret);
        return ret;
    }

    pr_info("die_notifier: registered\n");
    return 0;
}

static void __exit die_notifier_exit(void)
{
    unregister_die_notifier(&die_notifier_block);
    pr_info("die_notifier: unloaded (oops_count=%lu)\n", oops_count);
}

module_init(die_notifier_init);
module_exit(die_notifier_exit);
