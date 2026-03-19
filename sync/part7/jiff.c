/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/jiffies.h>	// for jiffies, HZ

int init_module(void)
{
	unsigned long start = jiffies;
	msleep(100);		// Sleep for 100 ms
	unsigned long end = jiffies;

	pr_info("Slept for roughly %u jiffies\n", (unsigned)(end - start));
	pr_info("In ms: ~%u ms\n", jiffies_to_msecs(end - start));
	return 0;
}

void cleanup_module(void)
{
}

MODULE_AUTHOR("raghu@techveda.org");
MODULE_DESCRIPTION("LKD_CW: jiffies");
MODULE_LICENSE("Dual MIT/GPL");
