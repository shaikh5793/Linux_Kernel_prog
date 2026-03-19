// PCI bus notifier: logs add/remove/bind/unbind events on the PCI bus.

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/notifier.h>
#include <linux/device.h>
#include <linux/pci.h>

#include "common_bus.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("PCI bus notifier");
MODULE_VERSION("1.0");

/*
 * What triggers this notifier?
 * - Device add/remove and driver bind/unbind on the PCI bus.
 * - Examples:
 *   - Hotplug PCI device (where supported)
 *   - Bind/unbind drivers dynamically
 */

static int pci_events, pci_add, pci_del;

static int pci_bus_cb(struct notifier_block *nb, unsigned long action, void *data)
{
    struct device *dev = data;

    pci_events++;
    if (action == BUS_NOTIFY_ADD_DEVICE)
        pci_add++;
    else if (action == BUS_NOTIFY_DEL_DEVICE)
        pci_del++;

    pr_info("PCI BUS: %s: %s\n", bus_action_str(action), dev_name(dev));
    return NOTIFY_OK;
}

static struct notifier_block pci_nb = {
    .notifier_call = pci_bus_cb,
    .priority = 0,
};

static int __init pci_bus_notifier_init(void)
{
    int ret;
    pr_info("pci_bus_notifier: loading\n");
    ret = bus_register_notifier(&pci_bus_type, &pci_nb);
    if (ret) {
        pr_err("pci_bus_notifier: register failed: %d\n", ret);
        return ret;
    }
    pr_info("pci_bus_notifier: registered\n");
    return 0;
}

static void __exit pci_bus_notifier_exit(void)
{
    bus_unregister_notifier(&pci_bus_type, &pci_nb);
    pr_info("pci_bus_notifier: unloaded (events=%d add=%d del=%d)\n", pci_events, pci_add, pci_del);
}

module_init(pci_bus_notifier_init);
module_exit(pci_bus_notifier_exit);

