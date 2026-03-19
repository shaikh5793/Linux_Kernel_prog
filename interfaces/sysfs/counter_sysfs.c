/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 *
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
 *
 * SysFS Interface Example - Counter Service
 * Purpose: Demonstrates user-kernel communication via /sys filesystem
 * Usage: Read/write operations through /sys/kernel/counter/value
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/string.h>

static int counter = 0;
static struct kobject *counter_kobj;

/*
 * value_show() - Display current counter value through sysfs
 * Called by: sysfs read operations when user reads from /sys/kernel/counter/value
 * Purpose: Formats counter value for display in sysfs attribute
 */
static ssize_t value_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", counter);
    dump_stack();
}

/*
 * value_store() - Handle write operations to update counter via sysfs
 * Called by: sysfs write operations when user writes to /sys/kernel/counter/value
 * Purpose: Parses user input to increment (1) or reset (0) counter
 */
static ssize_t value_store(struct kobject *kobj, struct kobj_attribute *attr,
                          const char *buf, size_t count)
{
    int value;
    int ret;

    ret = sscanf(buf, "%d", &value);
    if (ret != 1)
        return -EINVAL;

    if (value == 0)
        counter = 0;  /* reset */
    else if (value == 1)
        counter++;    /* increment */
    else
        return -EINVAL;
    dump_stack();
    return count;
}

static struct kobj_attribute value_attribute = __ATTR_RW(value);

static struct attribute *counter_attrs[] = {
    &value_attribute.attr,
    NULL,
};

static const struct attribute_group counter_attr_group = {
    .attrs = counter_attrs,
};

static int __init counter_init(void)
{
    int ret;

    counter_kobj = kobject_create_and_add("counter", kernel_kobj);
    if (!counter_kobj) {
        pr_err("Failed to create counter kobject\n");
        return -ENOMEM;
    }

    ret = sysfs_create_group(counter_kobj, &counter_attr_group);
    if (ret) {
        pr_err("Failed to create sysfs group\n");
        kobject_put(counter_kobj);
        return ret;
    }

    pr_info("Counter SysFS module loaded: /sys/kernel/counter/value\n");
    return 0;
}

static void __exit counter_exit(void)
{
    sysfs_remove_group(counter_kobj, &counter_attr_group);
    kobject_put(counter_kobj);
    pr_info("Counter SysFS module unloaded\n");
}

module_init(counter_init);
module_exit(counter_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("User-Kernel Interface Example");
MODULE_DESCRIPTION("Counter service via SysFS interface");
MODULE_VERSION("1.0");
