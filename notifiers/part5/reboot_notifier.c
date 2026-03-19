/*
 * reboot_notifier.c - Minimal reboot/shutdown notifier
 * Logs system shutdown/restart/halt/poweroff events for quick hooks.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/notifier.h>
#include <linux/reboot.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("Minimal reboot/shutdown notifier");
MODULE_VERSION("1.1");

/* Reboot notifier callback */
static int reboot_event_handler(struct notifier_block *nb,
				unsigned long action, void *data)
{
	pr_info("\n==============================================\n");
	pr_info("REBOOT NOTIFIER: System event detected!\n");
	pr_info("==============================================\n");

	/* Identify the event type */
	switch (action) {
	case SYS_DOWN:  /* Note: SYS_RESTART == SYS_DOWN in modern kernels */
		pr_info("Event: SYS_DOWN/SYS_RESTART (system going down/restarting)\n");
		pr_info("Performing shutdown/reboot cleanup...\n");
		break;

	case SYS_HALT:
		pr_info("Event: SYS_HALT (system halting)\n");
		pr_info("Performing halt cleanup...\n");
		break;

	case SYS_POWER_OFF:
		pr_info("Event: SYS_POWER_OFF (system powering off)\n");
		pr_info("Performing power-off cleanup...\n");
		break;

	default:
		pr_info("Event: UNKNOWN (%lu)\n", action);
		break;
	}

	/*
	 * Perform cleanup here:
	 * - Flush buffers
	 * - Save state
	 * - Notify hardware
	 * - Release resources
	 */

	pr_info("Cleanup completed\n");
	pr_info("==============================================\n");

	return NOTIFY_DONE;
}

/* Notifier block structure */
static struct notifier_block reboot_notifier_block = {
	.notifier_call = reboot_event_handler,
	.priority = 0,
};

/*
 * What triggers this notifier?
 * - System shutdown/restart paths:
 *   - SYS_DOWN/SYS_RESTART: `reboot`, `shutdown`, `systemctl reboot`
 *   - SYS_POWER_OFF: `poweroff`, `systemctl poweroff`, SysRq 'o'
 *   - SYS_HALT: `halt`, `systemctl halt`
 * Notes:
 * - Not called for emergency_restart(); that bypasses notifiers.
 */

static int __init reboot_notifier_init(void)
{
	int ret;

	pr_info("reboot_notifier: loading\n");

	/* Register reboot notifier */
	ret = register_reboot_notifier(&reboot_notifier_block);
	if (ret) {
		pr_err("reboot_notifier: register failed: %d\n", ret);
		return ret;
	}

	pr_info("reboot_notifier: registered (logs SYS_DOWN/RESTART/HALT/POWER_OFF)\n");
	return 0;
}

static void __exit reboot_notifier_exit(void)
{
	/* Unregister reboot notifier */
	unregister_reboot_notifier(&reboot_notifier_block);

	pr_info("reboot_notifier: unloaded\n");
}

module_init(reboot_notifier_init);
module_exit(reboot_notifier_exit);
