/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
* 

 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cpu.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/smp.h>
#include <linux/cpumask.h>

static struct task_struct *monitor_thread;
static int cpu_events = 0;

/*
 * What triggers these callbacks?
 * - CPU online: a CPU is brought online (hotplug on), e.g.:
 *     echo 1 > /sys/devices/system/cpu/cpuN/online
 * - CPU offline: a CPU is taken offline (hotplug off), e.g.:
 *     echo 0 > /sys/devices/system/cpu/cpuN/online
 * Registered via cpuhp_setup_state(), not a classic notifier chain,
 * but conceptually similar lifecycle callbacks for CPU hotplug events.
 */

static int cpu_hotplug_online(unsigned int cpu)
{
    cpu_events++;
    pr_info("%s: CPU %d came online (events: %d)\n", __func__, cpu, cpu_events);
    return 0;
}

static int cpu_hotplug_offline(unsigned int cpu)
{
    cpu_events++;
    pr_info("%s: CPU %d going offline (events: %d)\n", __func__, cpu, cpu_events);
    return 0;
}

static int monitor_thread_fn(void *data)
{
    int online_cpus, possible_cpus;
    int i = 0;
    
    pr_info("%s: Monitor thread started\n", __func__);
    
    while (!kthread_should_stop() && i < 20) {
        online_cpus = num_online_cpus();
        possible_cpus = num_possible_cpus();
        
        pr_info("%s: Iteration %d - Online CPUs: %d/%d, Events: %d\n", 
                __func__, i, online_cpus, possible_cpus, cpu_events);
        
        if (i % 5 == 0) {
            pr_info("%s: CPU mask info - online: %*pbl\n", 
                    __func__, cpumask_pr_args(cpu_online_mask));
        }
        
        cpus_read_lock();
        pr_info("%s: Acquired cpus_read_lock for safe CPU iteration\n", __func__);
        
        if (online_cpus > 1) {
            pr_info("%s: Multiple CPUs available for coordination\n", __func__);
        } else {
            pr_info("%s: Single CPU system or low CPU availability\n", __func__);
        }
        
        cpus_read_unlock();
        
        msleep(2000);
        i++;
    }
    
    pr_info("%s: Monitor thread exiting\n", __func__);
    return 0;
}

static int cpu_hotplug_state;

static int __init cpu_hotplug_init(void)
{
    int ret;
    
    pr_info("%s: CPU hotplug coordination module loaded\n", __func__);
    pr_info("%s: Initial CPU count - Online: %d, Possible: %d\n", 
            __func__, num_online_cpus(), num_possible_cpus());
    
    ret = cpuhp_setup_state(CPUHP_AP_ONLINE_DYN, "demo/cpu_hotplug",
                           cpu_hotplug_online, cpu_hotplug_offline);
    if (ret < 0) {
        pr_err("%s: Failed to setup CPU hotplug state: %d\n", __func__, ret);
        return ret;
    }
    cpu_hotplug_state = ret;
    
    monitor_thread = kthread_run(monitor_thread_fn, NULL, "cpu_monitor");
    if (IS_ERR(monitor_thread)) {
        pr_err("%s: Failed to create monitor thread\n", __func__);
        cpuhp_remove_state(cpu_hotplug_state);
        return PTR_ERR(monitor_thread);
    }
    
    return 0;
}

static void __exit cpu_hotplug_exit(void)
{
    if (monitor_thread)
        kthread_stop(monitor_thread);
    
    cpuhp_remove_state(cpu_hotplug_state);
    
    pr_info("%s: Total CPU events observed: %d\n", __func__, cpu_events);
    pr_info("%s: Final CPU count - Online: %d, Possible: %d\n", 
            __func__, num_online_cpus(), num_possible_cpus());
    pr_info("%s: CPU hotplug coordination module unloaded\n", __func__);
}

module_init(cpu_hotplug_init);
module_exit(cpu_hotplug_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("CPU hotplug coordination demonstration");
MODULE_VERSION("1.0");
