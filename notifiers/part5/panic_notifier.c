/*
 * panic_notifier.c - Minimal panic notifier with crash dump buffer
 * Collects basic system/task/memory/CPU info during panic using a
 * preallocated buffer (no allocations or sleeps in panic path).
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/notifier.h>
#include <linux/panic_notifier.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/rtc.h>
#include <linux/utsname.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <asm/stacktrace.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("Minimal panic notifier with crash dump");
MODULE_VERSION("2.1");

/*
 * Crash dump buffer - pre-allocated to avoid allocation during panic
 * Real implementations use reserved memory (like pstore, NVRAM, etc.)
 */
#define CRASH_DUMP_SIZE 4096
static char crash_dump_buffer[CRASH_DUMP_SIZE];
static size_t crash_dump_offset = 0;

/*
 * What triggers this notifier?
 * - Kernel panic, e.g. caused by:
 *   - Explicit `panic()` or BUG()/BUG_ON() leading to panic
 *   - sysrq-trigger: echo c > /proc/sysrq-trigger (VM-only for testing)
 *   - Oops with panic_on_oops=1
 * Notes:
 * - Runs extremely late in a dying system; keep the handler minimal,
 *   avoid allocations/locks, and do not sleep.
 */

/*
 * Helper to safely append to crash dump buffer
 * Safe to use during panic - no allocation, no locking
 */
static void crash_dump_append(const char *fmt, ...)
{
	va_list args;
	int len;

	if (crash_dump_offset >= CRASH_DUMP_SIZE - 1)
		return;

	va_start(args, fmt);
	len = vsnprintf(crash_dump_buffer + crash_dump_offset,
			CRASH_DUMP_SIZE - crash_dump_offset,
			fmt, args);
	va_end(args);

	if (len > 0)
		crash_dump_offset += len;
}

/*
 * Save basic system state information
 * Called during panic - must be fast and safe
 */
static void save_sysinfo(void)
{
	struct timespec64 ts;
	struct tm tm_result;

	ktime_get_real_ts64(&ts);
	time64_to_tm(ts.tv_sec, 0, &tm_result);

	crash_dump_append("=== KERNEL PANIC CRASH DUMP ===\n");
	crash_dump_append("Time: %04ld-%02d-%02d %02d:%02d:%02d UTC\n",
			  tm_result.tm_year + 1900, tm_result.tm_mon + 1,
			  tm_result.tm_mday, tm_result.tm_hour,
			  tm_result.tm_min, tm_result.tm_sec);
	crash_dump_append("Kernel: %s %s\n", utsname()->sysname, utsname()->release);
	crash_dump_append("Version: %s\n", utsname()->version);
	crash_dump_append("Machine: %s\n", utsname()->machine);
	crash_dump_append("Node: %s\n", utsname()->nodename);
}

/*
 * Save current task information
 * Shows what process was running when panic occurred
 */
static void save_task(void)
{
	struct task_struct *task = current;

	if (!task)
		return;

	crash_dump_append("\n=== Current Task ===\n");
	crash_dump_append("PID: %d\n", task->pid);
	crash_dump_append("Comm: %s\n", task->comm);
	crash_dump_append("State: %ld\n", task_state_index(task));
}

/*
 * Save minimal memory statistics
 * Shows memory pressure situation
 */
static void save_mem(void)
{
	struct sysinfo si;

	si_meminfo(&si);

	crash_dump_append("\n=== Memory Info ===\n");
	crash_dump_append("Total RAM: %lu KB\n", si.totalram << (PAGE_SHIFT - 10));
	crash_dump_append("Free RAM: %lu KB\n", si.freeram << (PAGE_SHIFT - 10));
	crash_dump_append("Buffers: %lu KB\n", si.bufferram << (PAGE_SHIFT - 10));
}

/*
 * Save CPU information
 * Shows which CPU encountered the panic
 */
static void save_cpu(void)
{
	crash_dump_append("\n=== CPU Info ===\n");
	crash_dump_append("CPU: %d\n", raw_smp_processor_id());
	crash_dump_append("Online CPUs: %d\n", num_online_cpus());
}

/*
 * Display the crash dump
 * In real systems, this would be written to:
 * - pstore (persistent storage)
 * - NVRAM
 * - Network (kdump over network)
 * - Reserved memory region
 * - Serial console
 */
