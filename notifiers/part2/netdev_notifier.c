/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/notifier.h>
#include <linux/kthread.h>
#include <linux/delay.h>

/*
 * What triggers this notifier?
 * - NETDEV_REGISTER / NETDEV_UNREGISTER: Interface created/removed
 *   Example: creating/removing virtual links, driver load/unload
 * - NETDEV_UP / NETDEV_DOWN: Interface administratively up/down
 *   Example: `ip link set <dev> up` / `ip link set <dev> down`
 * - NETDEV_CHANGEMTU: MTU changed
 *   Example: `ip link set <dev> mtu 1400`
 * - NETDEV_CHANGEADDR: MAC address changed
 *   Example: `ip link set <dev> address 02:11:22:33:44:55`
 */

static int netdev_events = 0;
static int up_events = 0;
static int down_events = 0;
static struct task_struct *monitor_thread;

static int netdev_notifier_callback(struct notifier_block *nb, unsigned long event, void *ptr)
{
    struct net_device *dev = netdev_notifier_info_to_dev(ptr);
    
    netdev_events++;
    
    switch (event) {
    case NETDEV_UP:
        up_events++;
        pr_info("%s: Interface '%s' is UP (total up events: %d)\n", 
                __func__, dev->name, up_events);
        break;
        
    case NETDEV_DOWN:
        down_events++;
        pr_info("%s: Interface '%s' is DOWN (total down events: %d)\n", 
                __func__, dev->name, down_events);
        break;
        
    case NETDEV_REGISTER:
        pr_info("%s: Interface '%s' registered\n", __func__, dev->name);
        break;
        
    case NETDEV_UNREGISTER:
        pr_info("%s: Interface '%s' unregistered\n", __func__, dev->name);
        break;
        
    case NETDEV_CHANGEMTU:
        pr_info("%s: Interface '%s' MTU changed to %d\n", 
                __func__, dev->name, dev->mtu);
        break;
        
    case NETDEV_CHANGEADDR:
        pr_info("%s: Interface '%s' MAC address changed\n", 
                __func__, dev->name);
        break;
        
    default:
        pr_info("%s: Interface '%s' event: %lu\n", 
                __func__, dev->name, event);
        break;
    }
    
    return NOTIFY_OK;
}

static struct notifier_block netdev_nb = {
    .notifier_call = netdev_notifier_callback,
    .priority = 0,
};

static int monitor_thread_fn(void *data)
{
    int i = 0;
    
    pr_info("%s: Network device monitor thread started\n", __func__);
    
    while (!kthread_should_stop() && i < 20) {
        pr_info("%s: Network stats - Total: %d, UP: %d, DOWN: %d\n", 
                __func__, netdev_events, up_events, down_events);
        
        if (i % 5 == 0) {
            pr_info("%s: Monitoring network interface events...\n", __func__);
        }
        
        msleep(3000);
        i++;
    }
    
    pr_info("%s: Network device monitor thread exiting\n", __func__);
    return 0;
}

static int __init netdev_notifier_init(void)
{
    int ret;
    
    pr_info("%s: Network device notifier module loaded\n", __func__);
    
    ret = register_netdevice_notifier(&netdev_nb);
    if (ret) {
        pr_err("%s: Failed to register netdev notifier: %d\n", __func__, ret);
        return ret;
    }
    
    monitor_thread = kthread_run(monitor_thread_fn, NULL, "netdev_monitor");
    if (IS_ERR(monitor_thread)) {
        pr_err("%s: Failed to create monitor thread\n", __func__);
        unregister_netdevice_notifier(&netdev_nb);
        return PTR_ERR(monitor_thread);
    }
    
    pr_info("%s: Network device notifier registered successfully\n", __func__);
    return 0;
}

static void __exit netdev_notifier_exit(void)
{
    if (monitor_thread)
        kthread_stop(monitor_thread);
        
    unregister_netdevice_notifier(&netdev_nb);
    
    pr_info("%s: Final network stats - Total: %d, UP: %d, DOWN: %d\n", 
            __func__, netdev_events, up_events, down_events);
    pr_info("%s: Network device notifier module unloaded\n", __func__);
}

module_init(netdev_notifier_init);
module_exit(netdev_notifier_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("Network device notifier demonstration");
MODULE_VERSION("1.0");
