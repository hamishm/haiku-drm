/*
 * Copyright 2015, Hamish Morrison, hamishm53@gmail.com.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#include <linux/pci.h>
#include <linux/list.h>


device_manager_info* dev_manager;

static const char* LINUX_DRIVER_MODULE = "drivers/graphics/drm/device_v1"


static struct list_head pci_drivers = LIST_HEAD_INIT(&pci_drivers);


static bool
id_matches(struct pci_device_id* id, struct pci_dev* device)
{
	return (id->vendor == PCI_ANY_ID || id->vendor == device->vendor)
		&& (id->device == PCI_ANY_ID || id->device == device->device)
		&& (id->subvendor == PCI_ANY_ID || id->subvendor == device->subvendor)
		&& (id->subdevice == PCI_ANY_ID || id->subdevice == device->subdevice)
		&& (id->class == 0 || id->class == (device->class & id->class_mask)
}

static pci_device_id*
driver_matches(struct pci_driver* driver, struct pci_dev* device)
{
	if (driver->id_table == NULL)
		return false;

	struct pci_device_id* id = driver->id_table;
	while (id->vendor != 0) {
		if (id_matches(id, device))
			return id;

		id++;
	}
	return NULL;		
}

static pci_driver*
find_driver(struct pci_dev* device)
{
	struct pci_driver* driver;

	list_for_each_entry(driver, &pci_drivers, list) {
		struct pci_device_id* id = driver_matches(driver, device);
		if (id != NULL) {
			device->matching_id = id;
			device->driver = driver;
			return driver;
		}
	}

	return NULL;
}


int
pci_register_driver(struct pci_driver* driver)
{
	list_add_tail(&driver->list, &pci_devices);
}


// #pragma mark --


static pci_dev* pci_device_for_node(device_node* node)
{
	struct pci_dev* dev = malloc(sizeof(pci_dev));
	if (dev == NULL)
		return NULL;

	status_t status =  dev_manager->get_driver(node,
		&(module_info*)dev->module, &(void*)dev->device);
	if (status != B_OK)
		goto error;

	struct pci_info* info;
	dev->module->pci_device_get_pci_info(dev->device, &info);

	dev->vendor = info->vendor_id;
	dev->device = info->device_id;
	dev->subvendor = info->u.h0.subsystem_vendor_id;
	dev->subdevice = info->u.h0.subsystem_id;
	dev->class = info->class_base << 16 | info->class_sub << 8 | info->class_api;
	dev->node = node;

	return dev;

error:
	free(dev);
	return NULL;	
}

static void pci_device_free(struct pci_dev* dev)
{
	free(dev);
}


static float
linux_driver_supports_device(device_node* parent)
{
	const char* bus = NULL;
	if (dev_manager->get_attr_string(parent, B_DEVICE_BUS, &bus, false) != B_OK)
		return -1;

	if (strcmp(bus, "pci") != 0)
		return -1;

	struct pci_dev* dev = pci_device_for_node(parent);
	if (dev == NULL)
		return -1;

	struct pci_driver* driver = find_driver(dev);
	pci_device_free(dev);

	if (driver == NULL)
		return -1;
	else
		return 1.0;
}


static status_t
linux_driver_register_device(device_node* parent)
{
	device_attr attrs[] = {
		{B_DEVICE_PRETTY_NAME, B_STRING_TYPE,
			{string: "Linux Compatibility Driver"}},
		{NULL}
	};

	return dev_manager->register_node(parent, LINUX_DRIVER_MODULE, attrs, NULL,
		NULL);
}


static status_t
linux_driver_init_driver(device_node* node, void** _driverCookie)
{
	uint32 major;
	uint32 minor;
	if (dev_manager->get_attr_uint32(node, LINUX_DEVICE_MAJOR, &major, false)
			== B_OK
		&& dev_manager->get_attr_uint32(node, LINUX_DEVICE_MINOR, &minor,
			false) == B_OK) {
		*driverCookie = (void*)MKDEV(major, minor);
		return B_OK;
	}
		
	device_node* parent = dev_manager->get_parent(node);
	if (parent == NULL)
		return B_ERROR;

	struct pci_dev* dev = pci_device_for_node(parent);
	dev_manager->put_node(parent);  // can do?

	if (dev == NULL)
		return B_ERROR;

	*_driverCookie = (void*)dev;
	return B_OK;
}


static void
linux_driver_uninit_driver(void* driverCookie)
{
	if ((uintptr_t)driverCookie & 0x80000000U) {
		pci_device_free((struct pci_dev*)driverCookie);
	}
}


static status_t
linux_driver_register_child_devices(void* driverCookie)
{
	struct pci_dev* dev = (struct pci_dev*)driverCookie;
	return dev->probe(dev, dev->found_id);
}


const struct driver_module_info linux_compat_pci_driver = {
	{
		LINUX_PCI_DRIVER_MODULE,
		0,
		NULL
	},

	linux_driver_supports_device,
	linux_driver_register_device,
	linux_driver_init_driver,
	linux_driver_uninit_driver,
	linux_driver_register_child_devices
};


module_dependency module_dependencies[] = {
	{B_DEVICE_MANAGER_MODULE_NAME, (module_info**)&dev_manager},
	{}
};


const module_info* modules[] = {
	(module_info*)&linux_compat_pci_driver,
	(module_info*)&linux_compat_dev_driver,
	(module_info*)&linux_compat_device
	NULL
};

