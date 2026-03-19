/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is licensed under GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include "kprobe_exec.skel.h"

static volatile sig_atomic_t exiting;

static void on_sig(int s)
{
	(void)s;
	exiting = 1;
}

int main(void)
{
	struct kprobe_exec_bpf *skel = kprobe_exec_bpf__open();
	int err;

	if (!skel)
    {
		fprintf(stderr, "open failed\n");
		return 1;
	}

	if ((err = kprobe_exec_bpf__load(skel)))
    {    
		fprintf(stderr, "load failed: %d\n", err);
		goto out;
	}

	if ((err = kprobe_exec_bpf__attach(skel)))
   {
		fprintf(stderr, "attach failed: %d\n", err);
		goto out;
	}

	signal(SIGINT, on_sig);
	signal(SIGTERM, on_sig);
	printf("kprobe_exec running. Ctrl-C to exit.\n");

	while (!exiting)
		sleep(1);

out:
	kprobe_exec_bpf__destroy(skel);
	return err != 0;
}

