/*
 * cap_demo.c - Simple Capability Demonstration
 *
 * Shows how capabilities enable specific privileges without root access.
 *
 * USAGE:
 *   gcc -o cap_demo cap_demo.c -lcap
 *   ./cap_demo                                        # No caps
 *   sudo setcap cap_net_bind_service=+ep cap_demo    # Port binding
 *   sudo setcap cap_dac_read_search=+ep cap_demo     # File reading
 *   sudo setcap cap_net_bind_service,cap_dac_read_search=+ep cap_demo  # Both
 *
 *   Then run ./cap_demo again to see the difference!
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/capability.h>
#include <sys/socket.h>
#include <netinet/in.h>

int check_capability(cap_value_t cap)
{
    cap_t caps;
    cap_flag_value_t value;

    caps = cap_get_proc();
    if (!caps)
        return 0;

    if (cap_get_flag(caps, cap, CAP_EFFECTIVE, &value) == -1) {
        cap_free(caps);
        return 0;
    }

    cap_free(caps);
    return (value == CAP_SET);
}

void print_status(void)
{
    cap_t caps = cap_get_proc();
    char *caps_text = caps ? cap_to_text(caps, NULL) : NULL;

    printf("\n=== Current Status ===\n");
    printf("UID: %u\n", getuid());
    printf("Capabilities: %s\n", caps_text && strlen(caps_text) ? caps_text : "none");
    printf("  CAP_NET_BIND_SERVICE: %s\n",
           check_capability(CAP_NET_BIND_SERVICE) ? "YES" : "NO");
    printf("  CAP_DAC_READ_SEARCH:  %s\n\n",
           check_capability(CAP_DAC_READ_SEARCH) ? "YES" : "NO");

    if (caps_text) cap_free(caps_text);
    if (caps) cap_free(caps);
}

int try_bind_port(int port)
{
    int sockfd, result;
    struct sockaddr_in addr;
    int opt = 1;

    printf("Bind to port %d: ", port);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf("FAIL (socket error)\n");
        return -1;
    }

    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    result = bind(sockfd, (struct sockaddr *)&addr, sizeof(addr));
    close(sockfd);

    if (result < 0) {
        printf("FAIL (%s)\n", strerror(errno));
        return -1;
    }

    printf("SUCCESS\n");
    return 0;
}

int try_read_file(const char *path)
{
    FILE *fp;
    char buf[50];

    printf("Read %s: ", path);

    fp = fopen(path, "r");
    if (!fp) {
        printf("FAIL (%s)\n", strerror(errno));
        return -1;
    }

    if (fgets(buf, sizeof(buf), fp)) {
        printf("SUCCESS\n");
    }

    fclose(fp);
    return 0;
}

void print_help(void)
{
    int has_bind = check_capability(CAP_NET_BIND_SERVICE);
    int has_read = check_capability(CAP_DAC_READ_SEARCH);

    printf("\n=== How to Grant Capabilities ===\n");

    if (!has_bind && !has_read) {
        printf("Try:\n");
        printf("  sudo setcap cap_net_bind_service=+ep cap_demo\n");
        printf("  sudo setcap cap_dac_read_search=+ep cap_demo\n");
        printf("  sudo setcap cap_net_bind_service,cap_dac_read_search=+ep cap_demo\n");
    } else {
        printf("Remove: sudo setcap -r cap_demo\n");
    }
    printf("\n");
}

int main(void)
{
    printf("\n=== Linux Capabilities Demo ===\n");
    print_status();

    printf("=== Tests ===\n");
    try_bind_port(80);
    try_bind_port(443);
    try_bind_port(8080);
    try_read_file("/etc/shadow");
    try_read_file("/etc/sudoers");
    try_read_file("/root/.bashrc");

    print_help();
    return 0;
}
