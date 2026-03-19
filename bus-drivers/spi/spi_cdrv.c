/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/spi/spi.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/ioctl.h>
#include <linux/property.h>
#include <linux/delay.h>

/* ANSI color codes for printk messages */
#define KRED   "\033[0;31m"
#define KGRN   "\033[0;32m"
#define KYEL   "\033[0;33m"
#define KBLU   "\033[0;34m"
#define KMAG   "\033[0;35m"
#define KCYN   "\033[0;36m"
#define KWHT   "\033[0;37m"
#define KNRM   "\033[0m"

/*
 * SPI flash commands for Winbond W25Q32 (per datasheet):
 *   - W25_READ      (0x03): Standard Read Data command.
 *   - W25_WRITE     (0x02): Page Program command.
 *   - W25_WREN      (0x06): Write Enable command.
 *   - W25_SEC_ERASE (0x20): 4KB Sector Erase command.
 */
#ifndef W25_READ
#define W25_READ      0x03
#endif

#ifndef W25_WRITE
#define W25_WRITE     0x02
#endif

#ifndef W25_WREN
#define W25_WREN      0x06
#endif

#ifndef W25_SEC_ERASE
#define W25_SEC_ERASE 0x20
#endif

/*
 * IOCTL definitions.
 *   - W25_IOC_SET_OFFSET: User passes a struct w25_offset (block, sector, page)
 *                         to set the flash offset.
 *   - W25_IOC_GET_OFFSET: Returns the current flash offset.
 *   - W25_IOC_ERASE:      User passes a struct w25_erase (block, sector) to trigger a sector erase.
 */
#define W25_IOC_MAGIC       'W'
#define W25_IOC_SET_OFFSET  _IOW(W25_IOC_MAGIC, 1, struct w25_offset)
#define W25_IOC_GET_OFFSET  _IOR(W25_IOC_MAGIC, 2, unsigned int)
#define W25_IOC_ERASE       _IOW(W25_IOC_MAGIC, 3, struct w25_erase)

struct w25_offset {
    unsigned int block;
    unsigned int sector;
    unsigned int page;
};

struct w25_erase {
    unsigned int block;
    unsigned int sector;
};

/*
 * Private device structure for the flash driver.
 */
struct w25_priv {
    struct spi_device *spi;
    unsigned int offset;      /* current flash offset (absolute address) */
    unsigned int size;        /* total flash size (in bytes) */
    unsigned int page_size;   /* flash page size */
    unsigned int addr_width;  /* address width in bytes */
    struct cdev cdev;
    dev_t devt;
    struct mutex lock;        /* serialize SPI operations */
};

static struct class *w25_class;  /* used for device_create() */

/*====================== File Operations ======================*/

/* open: assign our private structure to file->private_data */
static int w25_open(struct inode *inode, struct file *file)
{
    struct w25_priv *priv = container_of(inode->i_cdev, struct w25_priv, cdev);
    file->private_data = priv;
    pr_info(KCYN "w25q32: Device opened\n" KNRM);
    return 0;
}

static int w25_release(struct inode *inode, struct file *file)
{
    pr_info(KCYN "w25q32: Device closed\n" KNRM);
    return 0;
}

/*
 * w25_read - Read data from W25Q32 flash memory
 * 
 * Per W25Q32 datasheet section 9.2.8 "Read Data (03h)":
 * - Instruction: 0x03 (1 byte)
 * - Address: 24-bit (3 bytes), MSB first (A23-A16, A15-A8, A7-A0)
 * - Dummy bytes: None required
 * - Data: Continuous read from addressed location
 * - Max frequency: 50MHz (we use 50kHz for reliability)
 * 
 * SPI Transaction sequence:
 *   Master sends:  [03h][A23-A16][A15-A8][A7-A0]
 *   Slave returns: [D7-D0][D7-D0]...(continuous data)
 * 
 * Note: Read operations don't require WREN and have no wait time.
 */
