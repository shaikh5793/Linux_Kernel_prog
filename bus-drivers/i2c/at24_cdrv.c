/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/ioctl.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include "at24_ioctl.h"

struct eeprom_data {
    struct i2c_client *client;
    struct mutex lock;
    unsigned int page_offset;
    unsigned int size;
    unsigned int pagesize;
    unsigned int address_width;
    struct cdev cdev;
    dev_t devt;
};

static struct class *eeprom_class;

/*
 * eeprom_read - Read data from AT24C32 EEPROM
 * @file: File structure pointer
 * @buf: User-space buffer to copy data to
 * @count: Number of bytes to read
 * @ppos: Current file position (EEPROM address)
 *
 * This function reads data from the EEPROM starting at the current file position.
 * It uses the I2C protocol for AT24C32:
 *   1. Send START + device address (write mode) + memory address (2 bytes)
 *   2. Send REPEATED START + device address (read mode)
 *   3. Read data bytes
 *   4. Send STOP
 *
 * The i2c_master_send/recv functions handle the I2C protocol details automatically.
 *
 * Returns: Number of bytes read on success, negative error code on failure
 */
static ssize_t eeprom_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    struct eeprom_data *data = file->private_data;
    char *kbuf;
    int ret;

    /* Boundary check: don't read past end of EEPROM */
    if (*ppos + count > data->size)
        count = data->size - *ppos;

    /* Nothing to read if at or past end of EEPROM */
    if (count == 0)
        return 0;

    /* Allocate kernel buffer to hold data read from EEPROM */
    kbuf = kmalloc(count, GFP_KERNEL);
    if (!kbuf)
        return -ENOMEM;

    /* Lock to prevent concurrent I2C access */
    mutex_lock(&data->lock);

    /*
     * AT24C32 Random Read Protocol:
     * Step 1: Write the 16-bit memory address
     *   - addr[0] = high byte of address (bits 11-8 for AT24C32)
     *   - addr[1] = low byte of address (bits 7-0)
     */
    char addr[2];
    addr[0] = (*ppos >> 8) & 0xFF;  /* High byte */
    addr[1] = *ppos & 0xFF;         /* Low byte */

    /* Send address to EEPROM (START + device addr + memory addr) */
    ret = i2c_master_send(data->client, addr, 2);
    if (ret < 0) {
        mutex_unlock(&data->lock);
        kfree(kbuf);
        return ret;
    }

    /*
     * Step 2: Read data from EEPROM
     * i2c_master_recv performs: REPEATED START + device addr(R) + read data + STOP
     * No delay needed - AT24C32 read access is immediate
     */
    ret = i2c_master_recv(data->client, kbuf, count);
    mutex_unlock(&data->lock);

    if (ret < 0) {
        kfree(kbuf);
        return ret;
    }

    /* Copy data from kernel buffer to user-space buffer */
    if (copy_to_user(buf, kbuf, count)) {
        kfree(kbuf);
        return -EFAULT;
    }

    kfree(kbuf);

    /* Update file position for next read/write operation */
    *ppos += count;

    return count;
}

/*
 * eeprom_write - Write data to AT24C32 EEPROM
 * @file: File structure pointer
 * @buf: User-space buffer containing data to write
 * @count: Number of bytes to write
 * @ppos: Current file position (EEPROM address)
 *
 * This function writes data to the EEPROM starting at the current file position.
 * 
 * CRITICAL: AT24C32 Page Write Constraint
 * ========================================
 * AT24C32 has 128 pages of 32 bytes each. Page writes CANNOT cross page boundaries.
 * If a write crosses a page boundary, the address wraps to the start of the SAME page,
 * causing data corruption.
 *
 * Example (32-byte pages):
 *   Writing 40 bytes starting at address 20:
 *   - Without page boundary handling:
 *     Bytes 0-11  -> addresses 20-31 (page 0) ✓
 *     Bytes 12-39 -> addresses 0-27  (wraps to start of page 0) ✗ CORRUPTS DATA!
 *   
 *   - With page boundary handling (this implementation):
 *     Chunk 1: 12 bytes -> addresses 20-31 (completes page 0) ✓
 *     Chunk 2: 28 bytes -> addresses 32-59 (writes to page 1) ✓
 *
 * AT24C32 Page Write Protocol:
 *   1. Send START + device address (write mode)
 *   2. Send 16-bit memory address
 *   3. Send data bytes (up to page size, within same page)
 *   4. Send STOP
 *   5. Wait 5ms for internal write cycle to complete
 *
 * Returns: Number of bytes written on success, negative error code on failure
 */
