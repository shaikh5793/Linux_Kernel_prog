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
#include <linux/hid.h>

#define USB_VENDOR_ID 0x046d  /* Logitech */
#define USB_PRODUCT_ID 0xc534  /* Logitech Keyboard */
#define HID_BUFFER_SIZE 8

/* Device structure */
struct hid_usb_dev {
	struct usb_device *udev;
	struct usb_interface *interface;
	
	/* Interrupt endpoint and URB */
	struct usb_endpoint_descriptor *intr_in_endp;
	struct urb *intr_urb;
	unsigned char *intr_buffer;
};

static struct usb_device_id hid_usb_table[] = {
	{ USB_DEVICE(USB_VENDOR_ID, USB_PRODUCT_ID) },
	{ USB_INTERFACE_INFO(USB_INTERFACE_CLASS_HID, 
			     USB_INTERFACE_SUBCLASS_BOOT,
			     USB_INTERFACE_PROTOCOL_KEYBOARD) },
	{ USB_INTERFACE_INFO(USB_INTERFACE_CLASS_HID,
			     USB_INTERFACE_SUBCLASS_BOOT,
			     USB_INTERFACE_PROTOCOL_MOUSE) },
	{ }
};
MODULE_DEVICE_TABLE(usb, hid_usb_table);

/*
 * URB completion callback for HID interrupt transfers
 */
static void hid_interrupt_callback(struct urb *urb)
{
	struct hid_usb_dev *dev = urb->context;
	int status = urb->status;
	
	if (status == 0) {
		dev_info(&dev->interface->dev,
			"HID URB completed! Received %d bytes\n",
			urb->actual_length);
		
		if (urb->actual_length > 0) {
			print_hex_dump(KERN_INFO, "HID Data: ",
				       DUMP_PREFIX_OFFSET, 16, 1,
				       dev->intr_buffer,
				       min((int)urb->actual_length, 8), true);
		}
		
		/* Resubmit the URB for continuous monitoring */
		usb_submit_urb(urb, GFP_ATOMIC);
	} else {
		dev_err(&dev->interface->dev,
			"HID URB failed with status: %d\n", status);
	}
}

static int hid_usb_probe(struct usb_interface *interface,
			 const struct usb_device_id *id)
{
	struct usb_device *udev = interface_to_usbdev(interface);
	struct hid_usb_dev *dev;
	struct usb_host_interface *iface_desc;
	struct usb_endpoint_descriptor *endpoint;
	int i, retval;
	
	dev_info(&interface->dev, "HID USB driver attached\n");
	
	/* Allocate device structure */
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;
	
	dev->udev = usb_get_dev(udev);
	dev->interface = interface;
	
	/* Find interrupt IN endpoint */
	iface_desc = interface->cur_altsetting;
	for (i = 0; i < iface_desc->desc.bNumEndpoints; i++) {
		endpoint = &iface_desc->endpoint[i].desc;
		
		if ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) ==
		    USB_ENDPOINT_XFER_INT &&
		    (endpoint->bEndpointAddress & USB_DIR_IN)) {
			dev->intr_in_endp = endpoint;
			dev_info(&interface->dev,
				"Found interrupt IN endpoint: 0x%02X\n",
				endpoint->bEndpointAddress);
			break;
		}
	}
	
	if (!dev->intr_in_endp) {
		dev_err(&interface->dev, "No interrupt IN endpoint found\n");
		retval = -ENODEV;
		goto error;
	}
	
	/* Allocate URB and buffer */
	dev->intr_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!dev->intr_urb) {
		dev_err(&interface->dev, "Failed to allocate interrupt URB\n");
		retval = -ENOMEM;
		goto error;
	}
	
	dev->intr_buffer = kmalloc(HID_BUFFER_SIZE, GFP_KERNEL);
	if (!dev->intr_buffer) {
		dev_err(&interface->dev, "Failed to allocate interrupt buffer\n");
		retval = -ENOMEM;
		goto error;
	}
	
	/* Fill the URB */
	usb_fill_int_urb(dev->intr_urb,
			 dev->udev,
			 usb_rcvintpipe(dev->udev,
					dev->intr_in_endp->bEndpointAddress),
			 dev->intr_buffer,
			 HID_BUFFER_SIZE,
			 hid_interrupt_callback,
			 dev,
			 dev->intr_in_endp->bInterval);
	
	/* Submit the URB */
	retval = usb_submit_urb(dev->intr_urb, GFP_KERNEL);
	if (retval) {
		dev_err(&interface->dev, "Failed to submit interrupt URB: %d\n", retval);
		goto error;
	}
	
	usb_set_intfdata(interface, dev);
	
	return 0;
	
error:
	if (dev->intr_urb)
		usb_free_urb(dev->intr_urb);
	if (dev->intr_buffer)
		kfree(dev->intr_buffer);
	usb_put_dev(dev->udev);
	kfree(dev);
	return retval;
}

static void hid_usb_disconnect(struct usb_interface *interface)
{
	struct hid_usb_dev *dev = usb_get_intfdata(interface);
	
	usb_set_intfdata(interface, NULL);
	usb_kill_urb(dev->intr_urb);
	usb_free_urb(dev->intr_urb);
	kfree(dev->intr_buffer);
	usb_put_dev(dev->udev);
	kfree(dev);
	
	dev_info(&interface->dev, "HID USB driver disconnected\n");
}

static struct usb_driver hid_usb_driver = {
	.name       = "hid",
	.probe      = hid_usb_probe,
	.disconnect = hid_usb_disconnect,
	.id_table   = hid_usb_table,
};

static int __init hid_init(void)
{
	return usb_register(&hid_usb_driver);
}

static void __exit hid_exit(void)
{
	usb_deregister(&hid_usb_driver);
}

module_init(hid_init);
module_exit(hid_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("raghu@techveda.org");
MODULE_DESCRIPTION("HID USB Driver");
MODULE_VERSION("1.0");
