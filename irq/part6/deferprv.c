/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/workqueue.h>
#include <linux/sched.h>

static struct workqueue_struct *my_workq;
static struct work_struct work;

static void w_fun(struct work_struct *w_arg)
{
  pr_info("current task  name: %s pid :%d \n", current->comm,(int)current->pid);
}

static int __init my_init(void)
{
	my_workq = create_workqueue("my_workq");
	INIT_WORK(&work, w_fun);
	queue_work(my_workq, &work);
	return 0;

}

static void __exit my_exit(void)
{
	destroy_workqueue(my_workq);
}

module_init(my_init);
module_exit(my_exit);

MODULE_AUTHOR("support@techveda.org");
MODULE_DESCRIPTION("LKD_CW: private workqueue");
MODULE_LICENSE("GPL v2");
