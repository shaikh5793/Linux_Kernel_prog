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

struct cpu_stats {
	unsigned long counter;
	unsigned long allocations;
	char name[32];
};

static struct cpu_stats __percpu *cpu_data;
static unsigned long __percpu *simple_counter;

static int __init percpu_basic_init(void)
{
	int cpu;
	struct cpu_stats *stats;

	pr_info("[PERCPU] per-CPU memory allocation (SMP scalability)\n");

	/* alloc_percpu() allocates per-CPU memory */
	cpu_data = alloc_percpu(struct cpu_stats);
	if (!cpu_data) {
		pr_err("[PERCPU] alloc_percpu(cpu_stats): FAILED\n");
		return -ENOMEM;
	}
	pr_info("[PERCPU] alloc_percpu(cpu_stats): %zu bytes per CPU\n",
		sizeof(struct cpu_stats));

	simple_counter = alloc_percpu(unsigned long);
	if (!simple_counter) {
		pr_err("[PERCPU] alloc_percpu(counter): FAILED\n");
		free_percpu(cpu_data);
		return -ENOMEM;
	}

	/* Initialize per-CPU data */
	for_each_possible_cpu(cpu) {
		stats = per_cpu_ptr(cpu_data, cpu);
		stats->counter = cpu * 10;
		stats->allocations = 0;
		snprintf(stats->name, sizeof(stats->name), "CPU_%d", cpu);
		*per_cpu_ptr(simple_counter, cpu) = cpu * 100;
	}

	/* get_cpu_var() provides safe access with preemption disabled */
	stats = get_cpu_var(cpu_data);
	stats->counter++;
	stats->allocations = 42;
	pr_info("[PERCPU] get_cpu_var(): CPU %d, counter=%lu\n",
		smp_processor_id(), stats->counter);
	put_cpu_var(cpu_data);

	return 0;
}

static void __exit percpu_basic_exit(void)
{
	int cpu;

	/* Show final statistics */
	for_each_possible_cpu(cpu) {
		struct cpu_stats *stats = per_cpu_ptr(cpu_data, cpu);
		unsigned long *counter = per_cpu_ptr(simple_counter, cpu);
		pr_info("[PERCPU] CPU %d: %s, counter=%lu, simple=%lu\n",
			cpu, stats->name, stats->counter, *counter);
	}

	free_percpu(simple_counter);
	free_percpu(cpu_data);
}

module_init(percpu_basic_init);
module_exit(percpu_basic_exit);

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("Per-CPU memory basic operations");
MODULE_LICENSE("Dual MIT/GPL");
