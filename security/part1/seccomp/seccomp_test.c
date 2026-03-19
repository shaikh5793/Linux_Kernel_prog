/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

/*
 * seccomp_test.c - SECCOMP System Call Filtering Demonstration
 *
 * SECCOMP (Secure Computing Mode) is the 4th security layer that filters
 * which system calls a process can make. It can block syscalls even for
 * root with all capabilities and no AppArmor restrictions.
 *
 * This example uses libseccomp - a high-level API that generates BPF filters
 * automatically. You specify rules using seccomp_rule_add(), and libseccomp
 * compiles them into BPF bytecode behind the scenes.
 *
 *
 * SECCOMP MODES:
 * - SECCOMP_MODE_DISABLED: No filtering (default)
 * - SECCOMP_MODE_STRICT: Only read, write, exit, sigreturn allowed
 * - SECCOMP_MODE_FILTER: Custom BPF filter (what we use here)
 *
 * FILTER ACTIONS:
 * - SCMP_ACT_KILL: Kill process immediately
 * - SCMP_ACT_ERRNO(err): Return error code to syscall
 * - SCMP_ACT_ALLOW: Allow syscall to proceed
 * - SCMP_ACT_TRAP: Send SIGSYS signal
 *
 * USAGE:
 *   gcc -o seccomp_test seccomp_test.c -lseccomp
 *   ./seccomp_test
 *   ./seccomp_test --test-kill  (tests kill action)
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/prctl.h>
#include <linux/seccomp.h>
#include <sys/syscall.h>
#include <seccomp.h>  /* libseccomp - high-level SECCOMP API */

/*
 * print_status - Display current process status and SECCOMP mode
 *
 * Uses prctl(PR_GET_SECCOMP) to query the current SECCOMP mode.
 * This demonstrates how to check if SECCOMP filtering is active.
 *
 * SECCOMP modes:
 * - DISABLED (0): No filtering active
 * - STRICT (1): Only read, write, exit, sigreturn allowed
 * - FILTER (2): Custom BPF filter active (what we demonstrate)
 */
void print_status(void)
{
    int mode;

    printf("\n=== Status ===\n");
    printf("UID: %u\n", getuid());
    printf("PID: %d\n", getpid());

    /*
     * Query current SECCOMP mode using prctl()
     * This returns the mode, or -1 on error
     */
    mode = prctl(PR_GET_SECCOMP);
    printf("SECCOMP: ");
    switch(mode) {
        case SECCOMP_MODE_DISABLED:
            printf("DISABLED\n");
            break;
        case SECCOMP_MODE_STRICT:
            printf("STRICT\n");
            break;
        case SECCOMP_MODE_FILTER:
            printf("FILTER ACTIVE\n");
            break;
        default:
            printf("UNKNOWN\n");
    }
    printf("\n");
}

/*
 * test_allowed_syscall - Test a syscall that should remain allowed
 *
 * Tests gettimeofday() which is a harmless syscall that should
 * be allowed even with SECCOMP filter active.
 *
 * Uses syscall() to make direct system call (bypasses libc wrapper).
 */
void test_allowed_syscall(void)
{
    printf("gettimeofday(): ");
    if (syscall(SYS_gettimeofday, NULL, NULL) >= 0 || errno != ENOSYS) {
        printf("ALLOWED\n");
    } else {
        printf("BLOCKED\n");
    }
}

/*
 * test_blocked_syscall - Test a syscall blocked with EPERM
 *
 * Tests sethostname() which is blocked by our SECCOMP filter.
 * The filter is configured to return EPERM for this syscall.
 *
 * This demonstrates SCMP_ACT_ERRNO action - the syscall fails
 * with a specific error code, but the process continues running.
 */
void test_blocked_syscall(void)
{
    printf("sethostname(): ");
    if (syscall(SYS_sethostname, "test", 4) == 0) {
        printf("ALLOWED\n");
    } else {
        printf("BLOCKED (%s)\n", strerror(errno));
    }
}

/*
 * test_dangerous_syscall - Test a syscall blocked with KILL
 *
 * Tests reboot() which is blocked by our SECCOMP filter with
 * SCMP_ACT_KILL action.
 *
 * WARNING: Calling this will immediately kill the process!
 * This demonstrates the most severe SECCOMP action - instant
 * process termination with no chance to recover.
 */
void test_dangerous_syscall(void)
{
    printf("reboot(): ");
    if (syscall(SYS_reboot, 0, 0, 0, NULL) == 0) {
        printf("ALLOWED\n");
    } else {
        printf("BLOCKED (%s)\n", strerror(errno));
    }
}

