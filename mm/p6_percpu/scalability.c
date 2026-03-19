/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/percpu.h>
#include <linux/smp.h>
#include <linux/atomic.h>

/* Per-CPU counters (no contention) */
static unsigned long __percpu *percpu_packets;
static unsigned long __percpu *percpu_bytes;

/* Shared counters (causes contention) */
static atomic_long_t shared_packets = ATOMIC_LONG_INIT(0);
static atomic_long_t shared_bytes = ATOMIC_LONG_INIT(0);

static int __init percpu_scalability_init(void)
{
	int cpu, i;
	unsigned long total_packets = 0, total_bytes = 0;

	pr_info("[PERCPU_SCALE] per-CPU vs shared variable comparison\n");

	/* Allocate per-CPU variables */
	percpu_packets = alloc_percpu(unsigned long);
	percpu_bytes = alloc_percpu(unsigned long);
	if (!percpu_packets || !percpu_bytes) {
		pr_err("[PERCPU_SCALE] alloc_percpu(): FAILED\n");
		goto cleanup;
	}

	/* Initialize per-CPU data */
	for_each_possible_cpu(cpu) {
		*per_cpu_ptr(percpu_packets, cpu) = 0;
		*per_cpu_ptr(percpu_bytes, cpu) = 0;
	}

	/* Simulate packet processing */
	for (i = 0; i < 100; i++) {
		/* Per-CPU processing (scalable) */
		unsigned long *packets = get_cpu_var(percpu_packets);
		unsigned long *bytes = get_cpu_var(percpu_bytes);
		(*packets)++;
		(*bytes) += 1500;
		put_cpu_var(percpu_bytes);
		put_cpu_var(percpu_packets);

		/* Shared processing (contention) */
		atomic_long_inc(&shared_packets);
		atomic_long_add(1500, &shared_bytes);
	}

	/* Aggregate per-CPU statistics */
	for_each_possible_cpu(cpu) {
		total_packets += *per_cpu_ptr(percpu_packets, cpu);
		total_bytes += *per_cpu_ptr(percpu_bytes, cpu);
	}

	pr_info("[PERCPU_SCALE] per-CPU totals: packets=%lu, bytes=%lu\n",
		total_packets, total_bytes);
	pr_info("[PERCPU_SCALE] shared totals: packets=%ld, bytes=%ld\n",
		atomic_long_read(&shared_packets), atomic_long_read(&shared_bytes));

	return 0;

cleanup:
	if (percpu_packets) free_percpu(percpu_packets);
	if (percpu_bytes) free_percpu(percpu_bytes);
	return -ENOMEM;
}

static void __exit percpu_scalability_exit(void)
{
	free_percpu(percpu_packets);
	free_percpu(percpu_bytes);
}

module_init(percpu_scalability_init);
module_exit(percpu_scalability_exit);

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("Per-CPU memory scalability demonstration");
MODULE_LICENSE("Dual MIT/GPL");