static void show_dump(void)
{
	pr_emerg("\n");
	pr_emerg("==============================================\n");
	pr_emerg("    CRASH DUMP SAVED TO BUFFER\n");
	pr_emerg("==============================================\n");
	pr_emerg("%s\n", crash_dump_buffer);
	pr_emerg("==============================================\n");
	pr_emerg("Crash dump size: %zu bytes\n", crash_dump_offset);
	pr_emerg("==============================================\n");
}

/* Panic notifier callback - called when kernel panics */
static int panic_event_handler(struct notifier_block *nb,
			       unsigned long action, void *data)
{
	/*
	 * This function is called during kernel panic!
	 * Keep it SIMPLE and FAST - system is going down.
	 *
	 * SAFETY RULES DURING PANIC:
	 * 1. Use only pre-allocated memory
	 * 2. Don't take locks (they may be held)
	 * 3. Don't sleep or schedule
	 * 4. Keep execution time minimal
	 * 5. Assume other CPUs may be stopped
	 */

	pr_emerg("==============================================\n");
	pr_emerg("PANIC NOTIFIER: Kernel is panicking!\n");
	pr_emerg("==============================================\n");

	/* Reset crash dump buffer */
	crash_dump_offset = 0;
	memset(crash_dump_buffer, 0, CRASH_DUMP_SIZE);

	/* Collect crash information */
	pr_emerg("Collecting crash dump information...\n");

	save_sysinfo();
	save_task();
	save_mem();
	save_cpu();

	/* Display what we collected */
	show_dump();

	pr_emerg("Crash dump collection complete\n");
	pr_emerg("In production, this would be saved to persistent storage\n");

	/*
	 * Return value meanings:
	 * NOTIFY_DONE  - Continue calling other notifiers
	 * NOTIFY_OK    - Successfully handled
	 * NOTIFY_STOP  - Stop calling other notifiers
	 */
	return NOTIFY_DONE;
}

/* Notifier block structure */
static struct notifier_block panic_notifier_block = {
	.notifier_call = panic_event_handler,
	.priority = 0,  /* Higher priority = called earlier */
};

/*
 * Crash dump storage methods explained:
 *
 * 1. PSTORE (Persistent Storage):
 *    - Kernel subsystem for persistent storage
 *    - Survives reboots (RAM, NVRAM, flash)
 *    - Access via /sys/fs/pstore/
 *    - Used by many production systems
 *
 * 2. KDUMP:
 *    - Complete memory dump via kexec
 *    - Boots second kernel to dump first kernel's memory
 *    - Most comprehensive but requires setup
 *    - Writes to disk or network
 *
 * 3. Serial Console:
 *    - Simple, reliable fallback
 *    - Can be captured externally
 *    - Limited by serial bandwidth
 *
 * 4. NVRAM/EEPROM:
 *    - Small non-volatile storage
 *    - Fast access during panic
 *    - Limited size (KB range)
 *
 * 5. Network (netconsole):
 *    - UDP packets to remote server
 *    - May not work if network is broken
 *    - Good for cluster environments
 *
 * 6. Reserved Memory:
 *    - Memory region preserved across reboots
 *    - Read by bootloader or next boot
 *    - Requires bootloader cooperation
 *
 * Our example uses a simple buffer, but production systems
 * should use one of the above methods.
 */
static int __init panic_notifier_init(void)
{
    int ret;

	pr_info("panic_notifier: loading (collects minimal crash dump)\n");

	/* Register panic notifier */
	ret = atomic_notifier_chain_register(&panic_notifier_list,
					     &panic_notifier_block);
	if (ret) {
		pr_err("Failed to register panic notifier: %d\n", ret);
		return ret;
	}

	pr_info("panic_notifier: registered (buffer=%d bytes)\n", CRASH_DUMP_SIZE);

    return 0;
}

static void __exit panic_notifier_exit(void)
{
	/* Unregister panic notifier */
	atomic_notifier_chain_unregister(&panic_notifier_list,
					 &panic_notifier_block);

	pr_info("panic_notifier: unloaded\n");
}

module_init(panic_notifier_init);
module_exit(panic_notifier_exit);
