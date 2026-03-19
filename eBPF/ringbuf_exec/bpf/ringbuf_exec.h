/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is licensed under GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

#ifndef RINGBUF_EXEC_H
#define RINGBUF_EXEC_H

struct event {
	__u32 pid;
	char comm[16];
};

#endif /* RINGBUF_EXEC_H */

