/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/kthread.h>
#include <linux/delay.h>

/*
 * What triggers this notifier?
 * - MODULE_STATE_COMING: A module is being loaded (e.g., `insmod`, `modprobe`).
 * - MODULE_STATE_LIVE:   Module finished init and is live.
 * - MODULE_STATE_GOING:  A module is being unloaded (e.g., `rmmod`).
 * These callbacks observe global module lifecycle transitions.
 */

static int module_events = 0;
static int load_events = 0;
static int unload_events = 0;
static struct task_struct *stats_thread;

static int module_notifier_callback(struct notifier_block *nb, unsigned long val, void *data)
{
    struct module *mod = data;
    
    module_events++;
    
    switch (val) {
    case MODULE_STATE_COMING:
        load_events++;
        pr_info("%s: Module '%s' is being loaded (total loads: %d)\n", 
                __func__, mod->name, load_events);
        break;
        
    case MODULE_STATE_LIVE:
        pr_info("%s: Module '%s' is now live and ready\n", 
                __func__, mod->name);
        break;
        
    case MODULE_STATE_GOING:
        unload_events++;
        pr_info("%s: Module '%s' is being unloaded (total unloads: %d)\n", 
                __func__, mod->name, unload_events);
        break;
        
    default:
        pr_info("%s: Module '%s' state change: %lu\n", 
                __func__, mod->name, val);
        break;
    }
    
    return NOTIFY_OK;
}

static struct notifier_block module_nb = {
    .notifier_call = module_notifier_callback,
    .priority = 0,
};

static int stats_thread_fn(void *data)
{
    int i = 0;
    
    pr_info("%s: Module statistics thread started\n", __func__);
    
    while (!kthread_should_stop() && i < 20) {
        pr_info("%s: Stats - Total events: %d, Loads: %d, Unloads: %d\n", 
                __func__, module_events, load_events, unload_events);
        
        if (i % 5 == 0) {
            pr_info("%s: Module notifier is actively monitoring...\n", __func__);
        }
        
        msleep(2000);
        i++;
    }
    
    pr_info("%s: Module statistics thread exiting\n", __func__);
    return 0;
}

static int __init module_notifier_init(void)
{
    int ret;
    
    pr_info("%s: Module notifier module loaded\n", __func__);
    
    ret = register_module_notifier(&module_nb);
    if (ret) {
        pr_err("%s: Failed to register module notifier: %d\n", __func__, ret);
        return ret;
    }
    
    stats_thread = kthread_run(stats_thread_fn, NULL, "module_stats");
    if (IS_ERR(stats_thread)) {
        pr_err("%s: Failed to create stats thread\n", __func__);
        unregister_module_notifier(&module_nb);
        return PTR_ERR(stats_thread);
    }
    
    pr_info("%s: Module notifier registered successfully\n", __func__);
    return 0;
}

static void __exit module_notifier_exit(void)
{
    if (stats_thread)
        kthread_stop(stats_thread);
        
    unregister_module_notifier(&module_nb);
    
    pr_info("%s: Final statistics - Events: %d, Loads: %d, Unloads: %d\n", 
            __func__, module_events, load_events, unload_events);
    pr_info("%s: Module notifier module unloaded\n", __func__);
}

module_init(module_notifier_init);
module_exit(module_notifier_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("Module loading/unloading notifier demonstration");
MODULE_VERSION("1.0");
