/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 */



#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/semaphore.h>
#include <linux/slab.h>

typedef struct {
	int a;
	int b;
} priv_data;

static priv_data *p;
struct task_struct *t1, *t2;

DEFINE_SEMAPHORE(sem, 1);

static int kthr_reader(void *arg)
{
	int ret;
	if (!p) {
		pr_err("%s: The pointer p is NULL\n", __func__);
		return -EINVAL;
	}
	pr_info("%s: Attempting to lock semaphore.\n", __func__);

	ret = down_interruptible(&sem);
	if (ret) {
		/*
		 * down_interruptible() can return non-zero if it was 
		 * interrupted by a signal before the semaphore could be acquired.
		 */
		pr_err("%s: down_interruptible failed with ret = %d\n", __func__, ret);
		return -EINTR;
	}

	pr_info("%s: read a = %d, b = %d\n", __func__, p->a, p->b);
	up(&sem);

	pr_info("%s: Exiting reader thread.\n", __func__);

	return 0;
}

static int kthr_writer(void *arg)
{
	int ret;

	if (!p) {
		pr_err("%s: The pointer p is NULL\n", __func__);
		return -EINVAL;
	}
	pr_info("%s: Attempting to lock semaphore.\n", __func__);

	ret = down_interruptible(&sem);
	if (ret) {
		pr_err("%s: down_interruptible failed with ret = %d\n", __func__, ret);
		return -EINTR;
	}

	p->a = 10;
	pr_info("%s: Successfully wrote p->a = 10\n", __func__);

	/* Sleep to simulate some long operation */
	ssleep(10);

	p->b = 20;
	pr_info("%s: Successfully wrote p->b = 20\n", __func__);

	up(&sem);

	pr_info("%s: Exiting writer thread.\n", __func__);
	return 0;
}

static void data_init(priv_data * data)
{
	data->a = 0;
	data->b = 0;
}
static int __init kthr_init(void)
{
	p = (priv_data *) kmalloc(sizeof(priv_data), GFP_KERNEL);

	data_init(p);

	t1 = kthread_run(kthr_writer, NULL, "Kwriter");
	if(IS_ERR(t1)){
                pr_err("%s: unable to start kernel thread\n",__func__);
                return PTR_ERR(t1);
        }

	t2 = kthread_run(kthr_reader, NULL, "Kreader");
	if(IS_ERR(t2)){
                pr_err("%s: unable to start kernel thread\n",__func__);
                return PTR_ERR(t2);
        }

	return 0;
}

static void __exit kthr_exit(void)
{
	kfree(p);
}

module_init(kthr_init);
module_exit(kthr_exit);

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("Kernel semaphore Interface");
MODULE_LICENSE("Dual MIT/GPL");
