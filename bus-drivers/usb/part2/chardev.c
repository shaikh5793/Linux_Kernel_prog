/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>

#define USB_VENDOR_ID	0x1234
#define USB_PRODUCT_ID	0x5678
#define BUFFER_SIZE	1024
#define MINOR_BASE	0
#define DEVICE_COUNT	1

/* Our device structure */
struct chardev_usb_dev {
	struct usb_device *udev;
	struct usb_interface *interface;
	
	/* Character device */
	struct cdev cdev;
	dev_t dev_num;
	struct class *class;
	struct device *device;
	
	/* USB endpoints */
	struct usb_endpoint_descriptor *bulk_in_endp;
	struct usb_endpoint_descriptor *bulk_out_endp;
	
	/* Buffers and synchronization */
	unsigned char *bulk_in_buffer;
	unsigned char *bulk_out_buffer;
	struct mutex io_mutex;
	
	/* Reference counting */
	struct kref kref;
	int open_count;
};

static struct usb_device_id chardev_usb_table[] = {
	{ USB_DEVICE(USB_VENDOR_ID, USB_PRODUCT_ID) },
	{ }
};
MODULE_DEVICE_TABLE(usb, chardev_usb_table);

static struct usb_driver chardev_usb_driver;

/* Character device operations */
static int chardev_open(struct inode *inode, struct file *file)
{
	struct chardev_usb_dev *dev;
	
	dev = container_of(inode->i_cdev, struct chardev_usb_dev, cdev);
	
	mutex_lock(&dev->io_mutex);
	dev->open_count++;
	file->private_data = dev;
	mutex_unlock(&dev->io_mutex);
	
	dev_info(&dev->interface->dev, "Device opened (count: %d)\n", dev->open_count);
	return 0;
}

static int chardev_release(struct inode *inode, struct file *file)
{
	struct chardev_usb_dev *dev = file->private_data;
	
	if (!dev)
		return -ENODEV;
	
	mutex_lock(&dev->io_mutex);
	dev->open_count--;
	mutex_unlock(&dev->io_mutex);
	
	dev_info(&dev->interface->dev, "Device closed (count: %d)\n", dev->open_count);
	return 0;
}

static ssize_t chardev_read(struct file *file, char __user *buffer,
			    size_t count, loff_t *ppos)
{
	struct chardev_usb_dev *dev = file->private_data;
	int retval;
	int bytes_read;
	
	if (!dev || !dev->bulk_in_endp)
		return -ENODEV;
	
	if (count > BUFFER_SIZE)
		count = BUFFER_SIZE;
	
	mutex_lock(&dev->io_mutex);
	
	/*
	 * Perform bulk read from USB device using synchronous transfer
	 * =============================================================
	 * usb_bulk_msg() performs a synchronous bulk transfer with these parameters:
	 * 
	 * usb_dev:    Target USB device to read from
	 * pipe:       Encoded bulk IN pipe information
	 *            - usb_rcvbulkpipe(): Creates RECEIVE bulk pipe
	 *            - Combines device + endpoint address + IN direction
	 *            - Used for bulk data reception (large data transfers)
	 *            - No guaranteed bandwidth, uses available bus time
	 * data:       Kernel buffer to store received data
	 * len:        Maximum bytes to read in this transfer
	 * actual_len: Pointer to store actual bytes transferred
	 * timeout:    Timeout in milliseconds (5000ms = 5 seconds)
	 *            - 0 = wait forever, >0 = timeout after specified ms
	 * 
	 * Returns: 0 on success, negative error code on failure
	 * Note: This blocks until transfer completes or times out
	 */
	retval = usb_bulk_msg(dev->udev,
			      usb_rcvbulkpipe(dev->udev, dev->bulk_in_endp->bEndpointAddress),
			      dev->bulk_in_buffer,
			      count,
			      &bytes_read,
			      5000);
	
	if (retval) {
		dev_err(&dev->interface->dev, "Bulk read failed: %d\n", retval);
		mutex_unlock(&dev->io_mutex);
		return retval;
	}
	
	/* Copy data to user space */
	if (copy_to_user(buffer, dev->bulk_in_buffer, bytes_read)) {
		dev_err(&dev->interface->dev, "Failed to copy data to user space\n");
		mutex_unlock(&dev->io_mutex);
		return -EFAULT;
	}
	
	mutex_unlock(&dev->io_mutex);
	
	dev_info(&dev->interface->dev, "Read %d bytes from device\n", bytes_read);
	return bytes_read;
}