static ssize_t w25_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    struct w25_priv *priv = file->private_data;
    int ret;
    u8 cmd_buf[4];
    u8 *kbuf;

    if (!count)
        return 0;

    /* Allocate kernel buffer for SPI data transfer */
    kbuf = kmalloc(count, GFP_KERNEL);
    if (!kbuf)
        return -ENOMEM;

    mutex_lock(&priv->lock);
    
    /* Build Read Data command per datasheet:
     * Byte 0: Read instruction (03h)
     * Byte 1: Address[23:16] - Most significant byte
     * Byte 2: Address[15:8]  - Middle byte
     * Byte 3: Address[7:0]   - Least significant byte
     */
    cmd_buf[0] = W25_READ;                        /* 0x03 - Read Data instruction */
    cmd_buf[1] = (priv->offset >> 16) & 0xff;     /* A23-A16: Address byte 2 */
    cmd_buf[2] = (priv->offset >> 8)  & 0xff;     /* A15-A8:  Address byte 1 */
    cmd_buf[3] = (priv->offset)       & 0xff;     /* A7-A0:   Address byte 0 */

    /* Execute SPI transaction:
     * 1. Send 4-byte command (instruction + 3-byte address)
     * 2. Receive 'count' bytes of data
     * No dummy bytes needed for standard read (03h)
     */
    ret = spi_write_then_read(priv->spi, cmd_buf, sizeof(cmd_buf), kbuf, count);
    if (ret < 0) {
        mutex_unlock(&priv->lock);
        kfree(kbuf);
        dev_err(&priv->spi->dev, KRED "spi_write_then_read failed\n" KNRM);
        return ret;
    }
    mutex_unlock(&priv->lock);

    /* Copy data from kernel space to user space */
    if (copy_to_user(buf, kbuf, count)) {
        kfree(kbuf);
        return -EFAULT;
    }
    kfree(kbuf);

    /* Update offset for next operation */
    priv->offset += count;
    pr_info(KGRN "w25q32: Read %zu bytes at offset 0x%06x\n" KNRM, count, priv->offset - count);
    return count;
}

/*
 * w25_write - Write data to W25Q32 flash memory (Page Program)
 * 
 * Per W25Q32 datasheet section 9.2.23 "Page Program (02h)":
 * - Instruction: 0x02 (1 byte)
 * - Address: 24-bit (3 bytes), MSB first (A23-A16, A15-A8, A7-A0)
 * - Data: 1 to 256 bytes (must not cross page boundary)
 * - Page size: 256 bytes (datasheet Table 2)
 * - Write cycle time (tPP): 3ms typical, 5ms max (datasheet Table 13)
 * 
 * Prerequisites (datasheet section 9.2.1):
 * - Write Enable Latch (WEL) must be set via WREN (06h) command
 * - WEL is automatically reset after Page Program completes
 * 
 * Page boundary constraint:
 * - If write crosses 256-byte boundary, only data up to boundary is written
 * - Remaining data wraps to start of same page (overwrites earlier data)
 * - Driver restricts writes to page_size to prevent this
 * 
 * SPI Transaction sequence:
 *   1. CS low, send WREN (06h), CS high              [Enable writing]
 *   2. CS low, send [02h][A23-A16][A15-A8][A7-A0][Data...], CS high  [Program page]
 *   3. Wait tPP (up to 5ms) for internal write cycle [Wait for completion]
 */
