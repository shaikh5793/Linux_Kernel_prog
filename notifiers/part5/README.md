Linux Kernel Crash Notifiers (Concise)

Purpose
- Keep each example focused and non-overlapping:
  - panic_notifier.c: Panic path logging with a small preallocated dump buffer.
  - die_notifier.c: Minimal oops notifier; logs trap/signals and RIP.
  - reboot_notifier.c: Minimal shutdown/restart/halt/poweroff notifier.

Build
- make -C notifiers/part5

Modules
- panic_notifier.ko
  - Registers on panic path (atomic notifier).
  - Collects time, uname, current task, memory and CPU info.
  - No allocations or sleeps in handler.
  - Testing: Only in a VM. Example (crashes system): echo c > /proc/sysrq-trigger

- die_notifier.ko
  - Registers die notifier; logs oops (DIE_OOPS) with trapnr/signr/RIP.
  - Continues normal oops handling (NOTIFY_DONE).

- reboot_notifier.ko
  - Logs SYS_DOWN/SYS_RESTART/SYS_HALT/SYS_POWER_OFF.
  - For quick cleanup hooks during orderly shutdown.

Notes
- Do not test panic/oops on production systems. Use virtual machines or throwaway environments.
- For richer panic handling (kdump, pstore), see kernel docs; this repo shows minimal patterns.

Watchdog Integration
- The robustness module contains a watchdog example that focuses on fault tolerance and recovery behavior.
- Typical integration with notifiers (patterns only, hardware-specific code lives with watchdog):
  - Panic notifier: stop feeding the watchdog and optionally log pre-timeout data so the hardware reset can proceed deterministically.
  - Reboot notifier: disable/stop the watchdog during orderly shutdown to avoid immediate resets during userspace teardown.
  - Die (oops) notifier: record minimal info; do not perform long operations; watchdog policy may choose to escalate to panic.
- Recommendation: keep the watchdog demo under robustness and cross-reference it here; notifiers are the mechanism, watchdog is the use case.
