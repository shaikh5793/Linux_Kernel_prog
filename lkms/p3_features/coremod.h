/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

#ifndef _COREMOD_H
#define _COREMOD_H

/*
 * set_counter() - Sets the value of the internal counter.
 * @a: The new integer value for the counter.
 *
 * This function is exported for other kernel modules to use.
 *
 * Return: Always 0.
 */
int set_counter(int a);

/*
 * get_counter() - Gets the current value of the internal counter.
 *
 * This function is exported for other kernel modules to use.
 *
 * Return: The current integer value of the counter.
 */
int get_counter(void);

#endif /* _COREMOD_H */

