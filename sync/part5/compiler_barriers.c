/*
Copyright (c) 2024 TECH VEDA(www.techveda.org)
Author: Raghu Bharadwaj

This software is dual-licensed under the MIT License and GPL v2.
See the accompanying LICENSE file for the full text.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/compiler.h>

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("Compiler barriers demonstration");
MODULE_VERSION("1.0");

static struct task_struct *demo_thread;
static volatile int flag1 = 0;
static volatile int flag2 = 0;
static int counter = 0;

static int demo_thread_fn(void *data)
{
    int i, local_counter;
    
    pr_info("%s: Compiler barriers demo thread started\n", __func__);
    
    for (i = 0; i < 10 && !kthread_should_stop(); i++) {
        counter = i * 10;
        flag1 = 1;
        
        barrier();
        
        flag2 = 1;
        local_counter = counter;
        
        pr_info("%s: Iteration %d: counter=%d, flags=%d,%d\n", 
                __func__, i, local_counter, flag1, flag2);
        
        barrier();
        
        flag1 = 0;
        flag2 = 0;
        
        msleep(500);
    }
    
    pr_info("%s: Demo without barriers\n", __func__);
    
    for (i = 0; i < 5 && !kthread_should_stop(); i++) {
        counter = i * 100;
        flag1 = 1;
        flag2 = 1;
        local_counter = counter;
        
        pr_info("%s: No barrier - counter=%d, flags=%d,%d\n", 
                __func__, local_counter, flag1, flag2);
        
        flag1 = 0;
        flag2 = 0;
        
        msleep(300);
    }
    
    pr_info("%s: READ_ONCE/WRITE_ONCE demonstration\n", __func__);
    
    for (i = 0; i < 5 && !kthread_should_stop(); i++) {
        WRITE_ONCE(counter, i * 50);
        WRITE_ONCE(flag1, 1);
        
        local_counter = READ_ONCE(counter);
        
        pr_info("%s: READ_ONCE counter=%d, flag=%d\n", 
                __func__, local_counter, READ_ONCE(flag1));
        
        WRITE_ONCE(flag1, 0);
        
        msleep(400);
    }
    
    pr_info("%s: Compiler barriers demo thread exiting\n", __func__);
    return 0;
}

static int __init compiler_barriers_init(void)
{
    pr_info("%s: Compiler barriers module loaded\n", __func__);
    pr_info("%s: Demonstrating barrier(), READ_ONCE(), and WRITE_ONCE()\n", __func__);
    
    demo_thread = kthread_run(demo_thread_fn, NULL, "compiler_demo");
    if (IS_ERR(demo_thread)) {
        pr_err("%s: Failed to create demo thread\n", __func__);
        return PTR_ERR(demo_thread);
    }
    
    return 0;
}

static void __exit compiler_barriers_exit(void)
{
    if (demo_thread)
        kthread_stop(demo_thread);
    
    pr_info("%s: Compiler barriers module unloaded\n", __func__);
}

module_init(compiler_barriers_init);
module_exit(compiler_barriers_exit);