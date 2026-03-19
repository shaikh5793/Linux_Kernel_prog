/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/sched/task.h>

#define MODNAME "[deferred work]"

static void fn_deferred(struct work_struct *);

static DECLARE_WORK(work, (work_func_t)fn_deferred);

static void fn_deferred(struct work_struct *w_arg)
{
   pr_info("%s:current task name:%s pid:%d\n",__func__, current->comm, (int)current->pid);
   dump_stack();
//   schedule_work(&work);
}

static int __init my_init(void)
{
	pr_info("%s: scheduling deferred work\n",MODNAME);
	schedule_work(&work);
	return 0;
}

static void __exit my_exit(void)
{
	cancel_work_sync(&work);
}

module_init(my_init);
module_exit(my_exit);

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("deferring work");
MODULE_LICENSE("Dual MIT/GPL");