static ssize_t w25_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    struct w25_priv *priv = file->private_data;
    int ret;
    u8 wren = W25_WREN;
    u8 *tx_buf;

    if (!count)
        return 0;

    /* Enforce page size limit to prevent page boundary wrap
     * Per datasheet: "If more than 256 bytes are sent to the device,
     * previously latched data are discarded and the last 256 bytes
     * will be written to memory."
     */
    if (count > priv->page_size) {
        dev_err(&priv->spi->dev, KRED "Write size %zu exceeds page size (%u bytes)\n" KNRM, count, priv->page_size);
        return -EINVAL;
    }

    /* Allocate buffer: 4 bytes (cmd+addr) + data bytes */
    tx_buf = kmalloc(count + 4, GFP_KERNEL);
    if (!tx_buf)
        return -ENOMEM;

    /* Build Page Program command per datasheet:
     * Byte 0: Page Program instruction (02h)
     * Byte 1: Address[23:16] - Most significant byte
     * Byte 2: Address[15:8]  - Middle byte
     * Byte 3: Address[7:0]   - Least significant byte
     * Byte 4+: Data bytes to program
     */
    tx_buf[0] = W25_WRITE;                        /* 0x02 - Page Program instruction */
    tx_buf[1] = (priv->offset >> 16) & 0xff;      /* A23-A16: Address byte 2 */
    tx_buf[2] = (priv->offset >> 8)  & 0xff;      /* A15-A8:  Address byte 1 */
    tx_buf[3] = (priv->offset)       & 0xff;      /* A7-A0:   Address byte 0 */
    
    /* Copy user data into transmit buffer after command+address */
    if (copy_from_user(tx_buf + 4, buf, count)) {
        kfree(tx_buf);
        return -EFAULT;
    }

    mutex_lock(&priv->lock);
    
    /* Step 1: Issue Write Enable (WREN) command
     * Per datasheet section 9.2.1: "The Write Enable (WREN) instruction
     * sets the Write Enable Latch (WEL) bit. The WEL bit must be set
     * prior to every Page Program, Sector Erase, Block Erase, Chip Erase,
     * Write Status Register and Erase/Program Security Registers instruction."
     */
    ret = spi_write(priv->spi, &wren, 1);         /* Send 06h (WREN) */
    if (ret < 0) {
        mutex_unlock(&priv->lock);
        kfree(tx_buf);
        dev_err(&priv->spi->dev, KRED "spi_write (WREN) failed\n" KNRM);
        return ret;
    }
    
    /* Step 2: Issue Page Program command with address and data
     * Datasheet: "The Page Program instruction allows from one byte to
     * 256 bytes (a page) of data to be programmed at previously erased
     * (FFh) memory locations."
     */
    ret = spi_write(priv->spi, tx_buf, count + 4); /* Send [02h][Addr][Data] */
    if (ret < 0) {
        mutex_unlock(&priv->lock);
        kfree(tx_buf);
        dev_err(&priv->spi->dev, KRED "spi_write (Page Program) failed\n" KNRM);
        return ret;
    }
    
    /* Step 3: Wait for internal write cycle to complete
     * Per datasheet Table 13: tPP (Page Program time) = 3ms typ, 5ms max
     * During this time, the device is busy and Status Register can be
     * polled via RDSR (05h) to check BUSY bit.
     * Using fixed 5ms delay for simplicity instead of status polling.
     */
    msleep(5);  /* Wait for tPP (page program time) */
    
    mutex_unlock(&priv->lock);
    kfree(tx_buf);

    /* Update offset for next operation */
    priv->offset += count;
    pr_info(KGRN "w25q32: Wrote %zu bytes at offset 0x%06x\n" KNRM, count, priv->offset - count);
    return count;
}

/*
 * w25_ioctl - Handles custom commands:
 *   - W25_IOC_SET_OFFSET: Sets the flash offset using a struct w25_offset.
 *   - W25_IOC_GET_OFFSET: Returns the current flash offset.
 *   - W25_IOC_ERASE:      Erases a 4KB sector using a struct w25_erase.
 */
static long w25_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct w25_priv *priv = file->private_data;
    int ret = 0;
    struct w25_offset user_off;
    struct w25_erase user_erase;
    u8 cmd_buf[4];
    u8 wren;

    switch (cmd) {
    case W25_IOC_SET_OFFSET:
        if (copy_from_user(&user_off, (void __user *)arg, sizeof(user_off)))
            return -EFAULT;
        if (user_off.block > 64 || user_off.sector > 16 || user_off.page > 16)
            return -EINVAL;
        priv->offset = (user_off.block * 64 * 1024) +
                       (user_off.sector * 4 * 1024) +
                       (user_off.page * 256);
        pr_info(KBLU "w25q32: Offset set to 0x%06x (Block: %u, Sector: %u, Page: %u)\n" KNRM,
                priv->offset, user_off.block, user_off.sector, user_off.page);
        break;

    case W25_IOC_GET_OFFSET:
        if (copy_to_user((void __user *)arg, &priv->offset, sizeof(priv->offset)))
            return -EFAULT;
        pr_info(KBLU "w25q32: Current offset is 0x%06x\n" KNRM, priv->offset);
        break;

    case W25_IOC_ERASE:
        if (copy_from_user(&user_erase, (void __user *)arg, sizeof(user_erase)))
            return -EFAULT;
        if (user_erase.block > 64 || user_erase.sector > 16)
            return -EINVAL;
        {
            unsigned int erase_offset = (user_erase.block * 64 * 1024) +
                                        (user_erase.sector * 4 * 1024);
            mutex_lock(&priv->lock);
            wren = W25_WREN;
            ret = spi_write(priv->spi, &wren, 1);
            if (ret < 0) {
                mutex_unlock(&priv->lock);
                dev_err(&priv->spi->dev, KRED "spi_write (WREN) failed in erase\n" KNRM);
                return ret;
            }
            cmd_buf[0] = W25_SEC_ERASE;
            cmd_buf[1] = (erase_offset >> 16) & 0xff;
            cmd_buf[2] = (erase_offset >> 8)  & 0xff;
            cmd_buf[3] = (erase_offset)       & 0xff;
            ret = spi_write(priv->spi, cmd_buf, sizeof(cmd_buf));
            mutex_unlock(&priv->lock);
            if (ret < 0) {
                dev_err(&priv->spi->dev, KRED "spi_write (Sector Erase) failed\n" KNRM);
                return ret;
            }
            pr_info(KYEL "w25q32: Erased sector at offset 0x%06x (Block: %u, Sector: %u)\n" KNRM,
                    erase_offset, user_erase.block, user_erase.sector);
        }
        break;

    default:
        return -ENOTTY;
    }

    return ret;
}

