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

/* Device structure to hold our driver data and discovered endpoints */
struct basic_usb_dev {
	struct usb_device *udev;
	struct usb_interface *interface;
	
	/* Discovered endpoints for future communication examples */
	struct usb_endpoint_descriptor *bulk_in_endp;
	struct usb_endpoint_descriptor *bulk_out_endp;
	struct usb_endpoint_descriptor *int_in_endp;
	struct usb_endpoint_descriptor *int_out_endp;
	
	/* Endpoint counts by type */
	int num_bulk_in;
	int num_bulk_out;
	int num_int_in;
	int num_int_out;
};

/* USB device ID table - tells kernel which devices we handle */
static struct usb_device_id basic_usb_table[] = {
	{ USB_DEVICE(USB_VENDOR_ID, USB_PRODUCT_ID) },
	{ }	/* Terminating entry */
};
MODULE_DEVICE_TABLE(usb, basic_usb_table);

/*
 * Parse and display device descriptor information
 */
static void parse_device(struct usb_device *udev)
{
	struct usb_device_descriptor *desc = &udev->descriptor;
	
	dev_info(&udev->dev, "=== USB Device Descriptor Data ===\n");
	dev_info(&udev->dev, "Device Specification: USB %x.%02x\n",
		 le16_to_cpu(desc->bcdUSB) >> 8,
		 le16_to_cpu(desc->bcdUSB) & 0xff);
	dev_info(&udev->dev, "Vendor ID: 0x%04X\n", le16_to_cpu(desc->idVendor));
	dev_info(&udev->dev, "Product ID: 0x%04X\n", le16_to_cpu(desc->idProduct));
	dev_info(&udev->dev, "Device Release: %x.%02x\n",
		 le16_to_cpu(desc->bcdDevice) >> 8,
		 le16_to_cpu(desc->bcdDevice) & 0xff);
	dev_info(&udev->dev, "Number of Configurations: %d\n", desc->bNumConfigurations);
}

/*
 * Parse and display configuration descriptor information
 */
static void parse_config(struct usb_device *udev)
{
	struct usb_host_config *config;
	int i;
	
	dev_info(&udev->dev, "=== USB Configuration Descriptor Data ===\n");
	dev_info(&udev->dev, "Total Configurations: %d\n", udev->descriptor.bNumConfigurations);
	
	for (i = 0; i < udev->descriptor.bNumConfigurations; i++) {
		config = &udev->config[i];
		dev_info(&udev->dev, "Configuration %d:\n", i);
		dev_info(&udev->dev, "  Configuration Value: %d\n", 
			 config->desc.bConfigurationValue);
		dev_info(&udev->dev, "  Number of Interfaces: %d\n", 
			 config->desc.bNumInterfaces);
		dev_info(&udev->dev, "  Max Power: %d mA\n", 
			 config->desc.bMaxPower * 2);
		dev_info(&udev->dev, "  Attributes: 0x%02X (%s%s%s)\n",
			 config->desc.bmAttributes,
			 (config->desc.bmAttributes & 0x80) ? "Bus-powered " : "",
			 (config->desc.bmAttributes & 0x40) ? "Self-powered " : "",
			 (config->desc.bmAttributes & 0x20) ? "Remote-wakeup" : "");
	}
}

/*
 * Parse and display interface descriptor information
 */
