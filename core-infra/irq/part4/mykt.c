/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/sched/task.h>	// {get,put}_task_struct()
#include <linux/sched/signal.h>	// signal_pending()
#include <linux/signal.h>	// allow_signal()
#include <linux/kthread.h>

#define MODULE_NAME	"mykthread"
struct task_struct *Kthrd;

/* Our simple kernel thread. */
static int mykt(void *arg)
{
	pr_info("%s:name: %s PID: %d TGID: %d\n",
		__func__, current->comm, current->pid, current->tgid);

	allow_signal(SIGINT);
	allow_signal(SIGQUIT);

	while (1) {
		pr_info("%s:KThread %d going to sleep now...\n", __func__,
			current->pid);
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();	// and yield the processor...
		// we're back on! has to be due to either the SIGINT or SIGQUIT signal!
		if (signal_pending(current))
			break;
	}
	// We've been interrupted by a signal...
	set_current_state(TASK_RUNNING);
	pr_info("%s:KThread %d exiting now...\n", __func__, current->pid);
	BUG();
	return 0;
}

static int kthrd_init(void)
{
	int ret = 0, i = 0;

	pr_info("kt1: Create a kernel thread...\n");

	Kthrd = kthread_run(mykt, NULL, "%s.%d", MODULE_NAME, i);
	/* 2nd arg is (void * arg) to pass, ret val is task ptr on success */
	if (ret < 0) {
		pr_err("kt1: kthread_create failed (%d)\n", ret);
		return ret;
	}
	get_task_struct(Kthrd);	// inc refcnt, "take" the task struct

	pr_info("Module %s initialized, thread task ptr is 0x%pK.\n"
		"See the new k thread '%s.0' with ps "
		"(and kill it with SIGINT or SIGQUIT)\n",
		MODULE_NAME, Kthrd, MODULE_NAME);
	return 0;
}

static void kthrd_exit(void)
{
	if (Kthrd)
		kthread_stop(Kthrd);
	put_task_struct(Kthrd);	// dec refcnt, "release" the task struct
	pr_info("%s:Module %s unloaded.\n", __func__, MODULE_NAME);
}

module_init(kthrd_init);
module_exit(kthrd_exit);

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org");
MODULE_DESCRIPTION("Simple kernel thread");
MODULE_LICENSE("Dual MIT/GPL");
