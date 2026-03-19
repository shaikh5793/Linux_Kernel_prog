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
#include <linux/mutex.h>
#include <linux/completion.h>

#define USB_VENDOR_ID	0x03f0  /* HP, Inc */
#define USB_PRODUCT_ID	0x6d40  /* x765w USB pendrive */
#define BULK_BUF_SIZE	512

/* Device structure for async operations */
struct async_usb_dev {
	struct usb_device *udev;
	struct usb_interface *interface;
	
	/* Bulk endpoints */
	struct usb_endpoint_descriptor *bulk_in_endp;
	struct usb_endpoint_descriptor *bulk_out_endp;
	
	/* URBs for async operations */
	struct urb *read_urb;
	struct urb *write_urb;
	
	/* Transfer buffers */
	unsigned char *read_buffer;
	unsigned char *write_buffer;
	
	/* Synchronization */
	struct mutex io_mutex;
	struct completion read_completion;
	struct completion write_completion;
	
	/* Status tracking */
	int read_status;
	int write_status;
};

static struct usb_device_id async_usb_table[] = {
	{ USB_DEVICE(USB_VENDOR_ID, USB_PRODUCT_ID) },
	{ }
};
MODULE_DEVICE_TABLE(usb, async_usb_table);

/*
 * URB completion callback for read operations
 */
static void read_callback(struct urb *urb)
{
	struct async_usb_dev *dev = urb->context;
	
	dev->read_status = urb->status;
	
	if (urb->status == 0) {
		dev_info(&dev->interface->dev, 
			"Read URB completed successfully! Received %d bytes\n",
			urb->actual_length);
		
		if (urb->actual_length > 0) {
			print_hex_dump(KERN_INFO, "USB Async Read: ", 
				       DUMP_PREFIX_OFFSET, 16, 1, 
				       dev->read_buffer,
				       min((int)urb->actual_length, 16), true);
		}
	} else {
		dev_err(&dev->interface->dev, 
			"Read URB failed with status: %d\n", urb->status);
	}
	
	complete(&dev->read_completion);
}

/*
 * URB completion callback for write operations
 */
static void write_callback(struct urb *urb)
{
	struct async_usb_dev *dev = urb->context;
	
	dev->write_status = urb->status;
	
	if (urb->status == 0) {
		dev_info(&dev->interface->dev,
			"Write URB completed successfully! Sent %d bytes\n",
			urb->actual_length);
	} else {
		dev_err(&dev->interface->dev,
			"Write URB failed with status: %d\n", urb->status);
	}
	
	complete(&dev->write_completion);
}

/*
 * Perform asynchronous bulk read
 */
static int async_read(struct async_usb_dev *dev)
{
	int retval;
	
	if (!dev->bulk_in_endp) {
		dev_err(&dev->interface->dev, "No bulk IN endpoint found\n");
		return -ENODEV;
	}
	
	mutex_lock(&dev->io_mutex);
	
	/* Reset completion */
	reinit_completion(&dev->read_completion);
	
	/* Fill the URB */
	usb_fill_bulk_urb(dev->read_urb,
			  dev->udev,
			  usb_rcvbulkpipe(dev->udev, 
					  dev->bulk_in_endp->bEndpointAddress),
			  dev->read_buffer,
			  BULK_BUF_SIZE,
			  read_callback,
			  dev);
	
	/* Submit the URB */
	retval = usb_submit_urb(dev->read_urb, GFP_KERNEL);
	if (retval) {
		dev_err(&dev->interface->dev, "Failed to submit read URB: %d\n", retval);
		mutex_unlock(&dev->io_mutex);
		return retval;
	}
	
	dev_info(&dev->interface->dev, "Bulk read URB submitted, waiting for completion...\n");
	
	/* Wait for completion */
	if (!wait_for_completion_timeout(&dev->read_completion,
					 msecs_to_jiffies(5000))) {
		dev_err(&dev->interface->dev, "Bulk read timed out\n");
		usb_kill_urb(dev->read_urb);
		mutex_unlock(&dev->io_mutex);
		return -ETIMEDOUT;
	}
	
	retval = dev->read_status;
	mutex_unlock(&dev->io_mutex);
	
	return retval;
}

/*
 * Perform asynchronous bulk write
 */
static int async_write(struct async_usb_dev *dev, const char *data, size_t len)
{
	int retval;
	
	if (!dev->bulk_out_endp) {
		dev_err(&dev->interface->dev, "No bulk OUT endpoint found\n");
		return -ENODEV;
	}
	
	if (len > BULK_BUF_SIZE) {
		dev_err(&dev->interface->dev, "Data too large for buffer\n");
		return -EINVAL;
	}
	
	mutex_lock(&dev->io_mutex);
	
	/* Copy data to transfer buffer */
	memcpy(dev->write_buffer, data, len);
	
	/* Reset completion */
	reinit_completion(&dev->write_completion);
	
	/* Fill the URB */
	usb_fill_bulk_urb(dev->write_urb,
			  dev->udev,
			  usb_sndbulkpipe(dev->udev,
					  dev->bulk_out_endp->bEndpointAddress),
			  dev->write_buffer,
			  len,
			  write_callback,
			  dev);
	
	/* Submit the URB */
	retval = usb_submit_urb(dev->write_urb, GFP_KERNEL);
	if (retval) {
		dev_err(&dev->interface->dev, "Failed to submit write URB: %d\n", retval);
		mutex_unlock(&dev->io_mutex);
		return retval;
	}
	
	dev_info(&dev->interface->dev, "Bulk write URB submitted (%zu bytes)\n", len);
	
	/* Wait for completion */
	if (!wait_for_completion_timeout(&dev->write_completion,
					 msecs_to_jiffies(5000))) {
		dev_err(&dev->interface->dev, "Bulk write timed out\n");
		usb_kill_urb(dev->write_urb);
		mutex_unlock(&dev->io_mutex);
		return -ETIMEDOUT;
	}
	
	retval = dev->write_status;
	mutex_unlock(&dev->io_mutex);
	
	return retval;
}