static const struct file_operations w25_fops = {
    .owner          = THIS_MODULE,
    .open           = w25_open,
    .release        = w25_release,
    .read           = w25_read,
    .write          = w25_write,
    .unlocked_ioctl = w25_ioctl,
};

/*=================== SPI Driver Registration ==================*/

/*
 * Probe: Allocates and initializes our private data, reads device properties,
 * and registers the char device.
 */
static int w25_spi_probe(struct spi_device *spi)
{
    int ret;
    struct w25_priv *priv;

    priv = devm_kzalloc(&spi->dev, sizeof(*priv), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;

    priv->spi = spi;
    mutex_init(&priv->lock);

    ret = device_property_read_u32(&spi->dev, "w25,size", &priv->size);
    if (ret < 0) {
        dev_err(&spi->dev, KRED "Missing property 'w25,size'\n" KNRM);
        return -ENODEV;
    }

    ret = device_property_read_u32(&spi->dev, "w25,pagesize", &priv->page_size);
    if (ret < 0) {
        dev_err(&spi->dev, KRED "Missing property 'w25,pagesize'\n" KNRM);
        return -ENODEV;
    }

    ret = device_property_read_u32(&spi->dev, "w25,address-width", &priv->addr_width);
    if (ret < 0) {
        dev_err(&spi->dev, KRED "Missing property 'w25,address-width'\n" KNRM);
        return -ENODEV;
    }

    priv->offset = 0;

    ret = alloc_chrdev_region(&priv->devt, 0, 1, "w25q32");
    if (ret < 0) {
        dev_err(&spi->dev, KRED "Failed to allocate char dev region\n" KNRM);
        return ret;
    }

    cdev_init(&priv->cdev, &w25_fops);
    priv->cdev.owner = THIS_MODULE;
    ret = cdev_add(&priv->cdev, priv->devt, 1);
    if (ret < 0) {
        dev_err(&spi->dev, KRED "Failed to add cdev\n" KNRM);
        unregister_chrdev_region(priv->devt, 1);
        return ret;
    }

    if (!w25_class) {
        w25_class = class_create("w25q32");
        if (IS_ERR(w25_class)) {
            ret = PTR_ERR(w25_class);
            cdev_del(&priv->cdev);
            unregister_chrdev_region(priv->devt, 1);
            return ret;
        }
    }

    device_create(w25_class, &spi->dev, priv->devt, NULL, "w25q32");
    spi_set_drvdata(spi, priv);
    dev_info(&spi->dev, KGRN "w25q32 SPI flash char driver probed\n" KNRM);
    return 0;
}

/*
 * Remove: Cleans up the char device and allocated resources.
 */
static void w25_spi_remove(struct spi_device *spi)
{
    struct w25_priv *priv = spi_get_drvdata(spi);

    device_destroy(w25_class, priv->devt);
    cdev_del(&priv->cdev);
    unregister_chrdev_region(priv->devt, 1);
    dev_info(&spi->dev, KRED "w25q32 SPI flash char driver removed\n" KNRM);
}

static const struct spi_device_id w25_spi_id_table[] = {
    { "w25q32", 0 },
    { }
};
MODULE_DEVICE_TABLE(spi, w25_spi_id_table);

static const struct of_device_id w25_of_match[] = {
    { .compatible = "techveda,w25q32" },
    { },
};
MODULE_DEVICE_TABLE(of, w25_of_match);

static struct spi_driver w25_spi_driver = {
    .driver = {
        .name           = "w25q32_spi",
        .of_match_table = w25_of_match,
    },
    .probe  = w25_spi_probe,
    .remove = w25_spi_remove,
    .id_table = w25_spi_id_table,
};

module_spi_driver(w25_spi_driver);

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("SPI flash char driver for W25Q32 with read/write/ioctl interface and color coded printk messages");
MODULE_LICENSE("Dual MIT/GPL");