static ssize_t chardev_write(struct file *file, const char __user *buffer,
			     size_t count, loff_t *ppos)
{
	struct chardev_usb_dev *dev = file->private_data;
	int retval;
	int bytes_written;
	
	if (!dev || !dev->bulk_out_endp)
		return -ENODEV;
	
	if (count > BUFFER_SIZE)
		count = BUFFER_SIZE;
	
	mutex_lock(&dev->io_mutex);
	
	/* Copy data from user space */
	if (copy_from_user(dev->bulk_out_buffer, buffer, count)) {
		dev_err(&dev->interface->dev, "Failed to copy data from user space\n");
		mutex_unlock(&dev->io_mutex);
		return -EFAULT;
	}
	
	/*
	 * Perform bulk write to USB device using synchronous transfer
	 * ============================================================
	 * usb_bulk_msg() performs a synchronous bulk transfer with these parameters:
	 * 
	 * usb_dev:    Target USB device to write to
	 * pipe:       Encoded bulk OUT pipe information
	 *            - usb_sndbulkpipe(): Creates SEND bulk pipe
	 *            - Combines device + endpoint address + OUT direction
	 *            - Used for bulk data transmission (large data transfers)
	 *            - Best effort delivery, no guaranteed timing
	 * data:       Kernel buffer containing data to send
	 * len:        Number of bytes to write in this transfer
	 * actual_len: Pointer to store actual bytes transferred
	 * timeout:    Timeout in milliseconds (5000ms = 5 seconds)
	 *            - Prevents indefinite blocking on device issues
	 * 
	 * Returns: 0 on success, negative error code on failure
	 * Note: This blocks until transfer completes or times out
	 */
	retval = usb_bulk_msg(dev->udev,
			      usb_sndbulkpipe(dev->udev, dev->bulk_out_endp->bEndpointAddress),
			      dev->bulk_out_buffer,
			      count,
			      &bytes_written,
			      5000);
	
	if (retval) {
		dev_err(&dev->interface->dev, "Bulk write failed: %d\n", retval);
		mutex_unlock(&dev->io_mutex);
		return retval;
	}
	
	mutex_unlock(&dev->io_mutex);
	
	dev_info(&dev->interface->dev, "Wrote %d bytes to device\n", bytes_written);
	return bytes_written;
}

static const struct file_operations chardev_fops = {
	.owner = THIS_MODULE,
	.open = chardev_open,
	.release = chardev_release,
	.read = chardev_read,
	.write = chardev_write,
};

/* Reference counting cleanup */
static void chardev_delete(struct kref *kref)
{
	struct chardev_usb_dev *dev = container_of(kref, struct chardev_usb_dev, kref);
	
	usb_put_dev(dev->udev);
	kfree(dev->bulk_in_buffer);
	kfree(dev->bulk_out_buffer);
	kfree(dev);
}

/* Find bulk endpoints */
static int find_bulk_endpoints(struct chardev_usb_dev *dev)
{
	struct usb_host_interface *iface_desc;
	struct usb_endpoint_descriptor *endpoint;
	int i;
	
	iface_desc = dev->interface->cur_altsetting;
	
	for (i = 0; i < iface_desc->desc.bNumEndpoints; i++) {
		endpoint = &iface_desc->endpoint[i].desc;
		
		if ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_BULK) {
			if (endpoint->bEndpointAddress & USB_DIR_IN) {
				dev->bulk_in_endp = endpoint;
				dev_info(&dev->interface->dev, "Found bulk IN endpoint: 0x%02X\n",
					 endpoint->bEndpointAddress);
			} else {
				dev->bulk_out_endp = endpoint;
				dev_info(&dev->interface->dev, "Found bulk OUT endpoint: 0x%02X\n",
					 endpoint->bEndpointAddress);
			}
		}
	}
	
	return 0;
}