/*
 * apply_seccomp_filter - Configure and load SECCOMP filter
 *
 * Creates a SECCOMP filter using libseccomp high-level API.
 * This demonstrates a whitelist-by-default approach where most
 * syscalls are allowed, but specific dangerous ones are blocked.
 *
 * HOW IT WORKS:
 * - You call seccomp_rule_add() with high-level rules
 * - libseccomp automatically generates BPF bytecode from your rules
 * - seccomp_load() installs the BPF program into the kernel
 * - The kernel runs this BPF code on every syscall to decide: allow/block
 *
 * Compare with seccomp_modes.c which shows the raw BPF instructions
 * like BPF_STMT(), BPF_JUMP(). Here, libseccomp generates those for you!
 *
 * FILTER DESIGN:
 * - Default action: SCMP_ACT_ALLOW (whitelist approach)
 * - Block specific dangerous syscalls with different actions
 *
 * BLOCKED SYSCALLS:
 * 1. reboot() -> SCMP_ACT_KILL
 *    - Most dangerous: instant process termination
 *    - No error return, no cleanup possible
 *
 * 2. sethostname() -> SCMP_ACT_ERRNO(EPERM)
 *    - Returns error, process continues
 *    - Application can handle the error gracefully
 *
 * 3. ptrace() -> SCMP_ACT_ERRNO(EPERM)
 *    - Prevents debugging/tracing this process
 *    - Common anti-debugging technique
 *
 * IMPORTANT: Once seccomp_load() succeeds, the filter is active
 * and CANNOT be removed or modified! It persists across fork()
 * and is inherited by child processes.
 */
int apply_seccomp_filter(void)
{
    scmp_filter_ctx ctx;

    printf("=== Applying SECCOMP Filter ===\n");

    /*
     * Initialize filter context with default action
     * SCMP_ACT_ALLOW: Allow all syscalls by default
     * Alternative: SCMP_ACT_KILL would block everything (strict whitelist)
     */
    ctx = seccomp_init(SCMP_ACT_ALLOW);
    if (ctx == NULL) {
        fprintf(stderr, "Failed to init seccomp\n");
        return -1;
    }

    printf("Blocking:\n");

    /*
     * Add filter rule for reboot() syscall
     * SCMP_ACT_KILL: Immediately terminate process (no error return)
     * Argument 0: No argument-based filtering (block all reboot() calls)
     */
    printf("  reboot() -> KILL\n");
    seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(reboot), 0);

    /*
     * Add filter rule for sethostname() syscall
     * SCMP_ACT_ERRNO(EPERM): Return EPERM error, process continues
     * More graceful than KILL - allows error handling
     */
    printf("  sethostname() -> EPERM\n");
    seccomp_rule_add(ctx, SCMP_ACT_ERRNO(EPERM), SCMP_SYS(sethostname), 0);

    /*
     * Add filter rule for ptrace() syscall
     * Blocks debugging/tracing of this process
     * Common security hardening technique
     */
    printf("  ptrace() -> EPERM\n");
    seccomp_rule_add(ctx, SCMP_ACT_ERRNO(EPERM), SCMP_SYS(ptrace), 0);

    /*
     * Load the filter into the kernel
     * After this point, filter is ACTIVE and IRREVERSIBLE!
     * The BPF program is attached to the process and cannot be removed
     */
    if (seccomp_load(ctx) < 0) {
        fprintf(stderr, "Failed to load filter\n");
        seccomp_release(ctx);
        return -1;
    }

    printf("Filter loaded\n\n");

    /* Release the context (filter is already loaded in kernel) */
    seccomp_release(ctx);
    return 0;
}

int main(int argc, char *argv[])
{
    printf("\n=== SECCOMP Test ===\n");

    /*
     * PHASE 1: Before SECCOMP filter
     * Show baseline - what works without filtering
     */
    print_status();

    printf("=== Before SECCOMP ===\n");
    test_allowed_syscall();    /* gettimeofday() - should work */
    test_blocked_syscall();    /* sethostname() - may fail due to permissions */
    test_dangerous_syscall();  /* reboot() - fails due to permissions, not SECCOMP */
    printf("\n");

    /*
     * PHASE 2: Apply SECCOMP filter
     * This is the point of no return!
     * After this, the BPF filter is loaded into kernel and cannot be removed
     */
    if (apply_seccomp_filter() < 0) {
        return 1;
    }

    /*
     * PHASE 3: After SECCOMP filter
     * Status should now show SECCOMP_MODE_FILTER
     */
    print_status();

    printf("=== After SECCOMP ===\n");
    test_allowed_syscall();    /* gettimeofday() - still allowed */
    test_blocked_syscall();    /* sethostname() - now blocked by SECCOMP with EPERM */

    /*
     * PHASE 4: Optional KILL test
     * WARNING: This will terminate the process!
     *
     * We skip this by default because SCMP_ACT_KILL immediately
     * terminates the process with no chance to print anything.
     * User must explicitly request it with --test-kill
     */
    if (argc > 1 && strcmp(argv[1], "--test-kill") == 0) {
        printf("\nAttempting reboot() (will kill process):\n");
        test_dangerous_syscall();  /* Process dies here! */
        printf("(Should not reach here)\n");
    } else {
        printf("\nSkipping reboot() test (would kill process)\n");
        printf("Run with --test-kill to test it\n");
    }

    return 0;
}
