/*
 * seccomp_basic.c - Basic SECCOMP strict mode
 *
 * Demonstrates SECCOMP_MODE_STRICT which allows only:
 *   - read(), write(), exit(), sigreturn()
 * All other syscalls are blocked and kill the process.
 *
 * USAGE:
 *   gcc -o seccomp_basic seccomp_basic.c
 *   ./seccomp_basic
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <linux/seccomp.h>
#include <sys/syscall.h>
#include <errno.h>
#include <string.h>

void print_status(void)
{
    int mode = prctl(PR_GET_SECCOMP);

    printf("\n=== Status ===\n");
    printf("PID: %d\n", getpid());
    printf("SECCOMP mode: %d ", mode);

    switch(mode) {
        case 0: printf("(DISABLED)\n"); break;
        case 1: printf("(STRICT)\n"); break;
        case 2: printf("(FILTER)\n"); break;
        default: printf("(UNKNOWN)\n");
    }
    printf("\n");
}

int main(void)
{
    printf("=== SECCOMP Strict Mode Test ===\n");

    print_status();

    printf("=== Before SECCOMP ===\n");
    printf("getpid(): %d (ALLOWED)\n", getpid());
    printf("getuid(): %d (ALLOWED)\n", getuid());

    printf("\n=== Enabling SECCOMP STRICT mode ===\n");
    printf("Only read/write/exit/sigreturn will be allowed\n");
    printf("All other syscalls will KILL the process\n\n");

    /* Enable strict seccomp mode */
    if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_STRICT) == 0) {
        /* Can only use write() now */
        const char *msg = "SECCOMP enabled successfully\n\n";
        write(STDOUT_FILENO, msg, strlen(msg));

        /* Try getpid() - this will KILL the process */
        msg = "Attempting getpid()...\n";
        write(STDOUT_FILENO, msg, strlen(msg));

        getpid();  /* This kills the process with SIGKILL */

        msg = "(Should not reach here)\n";
        write(STDOUT_FILENO, msg, strlen(msg));
    } else {
        printf("Failed to enable SECCOMP\n");
        return 1;
    }

    return 0;
}
