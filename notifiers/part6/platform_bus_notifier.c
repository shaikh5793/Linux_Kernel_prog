// Platform bus notifier: logs add/remove/bind/unbind events on the platform bus.

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/notifier.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#include "common_bus.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("Platform bus notifier");
MODULE_VERSION("1.0");

/*
 * What triggers this notifier?
 * - Platform device add/remove and driver bind/unbind (DT/ACPI/platform data).
 */

static int plat_events, plat_add, plat_del;

static int plat_bus_cb(struct notifier_block *nb, unsigned long action, void *data)
{
    struct device *dev = data;

    plat_events++;
    if (action == BUS_NOTIFY_ADD_DEVICE)
        plat_add++;
    else if (action == BUS_NOTIFY_DEL_DEVICE)
        plat_del++;

    pr_info("PLATFORM BUS: %s: %s\n", bus_action_str(action), dev_name(dev));
    return NOTIFY_OK;
}

static struct notifier_block plat_nb = {
    .notifier_call = plat_bus_cb,
    .priority = 0,
};

static int __init platform_bus_notifier_init(void)
{
    int ret;
    pr_info("platform_bus_notifier: loading\n");
    ret = bus_register_notifier(&platform_bus_type, &plat_nb);
    if (ret) {
        pr_err("platform_bus_notifier: register failed: %d\n", ret);
        return ret;
    }
    pr_info("platform_bus_notifier: registered\n");
    return 0;
}

static void __exit platform_bus_notifier_exit(void)
{
    bus_unregister_notifier(&platform_bus_type, &plat_nb);
    pr_info("platform_bus_notifier: unloaded (events=%d add=%d del=%d)\n", plat_events, plat_add, plat_del);
}

module_init(platform_bus_notifier_init);
module_exit(platform_bus_notifier_exit);