static ssize_t eeprom_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    struct eeprom_data *data = file->private_data;
    char *kbuf;
    char *buffer;
    int ret;
    size_t written = 0;        /* Tracks total bytes written so far */
    unsigned int eeprom_addr;  /* Absolute EEPROM address for current write */

    /* Boundary check: can't write past end of EEPROM */
    if (*ppos >= data->size)
        return -ENOSPC;  /* No space available */
    
    /* Truncate write if it would exceed EEPROM size */
    if (*ppos + count > data->size)
        count = data->size - *ppos;

    if (count == 0)
        return 0;

    /* Allocate kernel buffer and copy data from user-space */
    kbuf = kmalloc(count, GFP_KERNEL);
    if (!kbuf)
        return -ENOMEM;

    if (copy_from_user(kbuf, buf, count)) {
        kfree(kbuf);
        return -EFAULT;
    }

    /*
     * Allocate I2C write buffer: 2 bytes for address + max page size for data
     * Format: [addr_high][addr_low][data...]
     */
    buffer = kmalloc(data->pagesize + 2, GFP_KERNEL);
    if (!buffer) {
        kfree(kbuf);
        return -ENOMEM;
    }

    /*
     * Write data in chunks, ensuring no chunk crosses a page boundary
     * Loop continues until all data is written
     */
    while (count > 0) {
        /* Calculate absolute EEPROM address for this chunk */
        eeprom_addr = *ppos + written;
        
        /*
         * Page boundary calculation:
         * - page_offset: Position within current page (0 to pagesize-1)
         * - bytes_in_page: Remaining bytes in current page
         * - write_size: Bytes to write in this iteration (limited by page boundary)
         *
         * Example: pagesize=32, eeprom_addr=20
         *   page_offset = 20 % 32 = 20
         *   bytes_in_page = 32 - 20 = 12
         *   Can write max 12 bytes before hitting page boundary
         */
        unsigned int page_offset = eeprom_addr % data->pagesize;
        unsigned int bytes_in_page = data->pagesize - page_offset;
        size_t write_size = min(count, bytes_in_page);

        /*
         * Prepare I2C write buffer:
         * buffer[0] = high byte of 16-bit address
         * buffer[1] = low byte of 16-bit address
         * buffer[2..] = data to write
         */
        buffer[0] = (eeprom_addr >> 8) & 0xFF;  /* Address bits 15-8 */
        buffer[1] = eeprom_addr & 0xFF;         /* Address bits 7-0 */
        memcpy(buffer + 2, kbuf + written, write_size);

        /* Send write command to EEPROM (address + data in single transaction) */
        mutex_lock(&data->lock);
        ret = i2c_master_send(data->client, buffer, write_size + 2);
        mutex_unlock(&data->lock);

        if (ret < 0) {
            kfree(kbuf);
            kfree(buffer);
            /* Return bytes written so far, or error if nothing written */
            return written > 0 ? written : ret;
        }

        /* Update counters for next iteration */
        written += write_size;
        count -= write_size;
        
        /*
         * CRITICAL: Wait for EEPROM internal write cycle
         * AT24C32 requires 5ms (typical) to 10ms (max) for page write
         * Cannot perform any I2C operations during this time
         */
        msleep(5);
    }

    /* Cleanup allocated buffers */
    kfree(kbuf);
    kfree(buffer);
    
    /* Update file position for next read/write operation */
    *ppos += written;
    
    return written;
}

