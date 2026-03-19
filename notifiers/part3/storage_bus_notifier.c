/*
 * storage_bus_notifier.c - Demonstrates storage-related device events
 *
 * Monitors USB bus for device add/remove and highlights Mass Storage class.
 * This uses the generic device model bus notifier, which is stable across
 * modern kernels, and avoids deprecated block notifiers.
 *
 * Build: part of notifiers/part3
 * Test:  - Plug/unplug USB storage devices
 *        - Or simulate add/remove of any USB device
 *
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * License: Dual MIT/GPL v2
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/notifier.h>
#include <linux/device.h>
#include <linux/usb.h>

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("USB bus notifier for storage-related events");
MODULE_VERSION("1.0");

/*
 * What triggers this notifier?
 * - USB device add/remove on the USB bus.
 * - We highlight USB Mass Storage class (bInterfaceClass = 0x08), which is
 *   the common case for block devices (thumb drives, external disks).
 *
 * Example actions that generate events:
 * - Plug/unplug a USB storage device
 * - Load/unload USB gadget/device emulation in a VM
 */

static int usb_bus_events;
static int usb_add_events;
static int usb_remove_events;

static const char *dev_action_str(unsigned long action)
{
    switch (action) {
    case USB_DEVICE_ADD:
        return "ADD_DEVICE";
    case USB_DEVICE_REMOVE:
        return "DEL_DEVICE";
    /* Some kernels may not provide bind/unbind notifications here */
    default:
        return "OTHER";
    }
}

static bool is_mass_storage(struct device *dev)
{
    struct usb_device *udev;
    struct usb_interface *intf;

    if (!dev)
        return false;

    /* Try to interpret the device as an interface first */
    intf = to_usb_interface(dev);
    if (intf && intf->cur_altsetting) {
        const struct usb_host_interface *alts = intf->cur_altsetting;
        return alts->desc.bInterfaceClass == USB_CLASS_MASS_STORAGE;
    }

    /* Or as a raw USB device */
    udev = to_usb_device(dev);
    if (udev) {
        /* Some devices expose Mass Storage only at interface level;
         * device-level class may be 0 (per-interface). */
        return udev->descriptor.bDeviceClass == USB_CLASS_MASS_STORAGE;
    }

    return false;
}

static int usb_bus_notifier_cb(struct notifier_block *nb,
                               unsigned long action, void *data)
{
    struct device *dev = data;
    const char *act = dev_action_str(action);

    usb_bus_events++;

    switch (action) {
    case USB_DEVICE_ADD:
        usb_add_events++;
        pr_info("USB BUS: %s: %s%s\n",
                act,
                dev_name(dev),
                is_mass_storage(dev) ? " (Mass Storage)" : "");
        break;
    case USB_DEVICE_REMOVE:
        usb_remove_events++;
        pr_info("USB BUS: %s: %s%s\n",
                act,
                dev_name(dev),
                is_mass_storage(dev) ? " (Mass Storage)" : "");
        break;
    default:
        pr_info("USB BUS: %s: %s\n", act, dev_name(dev));
        break;
    }

    return NOTIFY_OK;
}

static struct notifier_block usb_bus_nb = {
    .notifier_call = usb_bus_notifier_cb,
    .priority = 0,
};

static int __init storage_bus_notifier_init(void)
{
    pr_info("Storage (USB bus) notifier loaded\n");
    /* Register for USB core notifications (void on this kernel) */
    usb_register_notify(&usb_bus_nb);
    pr_info("Registered USB bus notifier. Plug/unplug USB devices to see events.\n");
    return 0;
}

static void __exit storage_bus_notifier_exit(void)
{
    usb_unregister_notify(&usb_bus_nb);
    pr_info("Storage (USB bus) notifier unloaded. Events: total=%d add=%d del=%d\n",
            usb_bus_events, usb_add_events, usb_remove_events);
}

module_init(storage_bus_notifier_init);
module_exit(storage_bus_notifier_exit);
