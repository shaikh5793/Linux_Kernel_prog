/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/kobject.h>

struct eeprom_data {
    struct i2c_client *client;
    struct mutex lock;
    unsigned int page_offset;
    unsigned int size;
    unsigned int pagesize;
    unsigned int address_width;
    struct kobject *eeprom_kobj;
};


struct eeprom_data *data = NULL;

static ssize_t eeprom_page_read(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    int ret;
    char addr[2];

    
    unsigned int address = data->page_offset * data->pagesize;

    addr[0] = (address >> 8) & 0xFF;
    addr[1] = address & 0xFF;

    mutex_lock(&data->lock);
    ret = i2c_master_send(data->client, addr, 2);
    if (ret < 0) {
        mutex_unlock(&data->lock);
        return ret;
    }

    /* Read data immediately (no delay needed for read operations) */
    ret = i2c_master_recv(data->client, buf, data->pagesize);
    mutex_unlock(&data->lock);

    if (ret < 0)
        return ret;

    /* Ensure the correct number of bytes were read */
    if (ret != data->pagesize)
        return -EIO;

    return data->pagesize;
}

static ssize_t eeprom_page_write(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    int ret;
    char *buffer;

    if (count > data->pagesize)
        return -EINVAL;

    buffer = kmalloc(2 + count, GFP_KERNEL);
    if (!buffer)
        return -ENOMEM;

/* Calculate the EEPROM address based on the current page offset */
    unsigned int address = data->page_offset * data->pagesize;

    buffer[0] = (address >> 8) & 0xFF;
    buffer[1] = address & 0xFF;
    memcpy(buffer + 2, buf, count);

    mutex_lock(&data->lock);
    ret = i2c_master_send(data->client, buffer, 2 + count);
    mutex_unlock(&data->lock);

    kfree(buffer);

    if (ret < 0)
        return ret;

    msleep(5);  // Wait for write cycle to complete

    return count;
}

static ssize_t eeprom_page_offset_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "%u\n", data->page_offset);
}

static ssize_t eeprom_page_offset_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    unsigned int offset;

    if (kstrtouint(buf, 10, &offset))
        return -EINVAL;

    if (offset >= data->size / data->pagesize)
        return -EINVAL;

    data->page_offset = offset;

    return count;
}

static struct kobj_attribute at24c32_attr = __ATTR(at24c32, 0664, eeprom_page_read, eeprom_page_write);
static struct kobj_attribute page_offset_attr = __ATTR(page_offset, 0664, eeprom_page_offset_show, eeprom_page_offset_store);

static struct attribute *eeprom_attrs[] = {
    &at24c32_attr.attr,
    &page_offset_attr.attr,
    NULL,
};

static const struct attribute_group eeprom_attr_group = {
    .attrs = eeprom_attrs,
};
/*
 * step 1: Gather config Data
 * Step 2: Allocate/setup resources
 * Step 3: Setup driver ops and register with chosen kernel driver model
 *
 */

static int eeprom_probe(struct i2c_client *client)
{
    int ret;
    data = devm_kzalloc(&client->dev, sizeof(*data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    data->client = client;
    mutex_init(&data->lock);
    data->page_offset = 0;

    /* Read configuration from device properties */
    if (device_property_read_u32(&client->dev, "at24,size", &data->size)) {
        dev_err(&client->dev, "Failed to get EEPROM size from device properties\n");
        return -EINVAL;
    }
    if (device_property_read_u32(&client->dev, "at24,pagesize", &data->pagesize)) {
        dev_err(&client->dev, "Failed to get EEPROM page size from device properties\n");
        return -EINVAL;
    }
    if (device_property_read_u32(&client->dev, "at24,address-width", &data->address_width)) {
        dev_err(&client->dev, "Failed to get EEPROM address width from device properties\n");
        return -EINVAL;
    }

    i2c_set_clientdata(client, data);

    /* Create a unique sysfs directory in the root */
    data->eeprom_kobj = kobject_create_and_add("at24c32_eeprom", NULL /* kernel_kobj */ );
    if (data->eeprom_kobj == NULL) {
                pr_err("failed to create a object in sysfs : i2c\n");
                return -ENOMEM;
        }

    ret = sysfs_create_group(data->eeprom_kobj, &eeprom_attr_group);
    if (ret) {
        dev_err(&client->dev, "Failed to create sysfs group\n");
        kobject_put(data->eeprom_kobj);
        return ret;
    }

    dev_info(&client->dev, "EEPROM driver probed\n");
    return 0;
}

static void eeprom_remove(struct i2c_client *client)
{
    struct eeprom_data *data = i2c_get_clientdata(client);

    sysfs_remove_group(data->eeprom_kobj, &eeprom_attr_group);
    kobject_put(data->eeprom_kobj);
}

static const struct i2c_device_id eeprom_id[] = {
    { "at24", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, eeprom_id);

static const struct of_device_id eeprom_of_match[] = {
    { .compatible = "techveda,at24c32", },
    { }
};
MODULE_DEVICE_TABLE(of, eeprom_of_match);

static struct i2c_driver eeprom_driver = {
    .driver = {
        .name = "at24_sys_driver",
        .of_match_table = eeprom_of_match,
    },
    .probe = eeprom_probe,
    .remove = eeprom_remove,
    .id_table = eeprom_id,
};

module_i2c_driver(eeprom_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("Custom EEPROM driver for AT25C32 with sysfs interface");
