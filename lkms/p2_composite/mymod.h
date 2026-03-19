/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

#ifndef _MYMOD_H
#define _MYMOD_H

/*
 * This header file contains the declarations for the functions and variables
 * that are shared between the different source files of the 'mymod' module.
 */

/**
 * mod_routine() - A function defined in routine.c that uses a shared variable.
 *
 * Return: The incremented value of the shared variable 'new'.
 */
int mod_routine(void);

/*
 * The declaration of the shared variable 'new'. It is defined in data.c.
 * Using 'extern' tells the compiler that this variable exists but is defined
 * in another source file.
 */
extern int new;

#endif /* _MYMOD_H */

