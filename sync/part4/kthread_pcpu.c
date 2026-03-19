/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
*/



#include <linux/module.h>
#include <linux/init.h>
#include <linux/percpu.h>
#include <linux/cpumask.h>

#define MODNAME "[PERCPU] "

static DEFINE_PER_CPU(long, cpuvar) = 10;
static long __percpu *cpualloc;

static int __init pcpu_init(void)
{
	int cpu;

	pr_info(MODNAME"module loaded at 0x%p\n", pcpu_init);

	
	for_each_possible_cpu(cpu)
		per_cpu(cpuvar, cpu) = 15;

	pr_info(MODNAME"cpuvar on cpu%d  = %ld\n",smp_processor_id(), get_cpu_var(cpuvar)++);
	put_cpu_var(cpuvar);

	
	cpualloc = alloc_percpu(long);

	
	for_each_possible_cpu(cpu)
		*per_cpu_ptr(cpualloc, cpu) = 100;

	return 0;
}

static void __exit pcpu_exit(void)
{
	int cpu;
	pr_info(MODNAME"exit module...\n");

	for_each_possible_cpu(cpu)
		pr_info("cpuvar cpu%d = %ld\n", cpu, per_cpu(cpuvar, cpu));

	pr_info(MODNAME"cpualloc = %ld\n", *per_cpu_ptr(cpualloc, smp_processor_id()));
	free_percpu(cpualloc);

	pr_info(MODNAME"module unloaded from 0x%p\n", pcpu_exit);
}

module_init(pcpu_init);
module_exit(pcpu_exit);

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("percpu data");
MODULE_LICENSE("Dual MIT/GPL");
