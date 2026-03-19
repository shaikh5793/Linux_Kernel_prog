/*
 * oom_notifier.c - Demonstrates OOM (Out-Of-Memory) notifier
 *
 * Shows how to register an OOM notifier to be called when the system
 * is running out of memory, BEFORE the OOM killer selects a victim.
 *
 * Use cases:
 *   - Free up caches before OOM killer runs
 *   - Log memory statistics for analysis
 *   - Trigger emergency memory reclaim
 *   - Notify monitoring systems
 *   - Custom memory management decisions
 *
 * Copyright (c) 2024 TECH VEDA (www.techveda.org)
 * Author: Raghu Bharadwaj
 * License: Dual MIT/GPL
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/oom.h>
#include <linux/notifier.h>
#include <linux/mm.h>
#include <linux/sysinfo.h>

/* Statistics */
static unsigned long oom_event_count = 0;
static unsigned long last_free_pages = 0;

/*
 * OOM notifier callback
 *
 * Called when system is running out of memory, BEFORE OOM killer
 * selects a victim process to terminate.
 *
 * Context: May be called in atomic context
 * Return: NOTIFY_OK to continue, NOTIFY_STOP to stop chain
 */
static int oom_event_handler(struct notifier_block *nb,
			      unsigned long val, void *data)
{
	struct sysinfo si;
	unsigned long free_pages;

	/* Increment OOM event counter */
	oom_event_count++;

	pr_warn("\n========================================\n");
	pr_warn("OOM NOTIFIER: Memory exhaustion detected!\n");
	pr_warn("========================================\n");
	pr_warn("OOM event count: %lu\n", oom_event_count);

	/* Get system memory info */
	si_meminfo(&si);
	free_pages = si.freeram;

	pr_warn("Memory Status:\n");
	pr_warn("  Total RAM:     %lu pages (%lu MB)\n",
		si.totalram, (si.totalram * PAGE_SIZE) >> 20);
	pr_warn("  Free RAM:      %lu pages (%lu MB)\n",
		si.freeram, (si.freeram * PAGE_SIZE) >> 20);
	pr_warn("  Shared RAM:    %lu pages\n", si.sharedram);
	pr_warn("  Buffer RAM:    %lu pages\n", si.bufferram);
	pr_warn("  Total Swap:    %lu pages\n", si.totalswap);
	pr_warn("  Free Swap:     %lu pages\n", si.freeswap);

	/* Show memory pressure trend */
	if (last_free_pages > 0) {
		long delta = (long)free_pages - (long)last_free_pages;
		pr_warn("  Memory trend:  %s %ld pages since last OOM\n",
			delta < 0 ? "Lost" : "Gained",
			delta < 0 ? -delta : delta);
	}
	last_free_pages = free_pages;

	/*
	 * Real-world actions you might take here:
	 *
	 * 1. Free up caches:
	 *    - Drop slab caches
	 *    - Flush buffer cache
	 *    - Release unused memory
	 *
	 * 2. Logging/Monitoring:
	 *    - Record detailed memory stats
	 *    - Dump process memory usage
	 *    - Alert monitoring systems
	 *
	 * 3. Emergency actions:
	 *    - Trigger memory compaction
	 *    - Force writeback of dirty pages
	 *    - Adjust memory limits
	 *
	 * 4. Prevention:
	 *    - Pause low-priority allocations
	 *    - Throttle memory-intensive tasks
	 */

	pr_warn("Action: Logging memory state for analysis\n");
	pr_warn("OOM killer will now select victim process...\n");
	pr_warn("========================================\n");

	/*
	 * Return values:
	 * NOTIFY_OK   - Handled successfully, continue chain
	 * NOTIFY_STOP - Stop calling other notifiers
	 * NOTIFY_DONE - Didn't handle, continue chain
	 */
	return NOTIFY_OK;
}

/* Notifier block structure */
static struct notifier_block oom_notifier_block = {
	.notifier_call = oom_event_handler,
	.priority = 0,  /* Standard priority */
};

/*
 * What triggers this notifier?
 * - Global out-of-memory (OOM) condition after reclaim fails,
 *   just before the kernel selects and kills a victim task.
 * - Typical triggers: extreme memory pressure, runaway allocations,
 *   or constrained cgroups leading to OOM.
 * Safety: The system is critically low on memory — avoid allocations.
 */

/*
 * OOM Notifier Concepts
 * =====================
 *
 * Chain: oom_notify_list (blocking notifier)
 *
 * When triggered:
 *   - System runs critically low on memory
 *   - After all reclaim attempts have failed
 *   - BEFORE OOM killer selects victim
 *   - Last chance to free memory
 *
 * Event flow:
 *   1. Memory allocation fails
 *   2. Kernel tries to reclaim memory
 *   3. Reclaim fails
 *   4. OOM notifiers called ← We are here!
 *   5. OOM killer selects victim process
 *   6. Victim process is killed
 *   7. Memory becomes available
 *
 * Notifier type: BLOCKING
 *   - Can sleep
 *   - Can do complex operations
 *   - Called in process context
 *
 * Important notes:
 *   - System is in CRITICAL state
 *   - Keep handler FAST
 *   - Don't allocate memory!
 *   - Don't make things worse
 */

/*
 * Testing: Use userspace programs under part1/tools to induce memory pressure
 *          in a controlled environment (preferably a VM).
 */

/* Show statistics */
static void show_stats(void)
{
	pr_info("OOM Notifier Statistics:\n");
	pr_info("  OOM events:  %lu\n", oom_event_count);
	pr_info("  Last free:   %lu pages\n", last_free_pages);
	pr_info("  Priority:    %d\n", oom_notifier_block.priority);
}

static int __init oom_notifier_init(void)
{
	int ret;
	struct sysinfo si;

	pr_info("OOM notifier demonstration module loading...\n");

	/* Register OOM notifier */
	ret = register_oom_notifier(&oom_notifier_block);
	if (ret) {
		pr_err("Failed to register OOM notifier: %d\n", ret);
		return ret;
	}

	pr_info("OOM notifier registered successfully\n");

	/* Show current memory status */
	si_meminfo(&si);
	pr_info("Current memory status:\n");
	pr_info("  Total RAM: %lu MB\n", (si.totalram * PAGE_SIZE) >> 20);
	pr_info("  Free RAM:  %lu MB\n", (si.freeram * PAGE_SIZE) >> 20);
	pr_info("  Threshold: Will trigger when free memory is critically low\n");

	/* Store initial state */
	last_free_pages = si.freeram;

	show_stats();

	pr_info("\n");
	pr_info("OOM notifier is now monitoring memory exhaustion events\n");
	pr_info("Testing: build and run part1/tools triggers (VM recommended)\n");

	return 0;
}

static void __exit oom_notifier_exit(void)
{
	/* Unregister OOM notifier */
	unregister_oom_notifier(&oom_notifier_block);

	/* Show final statistics */
	show_stats();

	pr_info("OOM notifier unregistered and module unloaded\n");
}

module_init(oom_notifier_init);
module_exit(oom_notifier_exit);



MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("Demonstrates OOM (Out-Of-Memory) notifier");