static void parse_interface(struct usb_interface *interface)
{
	struct usb_host_interface *iface_desc;
	int i;
	
	dev_info(&interface->dev, "=== USB Interface Descriptor Data ===\n");
	dev_info(&interface->dev, "Number of Alternate Settings: %d\n", 
		 interface->num_altsetting);
	
	for (i = 0; i < interface->num_altsetting; i++) {
		iface_desc = &interface->altsetting[i];
		dev_info(&interface->dev, "Alternate Setting %d:\n", i);
		dev_info(&interface->dev, "  Interface Number: %d\n", 
			 iface_desc->desc.bInterfaceNumber);
		dev_info(&interface->dev, "  Alternate Setting: %d\n", 
			 iface_desc->desc.bAlternateSetting);
		dev_info(&interface->dev, "  Number of Endpoints: %d\n", 
			 iface_desc->desc.bNumEndpoints);
		dev_info(&interface->dev, "  Interface Class: 0x%02X (%s)\n", 
			 iface_desc->desc.bInterfaceClass,
			 iface_desc->desc.bInterfaceClass == 8 ? "Mass Storage" :
			 iface_desc->desc.bInterfaceClass == 3 ? "HID" :
			 iface_desc->desc.bInterfaceClass == 9 ? "Hub" : "Other");
		dev_info(&interface->dev, "  Interface SubClass: 0x%02X\n", 
			 iface_desc->desc.bInterfaceSubClass);
		dev_info(&interface->dev, "  Interface Protocol: 0x%02X\n", 
			 iface_desc->desc.bInterfaceProtocol);
	}
}

/*
 * Discover and catalog all endpoints
 */
static void discover_endpoints(struct basic_usb_dev *dev)
{
	struct usb_host_interface *iface_desc;
	struct usb_endpoint_descriptor *endpoint;
	int i;
	const char *transfer_type;
	const char *direction;
	
	iface_desc = dev->interface->cur_altsetting;
	
	dev_info(&dev->interface->dev, "=== USB Endpoint Discovery & Cataloging ===\n");
	dev_info(&dev->interface->dev, "Interface has %d endpoints:\n",
		 iface_desc->desc.bNumEndpoints);
	
	/* Initialize counters */
	dev->num_bulk_in = dev->num_bulk_out = 0;
	dev->num_int_in = dev->num_int_out = 0;
	dev->bulk_in_endp = dev->bulk_out_endp = NULL;
	dev->int_in_endp = dev->int_out_endp = NULL;
	
	for (i = 0; i < iface_desc->desc.bNumEndpoints; i++) {
		endpoint = &iface_desc->endpoint[i].desc;
		
		/* Determine transfer type */
		switch (endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) {
		case USB_ENDPOINT_XFER_CONTROL:
			transfer_type = "Control";
			break;
		case USB_ENDPOINT_XFER_ISOC:
			transfer_type = "Isochronous";
			break;
		case USB_ENDPOINT_XFER_BULK:
			transfer_type = "Bulk";
			break;
		case USB_ENDPOINT_XFER_INT:
			transfer_type = "Interrupt";
			break;
		default:
			transfer_type = "Unknown";
			break;
		}
		
		/* Determine direction */
		direction = (endpoint->bEndpointAddress & USB_DIR_IN) ? "IN" : "OUT";
		
		dev_info(&dev->interface->dev, "Endpoint %d:\n", i);
		dev_info(&dev->interface->dev, "  Address: 0x%02X\n", 
			 endpoint->bEndpointAddress);
		dev_info(&dev->interface->dev, "  Direction: %s\n", direction);
		dev_info(&dev->interface->dev, "  Type: %s\n", transfer_type);
		dev_info(&dev->interface->dev, "  Max Packet Size: %d bytes\n",
			 le16_to_cpu(endpoint->wMaxPacketSize));
		
		if ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_INT) {
			dev_info(&dev->interface->dev, "  Interval: %d ms\n", 
				 endpoint->bInterval);
		}
		
		/* Catalog endpoints for future use */
		if ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_BULK) {
			if (endpoint->bEndpointAddress & USB_DIR_IN) {
				if (!dev->bulk_in_endp) {
					dev->bulk_in_endp = endpoint;
					dev_info(&dev->interface->dev, "  ★ Cataloged as primary BULK IN endpoint\n");
				}
				dev->num_bulk_in++;
			} else {
				if (!dev->bulk_out_endp) {
					dev->bulk_out_endp = endpoint;
					dev_info(&dev->interface->dev, "  ★ Cataloged as primary BULK OUT endpoint\n");
				}
				dev->num_bulk_out++;
			}
		} else if ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_INT) {
			if (endpoint->bEndpointAddress & USB_DIR_IN) {
				if (!dev->int_in_endp) {
					dev->int_in_endp = endpoint;
					dev_info(&dev->interface->dev, "  ★ Cataloged as primary INTERRUPT IN endpoint\n");
				}
				dev->num_int_in++;
			} else {
				if (!dev->int_out_endp) {
					dev->int_out_endp = endpoint;
					dev_info(&dev->interface->dev, "  ★ Cataloged as primary INTERRUPT OUT endpoint\n");
				}
				dev->num_int_out++;
			}
		}
	}
	
	/* Summary of discovered endpoints */
	dev_info(&dev->interface->dev, "=== Endpoint Discovery Summary ===\n");
	dev_info(&dev->interface->dev, "Bulk IN endpoints: %d\n", dev->num_bulk_in);
	dev_info(&dev->interface->dev, "Bulk OUT endpoints: %d\n", dev->num_bulk_out);
	dev_info(&dev->interface->dev, "Interrupt IN endpoints: %d\n", dev->num_int_in);
	dev_info(&dev->interface->dev, "Interrupt OUT endpoints: %d\n", dev->num_int_out);
	
	if (dev->bulk_in_endp || dev->bulk_out_endp) {
		dev_info(&dev->interface->dev, "✓ Device suitable for bulk transfer examples\n");
	}
	if (dev->int_in_endp || dev->int_out_endp) {
		dev_info(&dev->interface->dev, "✓ Device suitable for interrupt transfer examples\n");
	}
}

