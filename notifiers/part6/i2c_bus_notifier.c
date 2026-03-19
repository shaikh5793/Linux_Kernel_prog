// I2C bus notifier: logs add/remove/bind/unbind events on the I2C bus.

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/notifier.h>
#include <linux/device.h>
#include <linux/i2c.h>

#include "common_bus.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("I2C bus notifier");
MODULE_VERSION("1.0");

/*
 * What triggers this notifier?
 * - I2C client add/remove and driver bind/unbind on the I2C bus.
 * - Examples:
 *   - Dynamic device creation/removal via board code/DT overlays
 *   - Driver probing/unbinding
 */

static int i2c_events, i2c_add, i2c_del;

static int i2c_bus_cb(struct notifier_block *nb, unsigned long action, void *data)
{
    struct device *dev = data;

    i2c_events++;
    if (action == BUS_NOTIFY_ADD_DEVICE)
        i2c_add++;
    else if (action == BUS_NOTIFY_DEL_DEVICE)
        i2c_del++;

    pr_info("I2C BUS: %s: %s\n", bus_action_str(action), dev_name(dev));
    return NOTIFY_OK;
}

static struct notifier_block i2c_nb = {
    .notifier_call = i2c_bus_cb,
    .priority = 0,
};

static int __init i2c_bus_notifier_init(void)
{
    int ret;
    pr_info("i2c_bus_notifier: loading\n");
    ret = bus_register_notifier(&i2c_bus_type, &i2c_nb);
    if (ret) {
        pr_err("i2c_bus_notifier: register failed: %d\n", ret);
        return ret;
    }
    pr_info("i2c_bus_notifier: registered\n");
    return 0;
}

static void __exit i2c_bus_notifier_exit(void)
{
    bus_unregister_notifier(&i2c_bus_type, &i2c_nb);
    pr_info("i2c_bus_notifier: unloaded (events=%d add=%d del=%d)\n", i2c_events, i2c_add, i2c_del);
}

module_init(i2c_bus_notifier_init);
module_exit(i2c_bus_notifier_exit);

