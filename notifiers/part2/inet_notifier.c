/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/inetdevice.h>
#include <linux/notifier.h>
#include <linux/kthread.h>
#include <linux/delay.h>

/*
 * What triggers this notifier?
 * - IPv4 address add/remove events on an interface.
 *   Typical actions that generate callbacks here:
 *   - Add address: `ip addr add <IP>/<CIDR> dev <dev>`
 *   - Del address: `ip addr del <IP>/<CIDR> dev <dev>`
 * Notes:
 * - The event values are delivered via the inetaddr notifier path and
 *   correspond to interface address lifecycle on the device.
 */

static int inet_events = 0;
static int addr_add_events = 0;
static int addr_del_events = 0;
static struct task_struct *monitor_thread;

static int inet_notifier_callback(struct notifier_block *nb, unsigned long event, void *ptr)
{
    struct in_ifaddr *ifa = (struct in_ifaddr *)ptr;
    struct net_device *dev = ifa->ifa_dev->dev;
    
    inet_events++;
    
    switch (event) {
    case NETDEV_UP:
        addr_add_events++;
        pr_info("%s: IP address added to '%s' (total adds: %d)\n", 
                __func__, dev->name, addr_add_events);
        break;
        
    case NETDEV_DOWN:
        addr_del_events++;
        pr_info("%s: IP address removed from '%s' (total removes: %d)\n", 
                __func__, dev->name, addr_del_events);
        break;
        
    default:
        pr_info("%s: IP address event %lu on '%s'\n", 
                __func__, event, dev->name);
        break;
    }
    
    return NOTIFY_OK;
}

static struct notifier_block inet_nb = {
    .notifier_call = inet_notifier_callback,
    .priority = 0,
};

static int monitor_thread_fn(void *data)
{
    int i = 0;
    
    pr_info("%s: Internet address monitor thread started\n", __func__);
    
    while (!kthread_should_stop() && i < 15) {
        pr_info("%s: IP stats - Total: %d, Adds: %d, Removes: %d\n", 
                __func__, inet_events, addr_add_events, addr_del_events);
        
        if (i % 5 == 0) {
            pr_info("%s: Monitoring IP address changes...\n", __func__);
        }
        
        msleep(4000);
        i++;
    }
    
    pr_info("%s: Internet address monitor thread exiting\n", __func__);
    return 0;
}

static int __init inet_notifier_init(void)
{
    int ret;
    
    pr_info("%s: Internet address notifier module loaded\n", __func__);
    
    ret = register_inetaddr_notifier(&inet_nb);
    if (ret) {
        pr_err("%s: Failed to register inet notifier: %d\n", __func__, ret);
        return ret;
    }
    
    monitor_thread = kthread_run(monitor_thread_fn, NULL, "inet_monitor");
    if (IS_ERR(monitor_thread)) {
        pr_err("%s: Failed to create monitor thread\n", __func__);
        unregister_inetaddr_notifier(&inet_nb);
        return PTR_ERR(monitor_thread);
    }
    
    pr_info("%s: Internet address notifier registered successfully\n", __func__);
    return 0;
}

static void __exit inet_notifier_exit(void)
{
    if (monitor_thread)
        kthread_stop(monitor_thread);
        
    unregister_inetaddr_notifier(&inet_nb);
    
    pr_info("%s: Final IP stats - Total: %d, Adds: %d, Removes: %d\n", 
            __func__, inet_events, addr_add_events, addr_del_events);
    pr_info("%s: Internet address notifier module unloaded\n", __func__);
}

module_init(inet_notifier_init);
module_exit(inet_notifier_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("Internet address notifier demonstration");
MODULE_VERSION("1.0");