/*
 * Probe function - called when a matching device is plugged in
 * This is where we initialize our driver for the specific device
 */
static int probe(struct usb_interface *interface,
			   const struct usb_device_id *id)
{
	struct usb_device *udev = interface_to_usbdev(interface);
	struct basic_usb_dev *dev;

	dev_info(&interface->dev, "USB device now attached\n");
	/* Allocate memory for our device structure */
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		dev_err(&interface->dev, "Out of memory\n");
		return -ENOMEM;
	}

	/* Initialize our device structure */
	dev->udev = usb_get_dev(udev);
	dev->interface = interface;

	/* Store our data pointer in interface structure */
	usb_set_intfdata(interface, dev);

	/* Comprehensive USB descriptor walkthrough */
	parse_device(udev);
	parse_config(udev);
	parse_interface(interface);
	discover_endpoints(dev);
	return 0;
}

/*
 * Disconnect function - called when device is unplugged
 */
static void disconnect(struct usb_interface *interface)
{
	struct basic_usb_dev *dev;

	dev = usb_get_intfdata(interface);

	/* Clean up our allocated memory */
	usb_set_intfdata(interface, NULL);
	usb_put_dev(dev->udev);
	kfree(dev);

	dev_info(&interface->dev, "Basic USB driver disconnected\n");
}

/* USB driver structure */
static struct usb_driver basic_usb_driver = {
	.name		= "basic",
	.probe		= probe,
	.disconnect	= disconnect,
	.id_table	= basic_usb_table,
};

static int __init basic_init(void)
{
	int result;

	/* Register our driver with the USB subsystem */
	result = usb_register(&basic_usb_driver);
	if (result)
		pr_err("usb_register failed. Error number %d\n", result);
	else
		pr_info("Basic USB driver registered\n");

	return result;
}

static void __exit basic_exit(void)
{
	/* Deregister our driver */
	usb_deregister(&basic_usb_driver);
	pr_info("Basic USB driver deregistered\n");
}

module_init(basic_init);
module_exit(basic_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("raghu@techveda.org");
MODULE_DESCRIPTION("Basic USB Driver");
MODULE_VERSION("1.0");
