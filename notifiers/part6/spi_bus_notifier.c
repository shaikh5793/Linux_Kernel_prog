// SPI bus notifier: logs add/remove/bind/unbind events on the SPI bus.

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/notifier.h>
#include <linux/device.h>
#include <linux/spi/spi.h>

#include "common_bus.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("SPI bus notifier");
MODULE_VERSION("1.0");

/*
 * What triggers this notifier?
 * - SPI device add/remove and driver bind/unbind on the SPI bus.
 * - Examples: DT overlays or hotplug-capable controllers adding devices.
 */

static int spi_events, spi_add, spi_del;

static int spi_bus_cb(struct notifier_block *nb, unsigned long action, void *data)
{
    struct device *dev = data;

    spi_events++;
    if (action == BUS_NOTIFY_ADD_DEVICE)
        spi_add++;
    else if (action == BUS_NOTIFY_DEL_DEVICE)
        spi_del++;

    pr_info("SPI BUS: %s: %s\n", bus_action_str(action), dev_name(dev));
    return NOTIFY_OK;
}

static struct notifier_block spi_nb = {
    .notifier_call = spi_bus_cb,
    .priority = 0,
};

static int __init spi_bus_notifier_init(void)
{
    int ret;
    pr_info("spi_bus_notifier: loading\n");
    ret = bus_register_notifier(&spi_bus_type, &spi_nb);
    if (ret) {
        pr_err("spi_bus_notifier: register failed: %d\n", ret);
        return ret;
    }
    pr_info("spi_bus_notifier: registered\n");
    return 0;
}

static void __exit spi_bus_notifier_exit(void)
{
    bus_unregister_notifier(&spi_bus_type, &spi_nb);
    pr_info("spi_bus_notifier: unloaded (events=%d add=%d del=%d)\n", spi_events, spi_add, spi_del);
}

module_init(spi_bus_notifier_init);
module_exit(spi_bus_notifier_exit);

