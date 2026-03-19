/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
*/



#include <linux/module.h>	
#include <linux/kernel.h>	
#include <linux/init.h>         
#include <linux/kthread.h>	
#include <linux/sched.h>	
#include <linux/semaphore.h>    
#include <linux/delay.h>	


struct kthr_data {
	const char *name;	
	struct semaphore *sem1; 
	struct semaphore *sem2;  
};

static struct kthr_data prod, cons;
static struct semaphore psem, csem;
static struct task_struct *pthr, *cthr;

static int prod_fct(void *data)
{
	struct kthr_data *pdata = (struct kthr_data*)data;
	while(1) {
		down(pdata->sem1); // wait for consumer
		pr_info("%s: Generating data\n", pdata->name);
		mdelay(100);
		pr_info("%s: notify consumer\n",pdata->name);
		up(pdata->sem2);// notify consumer
		if (kthread_should_stop())
			break;
	}
	return 0;
}

static int cons_fct(void *data)
{
        struct kthr_data *pdata = (struct kthr_data*)data;
        while(1) {
                down(pdata->sem1);// wait for producer
                pr_info("%s: signal recvd form producer\n", pdata->name);
                mdelay(100);
                pr_info("%s: Done consuming, notify producer & wait for next chunck\n",pdata->name);
                up(pdata->sem2); // notify producer
                if (kthread_should_stop())
                        break;
        }
        return 0;
}

static int __init kthr_init(void)
{
	sema_init(&psem, 1); 
	sema_init(&csem, 0);

	prod.name = "producer"; 
	cons.name = "consumer";

	prod.sem1 = &psem; 
	prod.sem2 = &csem;

	cons.sem1 = &csem; 
	cons.sem2 = &psem;

	pthr = kthread_run(prod_fct, &prod,"producer");
	if(IS_ERR(pthr)){
                pr_err("%s: unable to start kernel thread\n",__func__);
                return PTR_ERR(pthr);
        }

	cthr = kthread_run(cons_fct, &cons,"consumer");
	if(IS_ERR(cthr)){
                pr_err("%s: unable to start kernel thread\n",__func__);
                return PTR_ERR(cthr);
        }

	return 0;
}

static void __exit kthr_exit(void)
{
	kthread_stop(pthr);
	kthread_stop(cthr);
}

module_init(kthr_init);
module_exit(kthr_exit);

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("sync with kernel semaphores");
MODULE_LICENSE("Dual MIT/GPL");

