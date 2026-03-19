/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

/*
 * Power Management Notifier Example
 *
 * Demonstrates power management event notifications for:
 * - System suspend/resume
 * - Hibernation prepare/restore
 * - Device state management during power transitions
 *
 * This is a BLOCKING notifier chain (can sleep in callbacks).
 *
 * What triggers this notifier?
 * - Suspend/Resume: `systemctl suspend` or `echo mem > /sys/power/state`
 *   - PM_SUSPEND_PREPARE → PM_POST_SUSPEND
 * - Hibernation: `systemctl hibernate` or `echo disk > /sys/power/state`
 *   - PM_HIBERNATION_PREPARE → PM_POST_HIBERNATION
 * - Restore from image during boot:
 *   - PM_RESTORE_PREPARE → PM_POST_RESTORE
 *
 * EVENTS CAPTURED:
 * - PM_SUSPEND_PREPARE     : About to suspend to RAM
 * - PM_POST_SUSPEND        : Resumed from suspend
 * - PM_HIBERNATION_PREPARE : About to hibernate (suspend to disk)
 * - PM_POST_HIBERNATION    : Resumed from hibernation
 * - PM_RESTORE_PREPARE     : About to restore from hibernation image
 * - PM_POST_RESTORE        : Restored from hibernation
 *
 * USE CASES:
 * - Network drivers: disconnect before suspend, reconnect after resume
 * - Storage drivers: flush caches, park disk heads
 * - Graphics drivers: save GPU state, restore display
 * - Audio drivers: mute audio, restore volume
 * - Power-aware applications: pause operations, resume work
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/suspend.h>
#include <linux/notifier.h>

static unsigned long suspend_count = 0;
static unsigned long resume_count = 0;
static unsigned long hibernate_count = 0;

/*
 * Power Management Event Handler
 *
 * This callback is invoked for all PM events.
 * It runs in process context and CAN SLEEP (blocking notifier).
 *
 * Return values:
 * - NOTIFY_OK: Event handled successfully
 * - NOTIFY_DONE: Event not handled
 * - NOTIFY_BAD: Error occurred (may prevent operation)
 */
static int pm_event_handler(struct notifier_block *nb,
                            unsigned long event, void *data)
{
    switch (event) {
    case PM_SUSPEND_PREPARE:
        suspend_count++;
        pr_info("PM NOTIFIER: System preparing to SUSPEND (count: %lu)\n",
                suspend_count);
        pr_info("  Action: Save device state, flush buffers, disconnect network\n");

        /*
         * This is where you would:
         * - Save device registers/state
         * - Flush pending I/O operations
         * - Stop DMA transfers
         * - Park disk heads (storage devices)
         * - Disconnect network connections
         * - Free non-critical memory
         */

        // Example: simulate device state save
        pr_info("  [Simulating] Saving device state...\n");

        break;

    case PM_POST_SUSPEND:
        resume_count++;
        pr_info("PM NOTIFIER: System RESUMED from suspend (count: %lu)\n",
                resume_count);
        pr_info("  Action: Restore device state, reinitialize hardware\n");

        /*
         * This is where you would:
         * - Restore device registers/state
         * - Reinitialize hardware
         * - Restart DMA transfers
         * - Reconnect network
         * - Restore display modes
         * - Resume I/O operations
         */

        // Example: simulate device state restore
        pr_info("  [Simulating] Restoring device state...\n");

        break;

    case PM_HIBERNATION_PREPARE:
        hibernate_count++;
        pr_info("PM NOTIFIER: System preparing to HIBERNATE (count: %lu)\n",
                hibernate_count);
        pr_info("  Action: Deep save - system will power off completely\n");

        /*
         * Hibernation is different from suspend:
         * - System state saved to disk
         * - System powers off completely
         * - On boot, state is restored from disk
         * - Need to handle complete power loss
         */

        pr_info("  [Simulating] Preparing for hibernation...\n");

        break;

    case PM_POST_HIBERNATION:
        pr_info("PM NOTIFIER: System RESUMED from hibernation\n");
        pr_info("  Action: Restore after complete power cycle\n");

        /*
         * After hibernation resume:
         * - System was completely powered off
         * - Hardware may need full reinitialization
         * - Some devices may be in different states
         */

        pr_info("  [Simulating] Restoring from hibernation...\n");

        break;

    case PM_RESTORE_PREPARE:
        pr_info("PM NOTIFIER: Preparing to RESTORE from hibernation image\n");
        pr_info("  Action: Prepare for state restoration\n");

        /*
         * Called before restoring hibernation image:
         * - About to load system state from disk
         * - Prepare devices for state restoration
         */

        break;

    case PM_POST_RESTORE:
        pr_info("PM NOTIFIER: Completed RESTORE from hibernation image\n");
        pr_info("  Action: Finalize restoration\n");

        /*
         * Called after hibernation image is restored:
         * - System state has been loaded
         * - Finalize device restoration
         */

        break;

    default:
        pr_info("PM NOTIFIER: Unknown PM event: %lu\n", event);
        return NOTIFY_DONE;
    }

    return NOTIFY_OK;
}

/*
 * Notifier block structure
 *
 * Priority: 0 (default)
 * Higher priority values execute first
 * Use higher priority for critical operations
 */
static struct notifier_block pm_nb = {
    .notifier_call = pm_event_handler,
    .priority = 0,  // Normal priority
};

static int __init pm_notifier_init(void)
{
    int ret;

    pr_info("=== PM Notifier Module Initializing ===\n");

    /* Register for power management notifications */
    ret = register_pm_notifier(&pm_nb);
    if (ret) {
        pr_err("Failed to register PM notifier: %d\n", ret);
        return ret;
    }

    pr_info("PM notifier registered successfully\n");
    pr_info("\n");
    pr_info("Testing instructions:\n");
    pr_info("  On laptops/desktops:\n");
    pr_info("    sudo systemctl suspend          # Suspend to RAM\n");
    pr_info("    sudo systemctl hibernate        # Suspend to disk\n");
    pr_info("  \n");
    pr_info("  Alternative (requires root):\n");
    pr_info("    echo mem > /sys/power/state     # Suspend\n");
    pr_info("    echo disk > /sys/power/state    # Hibernate\n");
    pr_info("  \n");
    pr_info("  After resume, check logs:\n");
    pr_info("    dmesg | grep 'PM NOTIFIER'\n");
    pr_info("\n");
    pr_info("Statistics will be printed on module unload\n");

    return 0;
}

static void __exit pm_notifier_exit(void)
{
    pr_info("=== PM Notifier Module Exiting ===\n");

    /* Unregister PM notifier */
    unregister_pm_notifier(&pm_nb);

    pr_info("PM notifier unregistered\n");
    pr_info("\n");
    pr_info("Statistics:\n");
    pr_info("  Suspend events:    %lu\n", suspend_count);
    pr_info("  Resume events:     %lu\n", resume_count);
    pr_info("  Hibernate events:  %lu\n", hibernate_count);
    pr_info("\n");

    if (suspend_count == 0 && resume_count == 0 && hibernate_count == 0) {
        pr_info("NOTE: No PM events detected.\n");
        pr_info("      This is normal if system wasn't suspended/hibernated.\n");
        pr_info("      Try: sudo systemctl suspend\n");
    } else {
        pr_info("Successfully captured %lu total PM events\n",
                suspend_count + resume_count + hibernate_count);
    }
}

module_init(pm_notifier_init);
module_exit(pm_notifier_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("Power Management Notifier - Suspend/Resume/Hibernate events");
MODULE_VERSION("1.0");