static int chardev_usb_probe(struct usb_interface *interface,
			     const struct usb_device_id *id)
{
	struct usb_device *udev = interface_to_usbdev(interface);
	struct chardev_usb_dev *dev;
	int retval;
	
	dev_info(&interface->dev, "Character device USB driver attached\n");
	
	/* Allocate device structure */
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;
	
	kref_init(&dev->kref);
	mutex_init(&dev->io_mutex);
	dev->udev = usb_get_dev(udev);
	dev->interface = interface;
	
	/* Find bulk endpoints */
	find_bulk_endpoints(dev);
	
	/* Allocate buffers */
	dev->bulk_in_buffer = kmalloc(BUFFER_SIZE, GFP_KERNEL);
	dev->bulk_out_buffer = kmalloc(BUFFER_SIZE, GFP_KERNEL);
	if (!dev->bulk_in_buffer || !dev->bulk_out_buffer) {
		dev_err(&interface->dev, "Failed to allocate buffers\n");
		retval = -ENOMEM;
		goto error;
	}
	
	/* Allocate character device number */
	retval = alloc_chrdev_region(&dev->dev_num, MINOR_BASE, DEVICE_COUNT, "chardev_usb");
	if (retval) {
		dev_err(&interface->dev, "Failed to allocate character device number\n");
		goto error;
	}
	
	/* Initialize character device */
	cdev_init(&dev->cdev, &chardev_fops);
	dev->cdev.owner = THIS_MODULE;
	
	/* Add character device */
	retval = cdev_add(&dev->cdev, dev->dev_num, DEVICE_COUNT);
	if (retval) {
		dev_err(&interface->dev, "Failed to add character device\n");
		goto error_chrdev;
	}
	
	/* Create device class */
	dev->class = class_create("chardev_usb");
	if (IS_ERR(dev->class)) {
		dev_err(&interface->dev, "Failed to create device class\n");
		retval = PTR_ERR(dev->class);
		goto error_cdev;
	}
	
	/* Create device node */
	dev->device = device_create(dev->class, &interface->dev, dev->dev_num,
				    NULL, "chardev_usb%d", MINOR(dev->dev_num));
	if (IS_ERR(dev->device)) {
		dev_err(&interface->dev, "Failed to create device node\n");
		retval = PTR_ERR(dev->device);
		goto error_class;
	}
	
	usb_set_intfdata(interface, dev);
	dev_info(&interface->dev, "Character device created: /dev/chardev_usb%d\n", 
		 MINOR(dev->dev_num));
	
	return 0;
	
error_class:
	class_destroy(dev->class);
error_cdev:
	cdev_del(&dev->cdev);
error_chrdev:
	unregister_chrdev_region(dev->dev_num, DEVICE_COUNT);
error:
	kref_put(&dev->kref, chardev_delete);
	return retval;
}

static void chardev_usb_disconnect(struct usb_interface *interface)
{
	struct chardev_usb_dev *dev = usb_get_intfdata(interface);
	
	if (!dev)
		return;
	
	usb_set_intfdata(interface, NULL);
	
	/* Remove character device */
	device_destroy(dev->class, dev->dev_num);
	class_destroy(dev->class);
	cdev_del(&dev->cdev);
	unregister_chrdev_region(dev->dev_num, DEVICE_COUNT);
	
	/* Decrement reference count */
	kref_put(&dev->kref, chardev_delete);
	
	dev_info(&interface->dev, "Character device USB driver disconnected\n");
}

static struct usb_driver chardev_usb_driver = {
	.name		= "chardev",
	.probe		= chardev_usb_probe,
	.disconnect	= chardev_usb_disconnect,
	.id_table	= chardev_usb_table,
};

static int __init chardev_init(void)
{
	return usb_register(&chardev_usb_driver);
}

static void __exit chardev_exit(void)
{
	usb_deregister(&chardev_usb_driver);
}

module_init(chardev_init);
module_exit(chardev_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("raghu@techveda.org");
MODULE_DESCRIPTION("Character Device USB Driver");
MODULE_VERSION("1.0");
