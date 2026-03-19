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

#define USB_VENDOR_ID	0x03f0  /* HP, Inc */
#define USB_PRODUCT_ID	0x6d40  /* x765w USB pendrive */
#define BULK_BUF_SIZE	512

/* Our device structure */
struct control_usb_dev {
	struct usb_device *udev;
	struct usb_interface *interface;
	
	/* Bulk endpoints for synchronous I/O */
	struct usb_endpoint_descriptor *bulk_in_endp;
	struct usb_endpoint_descriptor *bulk_out_endp;
	
	/* Buffers for synchronous transfers */
	unsigned char *read_buffer;
	unsigned char *write_buffer;
};

static struct usb_device_id control_usb_table[] = {
	{ USB_DEVICE(USB_VENDOR_ID, USB_PRODUCT_ID) },
	{ }
};
MODULE_DEVICE_TABLE(usb, control_usb_table);


/*
 * Find and store bulk endpoints for I/O operations
 */
static int find_endpoints(struct control_usb_dev *dev)
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

/*
 * Discover and print information about endpoints
 * This demonstrates how to iterate through interface descriptors
 */
static void discover_endpoints(struct usb_interface *interface)
{
	struct usb_host_interface *iface_desc;
	struct usb_endpoint_descriptor *endpoint;
	int i;

	iface_desc = interface->cur_altsetting;
	
	dev_info(&interface->dev, "Interface has %d endpoints:\n",
		 iface_desc->desc.bNumEndpoints);

	for (i = 0; i < iface_desc->desc.bNumEndpoints; i++) {
		endpoint = &iface_desc->endpoint[i].desc;
		
		dev_info(&interface->dev, "Endpoint %d:\n", i);
		dev_info(&interface->dev, "  Address: 0x%02X\n", 
			 endpoint->bEndpointAddress);
		dev_info(&interface->dev, "  Direction: %s\n",
			 (endpoint->bEndpointAddress & USB_DIR_IN) ? "IN" : "OUT");
		dev_info(&interface->dev, "  Type: ");
		
		switch (endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) {
		case USB_ENDPOINT_XFER_CONTROL:
			printk(KERN_CONT "Control\n");
			break;
		case USB_ENDPOINT_XFER_ISOC:
			printk(KERN_CONT "Isochronous\n");
			break;
		case USB_ENDPOINT_XFER_BULK:
			printk(KERN_CONT "Bulk\n");
			break;
		case USB_ENDPOINT_XFER_INT:
			printk(KERN_CONT "Interrupt\n");
			break;
		}
		
		dev_info(&interface->dev, "  Max Packet Size: %d\n",
			 le16_to_cpu(endpoint->wMaxPacketSize));
	}
}

/*
 * Perform synchronous bulk read operation
 */
static int sync_read(struct control_usb_dev *dev)
{
	int retval;
	int actual_length;
	
	if (!dev->bulk_in_endp) {
		dev_err(&dev->interface->dev, "No bulk IN endpoint found\n");
		return -ENODEV;
	}
	
	/*
	 * Perform synchronous bulk read transfer
	 * ======================================
	 * usb_bulk_msg() performs a synchronous bulk transfer:
	 * 
	 * usb_dev:      Target USB device
	 * pipe:         Bulk IN pipe configuration
	 *              - usb_rcvbulkpipe(): Creates RECEIVE bulk pipe
	 *              - Combines device + endpoint address + IN direction
	 *              - Bulk transfers provide error detection and flow control
	 * data:         Buffer to store received data
	 * len:          Maximum bytes to receive in this transfer
	 * actual_length: Pointer to store actual bytes received
	 * timeout:      Timeout in milliseconds (5000ms = 5 seconds)
	 * 
	 * Returns: 0 on success, negative error code on failure
	 * Note: This is a blocking call that waits for transfer completion
	 *       Unlike URB operations, this handles the entire transaction internally
	 */
	retval = usb_bulk_msg(dev->udev,
			      usb_rcvbulkpipe(dev->udev,
					      dev->bulk_in_endp->bEndpointAddress),
			      dev->read_buffer,
			      BULK_BUF_SIZE,
			      &actual_length,
			      5000);
	
	if (retval == 0) {
		dev_info(&dev->interface->dev, "Sync bulk read successful! Received %d bytes\n", 
			 actual_length);
		
		/* Print first few bytes as hex dump for debugging */
		if (actual_length > 0) {
			print_hex_dump(KERN_INFO, "USB Read Data: ", DUMP_PREFIX_OFFSET,
				       16, 1, dev->read_buffer, 
				       min(actual_length, 16), true);
		}
	} else {
		dev_err(&dev->interface->dev, "Sync bulk read failed with error: %d\n", retval);
	}
	
	return retval;
}

/*
 * Perform synchronous bulk write operation
 */
