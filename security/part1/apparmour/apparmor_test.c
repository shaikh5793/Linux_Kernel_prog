/*
 * apparmor_test.c - Demonstrate AppArmor overriding capabilities
 *
 * This program has CAP_DAC_READ_SEARCH capability, allowing it to read
 * any file. However, when an AppArmor profile is applied, it can be
 * blocked from reading specific files DESPITE having the capability.
 *
 * USAGE:
 *   gcc -o apparmor_test apparmor_test.c -lcap
 *   sudo setcap cap_dac_read_search=+ep apparmor_test
 *   ./apparmor_test                    # Phase 1: Works
 *   sudo ./setup_apparmor.sh           # Apply profile
 *   ./apparmor_test                    # Phase 2: Blocked by AppArmor
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/capability.h>
#include <sys/types.h>
#include <fcntl.h>

void print_status(void)
{
    cap_t caps = cap_get_proc();
    char *caps_text = caps ? cap_to_text(caps, NULL) : NULL;
    FILE *fp;
    char profile[256] = {0};

    printf("\n=== Status ===\n");
    printf("UID: %u\n", getuid());
    printf("Capabilities: %s\n", caps_text && strlen(caps_text) ? caps_text : "none");

    /* Check AppArmor confinement status */
    fp = fopen("/proc/self/attr/current", "r");
    if (fp) {
        if (fgets(profile, sizeof(profile), fp)) {
            profile[strcspn(profile, "\n")] = 0;
            printf("AppArmor: %s\n", profile);
        }
        fclose(fp);
    }
    printf("\n");

    if (caps_text) cap_free(caps_text);
    if (caps) cap_free(caps);
}

int try_read_file(const char *path)
{
    FILE *fp;
    char buf[100];

    printf("Read %s: ", path);

    fp = fopen(path, "r");
    if (!fp) {
        printf("FAIL (%s)\n", strerror(errno));
        return -1;
    }

    if (fgets(buf, sizeof(buf), fp)) {
        buf[40] = '\0';
        printf("SUCCESS\n");
    }

    fclose(fp);
    return 0;
}

int try_write_file(const char *path)
{
    FILE *fp;

    printf("Write %s: ", path);

    fp = fopen(path, "w");
    if (!fp) {
        printf("FAIL (%s)\n", strerror(errno));
        return -1;
    }

    fprintf(fp, "Test write\n");
    printf("SUCCESS\n");
    fclose(fp);
    return 0;
}

void print_help(void)
{
    cap_t caps = cap_get_proc();
    cap_flag_value_t value = CAP_CLEAR;
    FILE *fp;
    char profile[256] = {0};
    int confined = 0;

    if (caps) {
        cap_get_flag(caps, CAP_DAC_READ_SEARCH, CAP_EFFECTIVE, &value);
        cap_free(caps);
    }

    printf("\n=== Instructions ===\n");

    if (value != CAP_SET) {
        /* No capability - show how to grant it */
        printf("Grant capability:\n");
        printf("  sudo setcap cap_dac_read_search=+ep apparmor_test\n\n");
    } else {
        /* Has capability - check if confined */
        fp = fopen("/proc/self/attr/current", "r");
        if (fp) {
            if (fgets(profile, sizeof(profile), fp)) {
                confined = !strstr(profile, "unconfined");
            }
            fclose(fp);
        }

        if (!confined) {
            /* Has capability, not confined - show how to apply AppArmor */
            printf("Apply AppArmor profile:\n");
            printf("  sudo bash setup_apparmor.sh\n\n");
        } else {
            /* Confined by AppArmor - show how to check logs */
            printf("Check AppArmor denials:\n");
            printf("  sudo dmesg | grep -i apparmor | tail\n\n");
            printf("Disable AppArmor:\n");
            printf("  sudo aa-disable apparmor_test\n\n");
        }
    }
}

int main(void)
{
    print_status();

    printf("=== Tests ===\n");
    try_read_file("/etc/shadow");
    try_read_file("/etc/sudoers");
    try_write_file("/tmp/apparmor_test.txt");

    print_help();

    return 0;
}
