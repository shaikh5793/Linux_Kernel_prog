/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 */



#include <linux/kernel.h>
#include "mymod.h"

/**
 * mod_routine() - A simple function that increments a shared variable.
 *
 * This function demonstrates how a function in one source file can access
 * a variable defined in another source file (data.c) via a shared header.
 *
 * Return: The new, incremented value of the shared variable.
 */
int mod_routine(void)
{
	pr_info("cmod: Inside %s\n", __func__);
	return ++new;
}
