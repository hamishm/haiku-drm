/*
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013, 2014 Mellanox Technologies, Ltd.
 * Copyright (c) 2015, Hamish Morrison
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/pci.h>
#include <linux/slab.h>

#include <debug.h>
#include <driver.h>

#define ALL_MINORS 255


static struct list_head cdev_list = LIST_HEAD_INIT(cdev_list);


static void
dev_release(struct device *dev)
{
	kprintf("dev_release: %s\n", dev_name(dev));
	kfree(dev);
}


void
device_initialize(struct device* dev)
{
	kobject_init(&dev->kobj, &dev_ktype);
}


struct device*
device_create(struct class* class, struct device* parent, dev_t devt,
    void* drvdata, const char* fmt, ...)
{
	struct device *dev;
	va_list args;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	device_initialize(dev);
	dev->parent = parent;
	dev->class = class;
	dev->devt = devt;
	dev->driver_data = drvdata;
	dev->release = dev_release;
	va_start(args, fmt);
	kobject_set_name_vargs(&dev->kobj, fmt, args);
	va_end(args);
	device_register(dev);

	return dev;
}


/*
 * Devices are registered and created for exporting to sysfs.  create
 * implies register and register assumes the device fields have been
 * setup appropriately before being called.
 */
int
device_register(struct device *dev)
{
	status_t status;

	device_attr attrs[] = {
		{B_DEVICE_PRETTY_NAME, B_STRING_TYPE,
			{string: dev_name(dev)}
		},
		{LINUX_DEVICE_MAJOR, B_UINT32_TYPE,
			{ui32: MAJOR(dev->devt)}
		},
		{LINUX_DEVICE_MINOR, B_UINT32_TYPE,
			{ui32: MINOR(dev->devt)}
		},
		{NULL}
	};

	status = dev_manager->register_node(dev->parent->node,
		LINUX_DEVICE_DRIVER_MODULE, attrs, NULL, &dev->node);
	if (status != B_OK)
		return status;

	kobject_add(&dev->kobj, &dev->class->kobj, dev_name(dev));
	return B_OK;
}


void
device_unregister(struct device *dev)
{
	status_t status = dev_manager->unregister_node(dev->node);
	ASSERT(status == B_OK);
}


static inline bool
cdev_matches(struct cdev* cdev, dev_t dev)
{
	return MAJOR(dev) == MAJOR(cdev->dev)
		&& MINOR(dev) >= MINOR(cdev->dev)
		&& MINOR(dev) < MINOR(cdev->dev) + cdev->count;
}


static struct cdev*
cdev_find_device(dev_t dev)
{
	struct cdev* cdev;
	list_for_each_entry(cdev, &cdev_list, entry) {
		if (cdev_matches(cdev, dev))
			return cdev;
	}
	return NULL;
}


int
cdev_add(struct cdev* cdev, dev_t dev, unsigned count)
{
	list_add_tail(&cdev->entry, &cdev_list);
	return 0;
}


int register_chrdev(unsigned int major, const char* name,
	const struct file_operations* ops)
{
	struct cdev* cdev = cdev_alloc();
	if (cdev == NULL)
		return -ENOMEM;

	cdev->owner = ops->owner;
	cdev->ops = ops;
	kobject_set_name(&cdev->kobj, name);

	cdev_add(cdev, MKDEV(major, 0), ALL_MINORS);
	return 0;
}


void unregister_chrdev(unsigned int major, const char* name)
{
	struct cdev *cdev, *temp;
	list_for_each_entry_safe(cdev, temp, &cdev_list, entry) {
		if (MAJOR(cdev->dev) == major && cdev->count == ALL_MINORS) {
			cdev_del(cdev);
			list_del_init(&cdev->entry);
		}
	}
}


// #pragma mark --

static struct inode*
get_device_inode(dev_t dev)
{
	struct inode* inode;
	struct cdev* cdev = cdev_find_device(dev);
	if (cdev == NULL)
		return NULL;

	inode = kzalloc(sizeof(inode), GFP_KERNEL);
	if (inode == NULL)
		return NULL;

	inode->i_mode = S_IRUGO | S_IWUGO;
	inode->i_flags = 0;
	inode->i_rdev = dev;
	inode->i_cdev = cdev;
	inode->i_private = NULL;
	return inode;
}

static status_t
linux_device_init_driver(device_node* node, void** _driverCookie)
{
	struct inode* inode;
	uint32 major;
	uint32 minor;

	status_t status = dev_manager->get_attr_uint32(node, LINUX_DEVICE_MAJOR,
		&major, false);
	if (status != B_OK)
		return status;

	status = dev_manager->get_attr_uint32(node, LINUX_DEVICE_MINOR, &minor,
		false);
	if (status != B_OK)
		return status;

	inode = get_device_inode(MKDEV(major, minor));
	if (inode == NULL)
		return B_ERROR;

	*_driverCookie = (void*)inode;
	return B_OK;
}

static void
linux_device_uninit_driver(void* driverCookie)
{
	struct inode* inode = (struct inode*)driverCookie;
	kfree(inode);
}


static status_t
linux_device_init_device(void* driverCookie, void** _deviceCookie)
{
	*_deviceCookie = driverCookie;
	return B_OK;
}

static void
linux_device_uninit_device(void* deviceCookie)
{
}


const struct driver_module_info linux_device_driver = {
	{
		LINUX_DEVICE_DRIVER_MODULE,
		0,
		NULL
	},

	NULL,
	NULL,
	linux_device_init_driver,
	linux_device_uninit_driver,
	NULL
};






static status_t
linux_device_open(void* deviceCookie, const char* path, int openMode,
	void** _cookie)
{
	*_cookie = deviceCookie;
	return B_OK;
}

static status_t
linux_device_close(void* cookie)
{
	return B_OK;
}

static status_t
linux_device_free(void* cookie)
{
	return B_OK;
}

static status_t
linux_device_read(void* cookie, off_t position, void* buffer,
	size_t* _length)
{
	return B_BAD_VALUE;
}

static status_t
linux_device_write(void* cookie, off_t position, const void* data,
	size_t* _length)
{
	return B_BAD_VALUE;
}

static status_t
linux_device_control(void* cookie, uint32 op, void* buffer,
	size_t length)
{
	return B_BAD_VALUE;
}

static status_t
linux_device_select(void* cookie, uint8 event, selectsync* sync)
{
	return B_BAD_VALUE;
}

static status_t
linux_device_deselect(void* cookie, uint8 event, selectsync* sync)
{
	return B_BAD_VALUE;
}




const struct device_module_info linux_char_device = {
	{
		LINUX_CHAR_DEVICE_MODULE,
		0,
		NULL
	},

	linux_device_init_device,
	linux_device_uninit_device,
	NULL,

	linux_device_open,
	linux_device_close,
	linux_device_free,

	linux_device_read,
	linux_device_write,
	NULL,	// io

	linux_device_control,
	linux_device_select,
	linux_device_deselect
};