static int sync_write(struct control_usb_dev *dev, const char *data, size_t len)
{
	int retval;
	int actual_length;
	
	if (!dev->bulk_out_endp) {
		dev_err(&dev->interface->dev, "No bulk OUT endpoint found\n");
		return -ENODEV;
	}
	
	if (len > BULK_BUF_SIZE) {
		dev_err(&dev->interface->dev, "Data too large for buffer\n");
		return -EINVAL;
	}
	
	/* Copy data to our transfer buffer */
	memcpy(dev->write_buffer, data, len);
	
	/*
	 * Perform synchronous bulk write transfer
	 * =======================================
	 * usb_bulk_msg() performs a synchronous bulk transfer:
	 * 
	 * usb_dev:      Target USB device
	 * pipe:         Bulk OUT pipe configuration
	 *              - usb_sndbulkpipe(): Creates SEND bulk pipe
	 *              - Combines device + endpoint address + OUT direction
	 *              - Bulk transfers guarantee delivery and error detection
	 * data:         Buffer containing data to send
	 * len:          Number of bytes to send in this transfer
	 * actual_length: Pointer to store actual bytes sent
	 * timeout:      Timeout in milliseconds (5000ms = 5 seconds)
	 * 
	 * Returns: 0 on success, negative error code on failure
	 * Note: This is a blocking call that completes the entire transfer
	 *       The function handles USB protocol details automatically
	 */
	retval = usb_bulk_msg(dev->udev,
			      usb_sndbulkpipe(dev->udev,
					      dev->bulk_out_endp->bEndpointAddress),
			      dev->write_buffer,
			      len,
			      &actual_length,
			      5000);
	
	if (retval == 0) {
		dev_info(&dev->interface->dev, "Sync bulk write successful! Sent %d bytes\n", 
			 actual_length);
	} else {
		dev_err(&dev->interface->dev, "Sync bulk write failed with error: %d\n", retval);
	}
	
	return retval;
}

static int probe(struct usb_interface *interface,
			     const struct usb_device_id *id)
{
	struct usb_device *udev = interface_to_usbdev(interface);
	struct control_usb_dev *dev;
	int retval;
	const char test_data[] = "Hello USB Device via Sync!";

	dev_info(&interface->dev, "Synchronous Bulk USB driver attached\n");

	/* Allocate our device structure */
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev->udev = usb_get_dev(udev);
	dev->interface = interface;

	/* Allocate transfer buffers */
	dev->read_buffer = kmalloc(BULK_BUF_SIZE, GFP_KERNEL);
	dev->write_buffer = kmalloc(BULK_BUF_SIZE, GFP_KERNEL);
	if (!dev->read_buffer || !dev->write_buffer) {
		dev_err(&interface->dev, "Failed to allocate transfer buffers\n");
		retval = -ENOMEM;
		goto error;
	}

	usb_set_intfdata(interface, dev);

	/* Discover and enumerate all endpoints */
	discover_endpoints(interface);

	/* Find bulk endpoints for synchronous I/O */
	find_endpoints(dev);

	/* Demonstrate synchronous bulk transfers */
	dev_info(&interface->dev, "=== Demonstrating Synchronous Bulk Transfers ===\n");
	
	if (dev->bulk_out_endp) {
		retval = sync_write(dev, test_data, strlen(test_data));
		if (retval)
			dev_err(&interface->dev, "Sync bulk write operation failed: %d\n", retval);
	} else {
		dev_info(&interface->dev, "No bulk OUT endpoint - skipping write test\n");
	}

	if (dev->bulk_in_endp) {
		retval = sync_read(dev);
		if (retval)
			dev_err(&interface->dev, "Sync bulk read operation failed: %d\n", retval);
	} else {
		dev_info(&interface->dev, "No bulk IN endpoint - skipping read test\n");
	}

	dev_info(&interface->dev, "=== Synchronous USB operations completed ===\n");
	return 0;

error:
	/* Cleanup on error */
	if (dev->read_buffer)
		kfree(dev->read_buffer);
	if (dev->write_buffer)
		kfree(dev->write_buffer);
	usb_put_dev(dev->udev);
	kfree(dev);
	
	return retval;
}

static void disconnect(struct usb_interface *interface)
{
	struct control_usb_dev *dev;

	dev = usb_get_intfdata(interface);
	if (!dev)
		return;

	/* Clean up allocated resources */
	usb_set_intfdata(interface, NULL);
	
	kfree(dev->read_buffer);
	kfree(dev->write_buffer);
	
	usb_put_dev(dev->udev);
	kfree(dev);

	dev_info(&interface->dev, "Synchronous Bulk USB driver disconnected\n");
}

static struct usb_driver control_usb_driver = {
	.name		= "sync_bulk",
	.probe		= probe,
	.disconnect	= disconnect,
	.id_table	= control_usb_table,
};

static int __init control_init(void)
{
	return usb_register(&control_usb_driver);
}

static void __exit control_exit(void)
{
	usb_deregister(&control_usb_driver);
}

module_init(control_init);
module_exit(control_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("raghu@techveda.org");
MODULE_DESCRIPTION("Synchronous Bulk USB Driver");
MODULE_VERSION("1.0");
