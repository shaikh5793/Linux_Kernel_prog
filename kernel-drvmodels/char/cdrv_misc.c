/*
 * Copyright (c) 2024 Techveda
 * Author: Raghu Bharadwaj
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>

#define DRV_NAME "cdrv_misc"
#define COUNT 1
#define SUCCESS 0

struct device *dev;

static int cdrv_open(struct inode *inode, struct file *file)
{
	dev_dbg(dev, "%s() invoked\n",__func__);
	return -EINVAL;
}

static int cdrv_release(struct inode *inode, struct file *file)
{
	dev_dbg(dev, "%s() invoked \n", __func__);
	return SUCCESS;
}

static ssize_t cdrv_write(struct file *file, const char __user
			  * buf, size_t lbuf, loff_t * offset)
{
	dev_dbg(dev, "%s() invoked:Rec'vd data of len = %ld\n", __func__,
		lbuf);
	return lbuf;
}

static ssize_t cdrv_read(struct file *file, char __user * buf,
			 size_t count, loff_t * off)
{
	dev_dbg(dev, "%s() invoked.\n"
		"request to read %zu bytes\n", __func__, count);
	return count;
}

static struct file_operations cdrv_fops = {
	.owner = THIS_MODULE,
	.write = cdrv_write,
	.read = cdrv_read,
	.open = cdrv_open,
	.release = cdrv_release
};

static struct miscdevice cDrvMisc = {
        .minor = MISC_DYNAMIC_MINOR,    /* allocate misc minor */
        .name = DRV_NAME,
        .fops = &cdrv_fops,         /* drivers fops instance */
};


static __init int cdrv_probe(void)
{
	int ret;

	ret = misc_register(&cDrvMisc);
        if (ret < 0) {
        	dev_err(dev, "%s:failed registering with minor %d",__func__, cDrvMisc.minor);
        	return ret;
        }
	dev = cDrvMisc.this_device;
	dev_dbg(dev, "Driver Registered %s with Minor %d\n", DRV_NAME,cDrvMisc.minor);
	return SUCCESS;
}

static __exit void cdrv_remove(void)
{
	dev_dbg(dev, "%s: Device detached\n", __func__);
	misc_deregister(&cDrvMisc);
}
module_init(cdrv_probe);
module_exit(cdrv_remove);

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("char driver skeleton");
MODULE_LICENSE("Dual MIT/GPL");
