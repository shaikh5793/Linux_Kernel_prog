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
#include <linux/cdev.h>

#define DRV_NAME "cdrv_udev"
#define COUNT 1
#define SUCCESS 0

static struct cdrv_prv {
	struct cdev *veda_cdev;
	dev_t mydev;
	struct device *dev;
	struct class *vclass;
}ctx;

static int cdrv_open(struct inode *inode, struct file *file)
{
	dev_info(ctx.dev, "%s() invoked\n",__func__);
	return SUCCESS;
}

static int cdrv_release(struct inode *inode, struct file *file)
{
	dev_info(ctx.dev, "%s() invoked \n", __func__);
	return SUCCESS;
}

static ssize_t cdrv_write(struct file *file, const char __user
			  * buf, size_t lbuf, loff_t * offset)
{
	dev_info(ctx.dev, "%s() invoked:Rec'vd data of len = %ld\n", __func__,
		lbuf);
	return lbuf;
}

static ssize_t cdrv_read(struct file *file, char __user * buf,
			 size_t count, loff_t * off)
{
	dev_info(ctx.dev, "%s() invoked.\n"
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

static __init int cdrv_probe(void)
{
	int ret;
	int baseminor = 0;
	alloc_chrdev_region(&(ctx.mydev), baseminor, COUNT, DRV_NAME);

	/* Allocate cdev instance */
	ctx.veda_cdev = cdev_alloc();

	/* initialize cdev with fops object */
	cdev_init(ctx.veda_cdev, &cdrv_fops);

	/* register cdev */
	ret = cdev_add(ctx.veda_cdev, ctx.mydev, COUNT);
	if (ret < 0) {
		pr_err("Error registering device driver");
		return ret;
	}

  ctx.vclass = class_create("vDev");
    	if(!ctx.vclass){
        	unregister_chrdev_region(ctx.mydev, COUNT);
        	return PTR_ERR(ctx.vclass);
    	}

  ctx.dev = device_create(ctx.vclass, NULL, ctx.mydev, NULL, "vDevnode");
      if(!ctx.dev){
       		class_destroy(ctx.vclass);
       		unregister_chrdev_region(ctx.mydev, COUNT);
       		return PTR_ERR(ctx.dev);
    }
	dev_info(ctx.dev,"Driver Registered %s\n", DRV_NAME);
	return SUCCESS;
}

static __exit void cdrv_remove(void)
{
	dev_info(ctx.dev,"%s: Device detached\n", __func__);

	/* remove cdev */
	cdev_del(ctx.veda_cdev);

	/* free major/minor no's used */
	unregister_chrdev_region(ctx.mydev, COUNT);

	device_destroy(ctx.vclass, ctx.mydev);
	class_destroy(ctx.vclass);
}

module_init(cdrv_probe);
module_exit(cdrv_remove);


MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("char driver skeleton");
MODULE_LICENSE("Dual MIT/GPL");
