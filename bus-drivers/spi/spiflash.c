/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/fs.h>
#include <linux/property.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include "spi.h"

struct W25_priv *obj = NULL;

/*
 * read - Read 256 bytes from W25Q32 flash (sysfs attribute)
 * 
 * Per W25Q32 datasheet section 9.2.8 "Read Data (03h)":
 * - Command format: [03h][A23-A16][A15-A8][A7-A0]
 * - Returns: Continuous data stream starting from addressed location
 * - No dummy bytes required for standard read at frequencies ≤50MHz
 * - Read can continue sequentially; address auto-increments internally
 * 
 * This function always returns 256 bytes (one page) starting from
 * the current offset set via /sys/spiflash/offset
 * 
 * SPI sequence:
 *   CS↓ → TX:[03h][Addr2][Addr1][Addr0] → RX:[Data×256] → CS↑
 */
static ssize_t read(struct kobject *kobj, struct kobj_attribute *attr,
		    char *buf)
{
	unsigned retval;
	u8 tx_buf[4];

	pr_info("w25q32_sysfs: Reading 256 bytes from offset 0x%06lx\n", obj->offset);

	/* Build Read Data command per datasheet:
	 * Byte 0: Read instruction (03h)
	 * Byte 1: Address[23:16] - MSB of 24-bit address
	 * Byte 2: Address[15:8]  - Middle byte of address
	 * Byte 3: Address[7:0]   - LSB of address
	 */
	tx_buf[0] = W25_READ;                /* 0x03 - Read Data instruction */
	tx_buf[1] = obj->offset >> 16;       /* Address byte 2 (A23-A16) */
	tx_buf[2] = obj->offset >> 8;        /* Address byte 1 (A15-A8) */
	tx_buf[3] = obj->offset;             /* Address byte 0 (A7-A0) */

	/* Execute SPI transaction:
	 * Send: 4-byte command (instruction + 3-byte address)
	 * Receive: 256 bytes of data (one page)
	 * Note: Read operations don't require WREN and have no wait time
	 */
	retval = spi_write_then_read(obj->spi, tx_buf, 4, buf, 256);
	if (retval)
		pr_err("w25q32_sysfs: SPI read transaction failed (error=%u)\n", retval);
	
	/* Always return 256 bytes to sysfs (one page) */
	return 256;
}

/*
 * write - Write data to W25Q32 flash (sysfs attribute)
 * 
 * Per W25Q32 datasheet section 9.2.23 "Page Program (02h)":
 * - Requires WREN (06h) command first to set Write Enable Latch
 * - Command format: [02h][A23-A16][A15-A8][A7-A0][Data...]
 * - Maximum 256 bytes per operation (one page)
 * - Must not cross page boundary (256-byte aligned)
 * - Write cycle time (tPP): 3ms typical, 5ms maximum
 * 
 * Page Program sequence per datasheet section 9.2.1:
 *   1. CS↓ → TX:[06h] → CS↑              (WREN - Write Enable)
 *   2. CS↓ → TX:[02h][Addr][Data] → CS↑  (Page Program)
 *   3. Wait tPP (5ms) for write completion
 * 
 * Note: Writing to non-erased locations (not FFh) may result in
 * incorrect data. Sectors must be erased before programming.
 */
static ssize_t write(struct kobject *kobj, struct kobj_attribute *attr,
		     const char *buf, size_t count)
{
	u8 cmd = W25_WREN, tx_buf[260] = { 0 };  /* Max: 4 (cmd+addr) + 256 (data) */
	unsigned retval, i, j;

	pr_info("w25q32_sysfs: Writing %zu bytes to offset 0x%06lx\n", count, obj->offset);

	/* Build Page Program command per datasheet:
	 * Byte 0: Page Program instruction (02h)
	 * Byte 1: Address[23:16] - MSB of 24-bit address
	 * Byte 2: Address[15:8]  - Middle byte of address
	 * Byte 3: Address[7:0]   - LSB of address
	 * Byte 4+: Data bytes to program (1 to 256 bytes)
	 */
	tx_buf[0] = W25_WRITE;               /* 0x02 - Page Program instruction */
	tx_buf[1] = obj->offset >> 16;       /* Address byte 2 (A23-A16) */
	tx_buf[2] = obj->offset >> 8;        /* Address byte 1 (A15-A8) */
	tx_buf[3] = obj->offset;             /* Address byte 0 (A7-A0) */

	/* Copy user data after command+address bytes */
	j = 4;
	for (i = 0; i < count; i++) {
		tx_buf[j] = buf[i];
		j++;
	}

	/* Step 1: Issue Write Enable (WREN) command
	 * Per datasheet: "Write Enable Latch (WEL) bit must be set prior
	 * to every Page Program instruction. The WREN instruction sets WEL."
	 */
	retval = spi_write(obj->spi, &cmd, 1);  /* Send 06h (WREN) */
	if (retval)
		pr_err("w25q32_sysfs: Write Enable (WREN) failed (error=%u)\n", retval);
	
	/* Step 2: Issue Page Program command with address and data
	 * Datasheet: "Page Program allows 1 to 256 bytes to be programmed
	 * at previously erased (FFh) memory locations."
	 */
	retval = spi_write(obj->spi, tx_buf, count + 4);  /* Send [02h][Addr][Data] */
	if (retval)
		pr_err("w25q32_sysfs: Page Program command failed (error=%u)\n", retval);
	
	/* Step 3: Wait for internal write cycle to complete
	 * Per datasheet Table 13: tPP (Page Program time) = 3ms typ, 5ms max
	 * Using fixed 5ms delay instead of polling Status Register (05h).
	 */
	msleep(5);  /* Wait for tPP (page program time) */
	
	return count;
}

