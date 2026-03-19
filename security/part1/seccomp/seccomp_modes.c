/*
 * seccomp_modes.c - Demonstrate all SECCOMP modes
 *
 * SECCOMP has 3 modes:
 *   0. DISABLED - No syscall filtering (default)
 *   1. STRICT   - Only read/write/exit/sigreturn allowed (all else → KILL)
 *   2. FILTER   - Custom BPF filter per syscall (flexible actions)
 *
 * This program demonstrates each mode and what happens.
 *
 * USAGE:
 *   gcc -o seccomp_modes seccomp_modes.c
 *   ./seccomp_modes
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <sys/prctl.h>
#include <linux/seccomp.h>
#include <linux/filter.h>
#include <linux/audit.h>
#include <sys/syscall.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

/* Check current SECCOMP mode */
void print_mode(void)
{
    int mode = prctl(PR_GET_SECCOMP);

    printf("Current SECCOMP mode: %d ", mode);
    switch(mode) {
        case SECCOMP_MODE_DISABLED:
            printf("(DISABLED - no filtering)\n");
            break;
        case SECCOMP_MODE_STRICT:
            printf("(STRICT - only read/write/exit/sigreturn)\n");
            break;
        case SECCOMP_MODE_FILTER:
            printf("(FILTER - custom BPF filter)\n");
            break;
        default:
            printf("(UNKNOWN)\n");
    }
}

/* Try various syscalls to see what's allowed */
void test_syscalls(const char *phase)
{
    printf("\n--- Testing syscalls in %s ---\n", phase);

    /* Test 1: getpid() - safe syscall */
    printf("getpid(): ");
    fflush(stdout);
    pid_t pid = getpid();
    printf("%d (SUCCESS)\n", pid);

    /* Test 2: getuid() - safe syscall */
    printf("getuid(): ");
    fflush(stdout);
    uid_t uid = getuid();
    printf("%d (SUCCESS)\n", uid);

    /* Test 3: gettimeofday() - time syscall */
    printf("gettimeofday(): ");
    fflush(stdout);
    if (syscall(SYS_gettimeofday, NULL, NULL) >= 0) {
        printf("SUCCESS\n");
    } else {
        printf("FAILED (%s)\n", strerror(errno));
    }

    /* Test 4: sethostname() - privileged syscall */
    printf("sethostname(): ");
    fflush(stdout);
    if (syscall(SYS_sethostname, "test", 4) == 0) {
        printf("SUCCESS\n");
    } else {
        printf("FAILED (%s)\n", strerror(errno));
    }
}

/*
 * MODE 0: DISABLED (default)
 * All syscalls are allowed (subject to normal permissions)
 */
void demo_mode_disabled(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║  MODE 0: SECCOMP_MODE_DISABLED                       ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n");
    printf("\nDescription:\n");
    printf("  - Default mode for all processes\n");
    printf("  - No syscall filtering by SECCOMP\n");
    printf("  - Normal permission checks still apply:\n");
    printf("    • UID/GID checks\n");
    printf("    • Capability checks\n");
    printf("    • LSM (AppArmor/SELinux) checks\n");
    printf("  - All syscalls allowed (if permissions permit)\n\n");

    print_mode();
    test_syscalls("DISABLED mode");
}

/*
 * MODE 1: STRICT
 * Only 4 syscalls allowed: read, write, exit, sigreturn
 * All other syscalls → process killed with SIGKILL
 */
void demo_mode_strict(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║  MODE 1: SECCOMP_MODE_STRICT                         ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n");
    printf("\nDescription:\n");
    printf("  - Most restrictive mode\n");
    printf("  - Only 4 syscalls allowed:\n");
    printf("    • read()      - read from file descriptor\n");
    printf("    • write()     - write to file descriptor\n");
    printf("    • exit()      - terminate process\n");
    printf("    • sigreturn() - return from signal handler\n");
    printf("  - ANY other syscall → immediate SIGKILL\n");
    printf("  - Cannot be undone once enabled\n");
    printf("  - Useful for:\n");
    printf("    • Pure computation tasks\n");
    printf("    • Processes that only read/write\n");
    printf("    • Maximum isolation\n\n");

    printf("WARNING: About to enable STRICT mode\n");
    printf("Process will be KILLED on first disallowed syscall!\n\n");

    /* Enable STRICT mode */
    if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_STRICT) == 0) {
        const char *msg;

        msg = "SECCOMP STRICT mode enabled\n\n";
        write(STDOUT_FILENO, msg, strlen(msg));

        /* Can only use write() now - printf won't work! */
        msg = "Testing allowed syscall (write): SUCCESS\n";
        write(STDOUT_FILENO, msg, strlen(msg));

        msg = "\nAttempting getpid() - will KILL process...\n";
        write(STDOUT_FILENO, msg, strlen(msg));

        /* This will KILL the process */
        getpid();

        msg = "ERROR: Should not reach here!\n";
        write(STDOUT_FILENO, msg, strlen(msg));
    }
}