/*
 * Find and store bulk endpoints
 */
static int find_endpoints(struct async_usb_dev *dev)
{
	struct usb_host_interface *iface_desc;
	struct usb_endpoint_descriptor *endpoint;
	int i;
	
	iface_desc = dev->interface->cur_altsetting;
	
	for (i = 0; i < iface_desc->desc.bNumEndpoints; i++) {
		endpoint = &iface_desc->endpoint[i].desc;
		
		/* Check if this is a bulk endpoint */
		if ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) == 
		    USB_ENDPOINT_XFER_BULK) {
			
			if (endpoint->bEndpointAddress & USB_DIR_IN) {
				/* Bulk IN endpoint */
				dev->bulk_in_endp = endpoint;
				dev_info(&dev->interface->dev, 
					"Found bulk IN endpoint: 0x%02X\n",
					endpoint->bEndpointAddress);
			} else {
				/* Bulk OUT endpoint */
				dev->bulk_out_endp = endpoint;
				dev_info(&dev->interface->dev, 
					"Found bulk OUT endpoint: 0x%02X\n",
					endpoint->bEndpointAddress);
			}
		}
	}
	
	return 0;
}

static int probe(struct usb_interface *interface, const struct usb_device_id *id)
{
	struct usb_device *udev = interface_to_usbdev(interface);
	struct async_usb_dev *dev;
	int retval;
	const char test_data[] = "Hello USB Device!";
	
	dev_info(&interface->dev, "Async USB driver attached\n");
	
	/* Allocate device structure */
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;
	
	dev->udev = usb_get_dev(udev);
	dev->interface = interface;
	
	/* Initialize synchronization objects */
	mutex_init(&dev->io_mutex);
	init_completion(&dev->read_completion);
	init_completion(&dev->write_completion);
	
	/* Find bulk endpoints */
	find_endpoints(dev);
	
	/* Allocate URBs */
	dev->read_urb = usb_alloc_urb(0, GFP_KERNEL);
	dev->write_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!dev->read_urb || !dev->write_urb) {
		dev_err(&interface->dev, "Failed to allocate URBs\n");
		retval = -ENOMEM;
		goto error;
	}
	
	/* Allocate transfer buffers */
	dev->read_buffer = kmalloc(BULK_BUF_SIZE, GFP_KERNEL);
	dev->write_buffer = kmalloc(BULK_BUF_SIZE, GFP_KERNEL);
	if (!dev->read_buffer || !dev->write_buffer) {
		dev_err(&interface->dev, "Failed to allocate transfer buffers\n");
		retval = -ENOMEM;
		goto error;
	}
	
	usb_set_intfdata(interface, dev);
	
	/* Perform bulk transfers */
	if (dev->bulk_out_endp) {
		retval = async_write(dev, test_data, strlen(test_data));
		if (retval)
			dev_err(&interface->dev, "Bulk write operation failed: %d\n", retval);
	}
	
	if (dev->bulk_in_endp) {
		retval = async_read(dev);
		if (retval)
			dev_err(&interface->dev, "Bulk read operation failed: %d\n", retval);
	}
	
	return 0;
	
error:
	/* Cleanup on error */
	if (dev->read_buffer)
		kfree(dev->read_buffer);
	if (dev->write_buffer)
		kfree(dev->write_buffer);
	if (dev->read_urb)
		usb_free_urb(dev->read_urb);
	if (dev->write_urb)
		usb_free_urb(dev->write_urb);
	usb_put_dev(dev->udev);
	kfree(dev);
	
	return retval;
}

static void disconnect(struct usb_interface *interface)
{
	struct async_usb_dev *dev;
	
	dev = usb_get_intfdata(interface);
	if (!dev)
		return;
	
	/* Cancel any pending URBs */
	usb_kill_urb(dev->read_urb);
	usb_kill_urb(dev->write_urb);
	
	/* Clean up */
	usb_set_intfdata(interface, NULL);
	
	usb_free_urb(dev->read_urb);
	usb_free_urb(dev->write_urb);
	
	kfree(dev->read_buffer);
	kfree(dev->write_buffer);
	
	usb_put_dev(dev->udev);
	kfree(dev);
	
	dev_info(&interface->dev, "Async USB driver disconnected\n");
}

static struct usb_driver async_usb_driver = {
	.name		= "async",
	.probe		= probe,
	.disconnect	= disconnect,
	.id_table	= async_usb_table,
};

static int __init async_init(void)
{
	return usb_register(&async_usb_driver);
}

static void __exit async_exit(void)
{
	usb_deregister(&async_usb_driver);
}

module_init(async_init);
module_exit(async_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("raghu@techveda.org");
MODULE_DESCRIPTION("Async USB Driver");
MODULE_VERSION("1.0");
