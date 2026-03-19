/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is licensed under GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

char LICENSE[] SEC("license") = "GPL";

SEC("kprobe/__x64_sys_execve")
int BPF_KPROBE(handle_exec_kp)
{
	__u32 pid = bpf_get_current_pid_tgid() >> 32;

	bpf_printk("kprobe_exec: pid=%u execve\n", pid);
	return 0;
}