/*
 * MODE 2: FILTER
 * Custom BPF program decides per syscall
 * Actions: ALLOW, ERRNO, TRACE, TRAP, KILL
 */
void demo_mode_filter(void)
{
    struct sock_filter filter[] = {
        /* Load architecture */
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
                 (offsetof(struct seccomp_data, arch))),
        /* Check if x86_64 */
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, AUDIT_ARCH_X86_64, 0, 7),

        /* Load syscall number */
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
                 (offsetof(struct seccomp_data, nr))),

        /* Allow read, write, exit */
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_read, 4, 0),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_write, 3, 0),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_exit, 2, 0),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_exit_group, 1, 0),

        /* Deny everything else with EPERM */
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        /* Allow matched syscalls */
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
    };

    struct sock_fprog prog = {
        .len = sizeof(filter) / sizeof(filter[0]),
        .filter = filter,
    };

    printf("\n");
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║  MODE 2: SECCOMP_MODE_FILTER                         ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n");
    printf("\nDescription:\n");
    printf("  - Most flexible mode\n");
    printf("  - Uses BPF (Berkeley Packet Filter) program\n");
    printf("  - Decides action per syscall\n");
    printf("  - Available actions:\n");
    printf("    • ALLOW  - Permit the syscall\n");
    printf("    • ERRNO  - Deny with error code (e.g., EPERM)\n");
    printf("    • TRAP   - Send SIGSYS signal\n");
    printf("    • TRACE  - Notify tracer (ptrace)\n");
    printf("    • KILL   - Kill process immediately\n");
    printf("  - Used by:\n");
    printf("    • Docker (blocks ~40 dangerous syscalls)\n");
    printf("    • Chrome (sandboxes renderers)\n");
    printf("    • systemd (SystemCallFilter=)\n\n");

    printf("Example: Allow only read/write/exit, deny others with EPERM\n\n");

    /* Enable NO_NEW_PRIVS (required for FILTER mode) */
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
        perror("prctl(NO_NEW_PRIVS)");
        return;
    }

    /* Install filter */
    if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog)) {
        perror("prctl(SECCOMP)");
        return;
    }

    printf("SECCOMP FILTER mode enabled\n\n");
    print_mode();

    /* Test what's allowed */
    printf("\nTesting syscalls:\n");

    printf("write(): ");
    if (write(STDOUT_FILENO, "", 0) == 0) {
        printf("ALLOWED\n");
    } else {
        printf("BLOCKED\n");
    }

    printf("getpid(): ");
    if (getpid() > 0) {
        printf("ALLOWED\n");
    } else {
        printf("BLOCKED (%s)\n", strerror(errno));
    }

    printf("gettimeofday(): ");
    if (syscall(SYS_gettimeofday, NULL, NULL) == 0) {
        printf("ALLOWED\n");
    } else {
        printf("BLOCKED (%s)\n", strerror(errno));
    }
}

void print_menu(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║          SECCOMP Modes Demonstration                 ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n");
    printf("\nSECCOMP has 3 modes for syscall filtering:\n\n");
    printf("  0. DISABLED - No filtering (default)\n");
    printf("  1. STRICT   - Only read/write/exit allowed\n");
    printf("  2. FILTER   - Custom BPF filter\n\n");
    printf("Choose mode to demonstrate:\n");
    printf("  1) MODE 0: DISABLED\n");
    printf("  2) MODE 1: STRICT (will kill process!)\n");
    printf("  3) MODE 2: FILTER\n");
    printf("  4) Exit\n\n");
    printf("Choice: ");
}

int main(void)
{
    int choice;

    while (1) {
        print_menu();
        if (scanf("%d", &choice) != 1) {
            printf("Invalid input\n");
            while(getchar() != '\n');
            continue;
        }

        switch(choice) {
            case 1:
                demo_mode_disabled();
                printf("\nPress Enter to continue...");
                while(getchar() != '\n');
                getchar();
                break;

            case 2:
                printf("\nAre you sure? Process will be killed! (y/n): ");
                while(getchar() != '\n');
                if (getchar() == 'y') {
                    demo_mode_strict();
                    /* Won't reach here */
                }
                break;

            case 3:
                demo_mode_filter();
                printf("\nPress Enter to continue...");
                while(getchar() != '\n');
                getchar();
                break;

            case 4:
                printf("Exiting...\n");
                return 0;

            default:
                printf("Invalid choice\n");
        }
    }

    return 0;
}
