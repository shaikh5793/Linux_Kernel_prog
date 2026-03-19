/*
 * test_creds.c - Test program to demonstrate real vs effective UID
 *
 * Compile: gcc -o test_creds test_creds.c
 * Setup:   sudo chown root:root test_creds
 *          sudo chmod u+s test_creds  (set setuid bit)
 * Run:     ./test_creds
 */

#define _GNU_SOURCE  /* Needed for getresuid/getresgid */
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>

int main(void)
{
    uid_t ruid, euid, suid;
    gid_t rgid, egid, sgid;
    FILE *fp;
    char buffer[256];

    /* Get all UID types */
    getresuid(&ruid, &euid, &suid);
    getresgid(&rgid, &egid, &sgid);

    printf("=== User Space Credentials ===\n");
    printf("Real UID:      %u (original user who ran this)\n", ruid);
    printf("Effective UID: %u (used for permission checks)\n", euid);
    printf("Saved UID:     %u (backup of euid)\n\n", suid);

    printf("Real GID:      %u\n", rgid);
    printf("Effective GID: %u\n", egid);
    printf("Saved GID:     %u\n\n", sgid);

    printf("=== Kernel Space Credentials ===\n");
    printf("Reading /proc/creds_basic:\n\n");

    /*
     * Read the proc file directly instead of using system()
     * This way the SAME process (with setuid) reads the file
     * and the kernel will show OUR credentials, not cat's
     */
    fp = fopen("/proc/creds_basic", "r");
    if (fp == NULL) {
        perror("Failed to open /proc/creds_basic");
        return 1;
    }

    /* Print the contents */
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        printf("%s", buffer);
    }

    fclose(fp);

    return 0;
}
