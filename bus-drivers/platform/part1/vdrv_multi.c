/*
 * Copyright (c) 2024 TECH VEDA
 * Author: Raghu Bharadwaj
 * This code is dual-licensed under the MIT License and GPL v2
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/slab.h>

#define DRIVER_NAME "vdev"
#define CLASS_NAME "vdev_class"

static int major;
static struct class *vdev_class = NULL;

struct vdev_info {
    struct cdev cdev;
    void *devmem;
    int size;
    int permission; // 1=read-only, 2=write-only, 3=read/write
    char serial[32];
    int device_id;
};


/*
 * 
 * fd = open("/dev/vDev0", RDWR);
 * 
 * open() operation should obtain the reference to config data of the device
 * needed by the caller app
 */

static int vdev_open(struct inode *inode, struct file *file) {
	/* returns address of device specific vdev_info instance */
    struct vdev_info *info = container_of(inode->i_cdev, struct vdev_info, cdev);
    file->private_data = info;
    return 0;
}

static int vdev_release(struct inode *inode, struct file *file) {
    return 0;
}
/* 
 * read(fd, buf, 256);
 * step 1: check if read is valid
 * step 2: check if length of data requested to read is feasible
 * step 3: check if device is ready for the operation
 * step 4: Program device and gather data
 * step 5: convert the data into application usable format(if needed)
 * step 6: Transfer data into user buffer
 * step 7: increment the filedescriptor offset position by number of bytes
 *         transferred to user buffer
 * step 8: return count of bytes transferred/success
 */

static ssize_t vdev_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
    struct vdev_info *info = file->private_data;
    if (!(info->permission & 0x1))
        return -EACCES;

    if (*ppos >= info->size)
        return 0;  // EOF

    if (*ppos + count > info->size)
        count = info->size - *ppos;

    if (copy_to_user(buf, info->devmem + *ppos, count))
        return -EFAULT;

    *ppos += count;
    return count;
}
/* write(fd, buf, 200);
 *
 * step 1: check if write is valid
 * step 2: check if length of data requested to be written are feasible
 * step 3: check if device is ready for the operation
 * step 4: Transfer data from user buffer into driver buffer(copy_from_user)
 * step 5: validate and covert data into device usable format(if required)
 * step 6: Program the device for the relevant operation
 * step 7: update the operation status to user
 * step 8: increment file descriptor offset by number of bytes
 *         transferred(optional)
 * step 9: return status
 *
 */


static ssize_t vdev_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos) {
    struct vdev_info *info = file->private_data;
    if (!(info->permission & 0x2))
        return -EACCES;

    if (*ppos >= info->size)
        return 0; // Cannot write beyond the allocated memory

    if (*ppos + count > info->size)
        count = info->size - *ppos;

    if (copy_from_user(info->devmem + *ppos, buf, count))
        return -EFAULT;

    *ppos += count;
    return count;
}

static loff_t vdev_lseek(struct file *file, loff_t offset, int whence) {
    struct vdev_info *info = file->private_data;
    loff_t new_pos = 0;

    switch (whence) {
        case SEEK_SET:
            new_pos = offset;
            break;
        case SEEK_CUR:
            new_pos = file->f_pos + offset;
            break;
        case SEEK_END:
            new_pos = info->size + offset;
            break;
        default:
            return -EINVAL;
    }

    if (new_pos < 0 || new_pos > info->size)
        return -EINVAL;

    file->f_pos = new_pos;
    return new_pos;
}

static struct file_operations vdev_fops = {
    .owner = THIS_MODULE,
    .open = vdev_open,
    .release = vdev_release,
    .read = vdev_read,
    .write = vdev_write,
    .llseek = vdev_lseek,
};
/* 
 * step 1: Gather device config data
 * step 2: Allocate/map resources
 * step 3: setup driver operations and register them with chosen kernel driver
 *         model.
 */

static int vdev_probe(struct platform_device *pdev) {
    struct vdev_info *info;
    struct device *dev = &pdev->dev;
    const char *serial;
    dev_t devt;
    int ret;

    info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
    if (!info)
        return -ENOMEM;

 if (of_property_read_u32(dev->of_node, "vdev-id", &info->device_id)) {
        dev_err(dev, "Failed to read vdev-id property\n");
        return -EINVAL;
   }

    if (of_property_read_u32(dev->of_node, "virt,size", &info->size) ||
        of_property_read_u32(dev->of_node, "virt,permission", &info->permission) ||
        of_property_read_string(dev->of_node, "virt,dev-serial", &serial)) {
        dev_err(dev, "Failed to read device properties\n");
        return -ENODEV;
    }

    strncpy(info->serial, serial, sizeof(info->serial) - 1);
    info->serial[sizeof(info->serial) - 1] = '\0';

    info->devmem = devm_kzalloc(dev, info->size, GFP_KERNEL);
    if (!info->devmem) {
        dev_err(dev, "Failed to allocate memory for device\n");
        return -ENOMEM;
    }
    if (major == 0){
	    ret = alloc_chrdev_region(&devt, 0, 2, DRIVER_NAME);
            if (ret < 0){
                dev_err(dev, "failed to grab major no\n");
		return ret;
	    }
	    major = MAJOR(devt);
	    dev_info(dev, "alloted unique major no: %d\n", major);
    }
    devt = MKDEV(major, info->device_id);
    cdev_init(&info->cdev, &vdev_fops);
    info->cdev.owner = THIS_MODULE;

    ret = cdev_add(&info->cdev, devt, 1);
    if (ret) {
        dev_err(dev, "Failed to add cdev\n");
        return ret;
    }
    if (vdev_class == NULL){
	vdev_class = class_create(CLASS_NAME);
		if (IS_ERR(vdev_class))
			return PTR_ERR(vdev_class);
     }

    device_create(vdev_class, dev, devt, NULL, "%s%d", DRIVER_NAME, info->device_id);
    dev_info(dev, "vdev device %s registered with ID %d\n", info->serial, info->device_id);

    dev_set_drvdata(&pdev->dev, info);
    return 0;
}

static int vdev_remove(struct platform_device *pdev) 
{

    struct vdev_info *info = dev_get_drvdata(&pdev->dev);
    dev_t devt;

    if (info) {
        devt = MKDEV(major, info->device_id);
        device_destroy(vdev_class, devt);
        cdev_del(&info->cdev);
    }

    return 0;
}
static const struct of_device_id vdev_dt_ids[] = {
    { .compatible = "vDev-Ax" },
    { .compatible = "vDev-Bx" },
    {  }
};

static struct platform_driver vdev_driver = {
    .driver = {
        .name = DRIVER_NAME,
        .of_match_table = vdev_dt_ids,
    },
    .probe = vdev_probe,
    .remove = vdev_remove,
};


static int __init vdev_init(void) {
    return platform_driver_register(&vdev_driver);
}

static void __exit vdev_exit(void) {
    dev_t devt;

    devt = MKDEV(major, 0);
    platform_driver_unregister(&vdev_driver);
    class_destroy(vdev_class);
    unregister_chrdev_region(devt, 2);
}

module_init(vdev_init);
module_exit(vdev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Raghu Bharadwaj<raghu@techveda.org>");
MODULE_DESCRIPTION("DT Node Device Driver");
