/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h> /* For msleep */

/**
 * rate_limit_init() - Demonstrates printk rate-limiting.
 *
 * This function is called when the module is loaded. It shows how
 * printk_ratelimit() can prevent log floods by attempting to print
 * many messages in a tight loop, and then shows how the limit resets
 * after a delay.
 *
 * Return: 0 on success.
 */
static int __init rate_limit_init(void)
{
	int i;
	int suppressed_count = 0;

	pr_info("--- Rate-limiting demonstration starting. ---\n");

	/*
	 * Attempt to flood the logs with 20 messages in a tight loop.
	 * Most of these should be suppressed by printk_ratelimit().
	 */
	pr_info("Attempting to print 20 messages in a burst...\n");
	for (i = 0; i < 20; i++) {
		if (printk_ratelimit()) {
			pr_info("Burst message #%d\n", i + 1);
		} else {
			suppressed_count++;
		}
	}
	pr_info("Burst finished. Suppressed %d messages due to rate-limiting.\n",
		suppressed_count);

	/*
	 * The default rate limit is 5 messages per 5 seconds. We wait for
	 * 6 seconds to ensure the rate-limit window has expired.
	 */
	pr_info("Waiting for 6 seconds to reset the rate limit...\n");
	msleep(6000);

	/* This message should now print successfully. */
	pr_info("Attempting to print a message after the delay...\n");
	if (printk_ratelimit())
		pr_info("This message appeared after the rate-limit reset.\n");
	else
		pr_warn("This message should have printed but was suppressed.\n");


	pr_info("--- Rate-limiting demonstration finished. ---\n");

	return 0;
}

/**
 * rate_limit_exit() - The module exit function.
 *
 * This function is called when the module is unloaded.
 */
static void __exit rate_limit_exit(void)
{
	pr_info("Module unloaded. Rate-limiting demonstration is over.\n");
}

module_init(rate_limit_init);
module_exit(rate_limit_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("raghu@techveda.org");
MODULE_DESCRIPTION("Demonstrates printk rate-limiting");

