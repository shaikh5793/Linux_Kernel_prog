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

/* Static per-CPU variables (allocated at compile time) */
DEFINE_PER_CPU(unsigned long, static_counter);
DEFINE_PER_CPU(char[64], cpu_buffer);

struct cpu_context {
	unsigned long interrupts;
	pid_t current_pid;
};

DEFINE_PER_CPU(struct cpu_context, cpu_ctx);

static int __init static_percpu_init(void)
{
	int cpu;
	char *buffer;
	struct cpu_context *ctx;

	pr_info("[STATIC_PERCPU] static per-CPU variables (compile-time allocation)\n");

	/* Initialize static per-CPU variables */
	for_each_possible_cpu(cpu) {
		per_cpu(static_counter, cpu) = cpu * 10;
		
		ctx = per_cpu_ptr(&cpu_ctx, cpu);
		ctx->interrupts = 0;
		ctx->current_pid = 0;
		
		buffer = per_cpu(cpu_buffer, cpu);
		snprintf(buffer, 64, "CPU_%d_data", cpu);
	}

	/* this_cpu_* operations are fast (no preemption disable needed) */
	this_cpu_inc(static_counter);
	this_cpu_add(static_counter, 5);
	
	ctx = this_cpu_ptr(&cpu_ctx);
	ctx->interrupts += 10;
	ctx->current_pid = current->pid;
	
	pr_info("[STATIC_PERCPU] this_cpu ops: CPU %d, counter=%lu, pid=%d\n",
		smp_processor_id(),
		this_cpu_read(static_counter),
		ctx->current_pid);

	/* Safe access with preemption disabled */
	cpu = get_cpu();
	buffer = per_cpu(cpu_buffer, cpu);
	snprintf(buffer, 64, "CPU_%d_updated_%lu", cpu, per_cpu(static_counter, cpu));
	pr_info("[STATIC_PERCPU] get_cpu(): CPU %d, buffer=%s\n", cpu, buffer);
	put_cpu();

	return 0;
}

static void __exit static_percpu_exit(void)
{
	int cpu;

	/* Show final statistics */
	for_each_possible_cpu(cpu) {
		struct cpu_context *ctx = per_cpu_ptr(&cpu_ctx, cpu);
		char *buffer = per_cpu(cpu_buffer, cpu);
		pr_info("[STATIC_PERCPU] CPU %d: counter=%lu, interrupts=%lu, buffer=%s\n",
			cpu, per_cpu(static_counter, cpu), ctx->interrupts, buffer);
	}
}

module_init(static_percpu_init);
module_exit(static_percpu_exit);

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("Static per-CPU variables demonstration");
MODULE_LICENSE("Dual MIT/GPL");