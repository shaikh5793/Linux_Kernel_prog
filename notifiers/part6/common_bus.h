#ifndef NOTIFIERS_PART6_COMMON_BUS_H
#define NOTIFIERS_PART6_COMMON_BUS_H

#include <linux/notifier.h>
#include <linux/device.h>

static inline const char *bus_action_str(unsigned long action)
{
    switch (action) {
    case BUS_NOTIFY_ADD_DEVICE:
        return "ADD_DEVICE";
    case BUS_NOTIFY_DEL_DEVICE:
        return "DEL_DEVICE";
    case BUS_NOTIFY_BOUND_DRIVER:
        return "BOUND_DRIVER";
    case BUS_NOTIFY_UNBOUND_DRIVER:
        return "UNBOUND_DRIVER";
    default:
        return "OTHER";
    }
}

#endif /* NOTIFIERS_PART6_COMMON_BUS_H */

