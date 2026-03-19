/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ktime.h>

int init_module(void)
{
	ktime_t start, end;
	s64 elapsed_ns;

	start = ktime_get();
	msleep(100);
	end = ktime_get();

	elapsed_ns = ktime_to_ns(ktime_sub(end, start));
	pr_info("Function took %lld ns\n", elapsed_ns);
	return -EINTR;
}

void cleanup_module(void)
{
}

MODULE_AUTHOR("raghu@techveda.org");
MODULE_DESCRIPTION("LKD_CW: HR delay");
MODULE_LICENSE("Dual MIT/GPL");