static ssize_t get_offset(struct kobject *kobj, struct kobj_attribute *attr,
			  char *buf)
{
	return sprintf(buf, "%ld", obj->offset);
}

static ssize_t set_offset(struct kobject *kobj, struct kobj_attribute *attr,
			  const char *buf, size_t count)
{

	unsigned block, sect, page;

	sscanf(buf, "%u:%u:%u", &block, &sect, &page);

	if ((block > 64) || (sect > 16) || (page > 16))
		return -EAGAIN;

	obj->offset = ((block * 64 * 1024) + (sect * 4 * 1024) + (page * 256));
	return count;
}

static ssize_t erase(struct kobject *kobj, struct kobj_attribute *attr,
		     const char *buf, size_t count)
{

	unsigned blockno, sectno, offset, status;

	u8 cmd = W25_WREN, arr[4];

	sscanf(buf, "%u:%u", &blockno, &sectno);

	if ((blockno > 64) || (sectno > 16))
		return -EAGAIN;

	offset = ((blockno * 64 * 1024) + (sectno * 4 * 1024));
	/* write enable latch */
	status = spi_write(obj->spi, &cmd, 1);
	if (status)
		pr_info("spi_write got failed\n");
	arr[0] = W25_SEC_ERASE;
	arr[1] = offset >> 16;
	arr[2] = offset >> 8;
	arr[3] = offset;

	status = spi_write(obj->spi, arr, 4);
	if (status)
		pr_info("spi_write got failed\n");

	return count;
}

static struct kobj_attribute data_attribute =
__ATTR(w25q32, S_IRUGO | S_IWUSR, read, write);
static struct kobj_attribute offset_attribute =
__ATTR(offset, S_IRUGO | S_IWUSR, get_offset, set_offset);
static struct kobj_attribute erase_attribute =
__ATTR(erase, S_IRUGO | S_IWUSR, NULL, erase);

static struct attribute *attrs[] = {
	&data_attribute.attr,
	&offset_attribute.attr,
	&erase_attribute.attr,
	NULL,			/* need to NULL terminate the list of attributes */
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

static int spi_probe(struct spi_device *spi)
{
	int retval;

	obj = (struct W25_priv *)kmalloc(sizeof(struct W25_priv), GFP_KERNEL);
	if (obj == NULL) {
		pr_info("Requested memory not allocated\n");
		return -ENOMEM;
	}

	obj->spi = spi;
	obj->offset = 0;

	if (device_property_read_u32(&spi->dev, "w25,size", &obj->size) < 0) {
		dev_err(&spi->dev, "Error: missing 'w25,size' property\n");
		kfree(obj);
		return -ENODEV;
	}

	if (device_property_read_u32(&spi->dev, "w25,pagesize", &obj->page_size) < 0) {
		dev_err(&spi->dev, "Error: missing 'w25,pagesize' property\n");
		kfree(obj);
		return -ENODEV;
	}

	if (device_property_read_u32(&spi->dev, "w25,address-width", &obj->addr_width) < 0) {
		dev_err(&spi->dev, "Error: missing 'w25,address-width' property\n");
		kfree(obj);
		return -ENODEV;
	}

	obj->kobj = kobject_create_and_add("spiflash", NULL);
	if (!obj->kobj) {
		dev_err(&spi->dev, "Failed to create kobject\n");
		kfree(obj);
		return -ENOMEM;
	}

	retval = sysfs_create_group(obj->kobj, &attr_group);
	if (retval) {
		dev_err(&spi->dev, "Failed to create sysfs group\n");
		kobject_put(obj->kobj);
		kfree(obj);
		return retval;
	}

	dev_info(&spi->dev, "W25Q32 sysfs driver probed successfully\n");
	return 0;

}

static void spi_remove(struct spi_device *spi)
{
	if (obj) {
		sysfs_remove_group(obj->kobj, &attr_group);
		kobject_put(obj->kobj);
		kfree(obj);
		obj = NULL;
	}
	dev_info(&spi->dev, "W25Q32 sysfs driver removed\n");
}

static const struct spi_device_id w25q32_spi_id_table[] = {
	{"w25q32", 0},
	{},
};

MODULE_DEVICE_TABLE(spi, w25q32_spi_id_table);

static const struct of_device_id w25q32_of_match[] = {
    { .compatible = "techveda,w25q32" },
    { },
};
MODULE_DEVICE_TABLE(of, w25q32_of_match);


static struct spi_driver spi_driver = {
	.driver = {
		.name = "w25q32_sysfs",
		.owner = THIS_MODULE,
		.of_match_table = w25q32_of_match,
	},
	.probe = spi_probe,
	.remove = spi_remove,
	.id_table = w25q32_spi_id_table,
};

module_spi_driver(spi_driver);

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("W25Q32 SPI flash sysfs driver");
MODULE_LICENSE("Dual MIT/GPL");
