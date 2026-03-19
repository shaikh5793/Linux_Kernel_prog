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

static int drv_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	dev_dbg(dev,"%s: Device found\n",__func__);
	return 0;
}

static int drv_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	dev_dbg(dev, "%s: Device detached\n",__func__);
	return 0;
}

static struct platform_driver sample_platdrv = {
	.probe  = drv_probe,
	.remove = drv_remove,
	.driver = {
		.name  = "veda_sample"// matches platform device name
	},
};

module_platform_driver(sample_platdrv);

MODULE_AUTHOR("Raghu Bharadwaj <raghu@techveda.org>");
MODULE_DESCRIPTION("platform driver skeleton");
MODULE_LICENSE("Dual MIT/GPL");