static long eeprom_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct eeprom_data *data = file->private_data;
    int ret = 0;
    char *kbuf;
    unsigned int address;

    switch (cmd) {
    case EEPROM_IOCTL_SET_PAGE_OFFSET:
        if (copy_from_user(&data->page_offset, (unsigned int __user *)arg, sizeof(data->page_offset)))
            return -EFAULT;
        break;

    case EEPROM_IOCTL_PAGE_READ:
        kbuf = kmalloc(data->pagesize, GFP_KERNEL);
        if (!kbuf)
            return -ENOMEM;

        address = data->page_offset * data->pagesize;
        {
            char addr_buf[2];
            addr_buf[0] = (address >> 8) & 0xFF;
            addr_buf[1] = address & 0xFF;

            mutex_lock(&data->lock);
            ret = i2c_master_send(data->client, addr_buf, 2);
            if (ret < 0) {
                mutex_unlock(&data->lock);
                kfree(kbuf);
                return ret;
            }

            ret = i2c_master_recv(data->client, kbuf, data->pagesize);
            mutex_unlock(&data->lock);

            if (ret < 0) {
                kfree(kbuf);
                return ret;
            }

            if (copy_to_user((char __user *)arg, kbuf, data->pagesize)) {
                kfree(kbuf);
                return -EFAULT;
            }
        }
        kfree(kbuf);
        break;

    case EEPROM_IOCTL_PAGE_WRITE:
        kbuf = kmalloc(data->pagesize, GFP_KERNEL);
        if (!kbuf)
            return -ENOMEM;

        if (copy_from_user(kbuf, (char __user *)arg, data->pagesize)) {
            kfree(kbuf);
            return -EFAULT;
        }

        address = data->page_offset * data->pagesize;
        {
            char *buffer = kmalloc(data->pagesize + 2, GFP_KERNEL);
            if (!buffer) {
                kfree(kbuf);
                return -ENOMEM;
            }

            buffer[0] = (address >> 8) & 0xFF;
            buffer[1] = address & 0xFF;
            memcpy(buffer + 2, kbuf, data->pagesize);

            mutex_lock(&data->lock);
            ret = i2c_master_send(data->client, buffer, data->pagesize + 2);
            mutex_unlock(&data->lock);

            kfree(buffer);

            if (ret < 0) {
                kfree(kbuf);
                return ret;
            }

            msleep(5);  // Wait for write cycle to complete
        }
        kfree(kbuf);
        break;

    default:
        return -ENOTTY;
    }

    return ret;
}

static int eeprom_open(struct inode *inode, struct file *file)
{
    struct eeprom_data *data = container_of(inode->i_cdev, struct eeprom_data, cdev);
    file->private_data = data;
    return 0;
}

static int eeprom_release(struct inode *inode, struct file *file)
{
    return 0;
}

static const struct file_operations eeprom_fops = {
    .owner = THIS_MODULE,
    .read = eeprom_read,
    .write = eeprom_write,
    .unlocked_ioctl = eeprom_ioctl,
    .open = eeprom_open,
    .release = eeprom_release,
};

/* Probe and remove functions 
1. Gather config data
2. setup resources
3. register driver with upper-layer as per applicable driver model
*/
static int eeprom_probe(struct i2c_client *client)
{
    struct eeprom_data *data;
    int ret;

    data = devm_kzalloc(&client->dev, sizeof(*data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    data->client = client;
    mutex_init(&data->lock);

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

    /* Initialize and add cdev */
    cdev_init(&data->cdev, &eeprom_fops);
    data->cdev.owner = THIS_MODULE;
    ret = alloc_chrdev_region(&data->devt, 0, 1, "eeprom");
    if (ret < 0) {
        dev_err(&client->dev, "Failed to allocate chrdev region\n");
        return ret;
    }

    ret = cdev_add(&data->cdev, data->devt, 1);
    if (ret < 0) {
        dev_err(&client->dev, "Failed to add cdev\n");
        unregister_chrdev_region(data->devt, 1);
        return ret;
    }

    /* Create device node */
    eeprom_class = class_create("eeprom");
    if (IS_ERR(eeprom_class)) {
        ret = PTR_ERR(eeprom_class);
        cdev_del(&data->cdev);
        unregister_chrdev_region(data->devt, 1);
        return ret;
    }

    device_create(eeprom_class, NULL, data->devt, NULL, "eeprom%d", MINOR(data->devt));

    i2c_set_clientdata(client, data);
    dev_info(&client->dev, "EEPROM driver probed\n");
    return 0;
}

static void eeprom_remove(struct i2c_client *client)
{
    struct eeprom_data *data = i2c_get_clientdata(client);

    device_destroy(eeprom_class, data->devt);
    class_destroy(eeprom_class);
    cdev_del(&data->cdev);
    unregister_chrdev_region(data->devt, 1);
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
        .name = "at24_cdrv_driver",
        .of_match_table = eeprom_of_match,
    },
    .probe = eeprom_probe,
    .remove = eeprom_remove,
    .id_table = eeprom_id,
};

module_i2c_driver(eeprom_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("Custom EEPROM driver for AT25C32");
