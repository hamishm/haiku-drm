/*
 * Copyright 2015, Hamish Morrison, hamishm53@gmail.com.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef _LINUX_DRIVER_H
#define _LINUX_DRIVER_H

#include <device_manager.h>


// TODO!
#define LINUX_PCI_DRIVER_MODULE "drivers/graphics/drm/pci_driver_v1"
#define LINUX_DEVICE_DRIVER_MODULE "drivers/graphics/drm/dev_driver_v1"
#define LINUX_CHAR_DEVICE_MODULE "drivers/graphics/drm/char_device_v1"

static const char* LINUX_DEVICE_MAJOR = "linux/major";
static const char* LINUX_DEVICE_MINOR = "linux/minor";

extern const struct driver_module_info linux_pci_driver;
extern const struct driver_module_info linux_device_driver;
extern const struct device_module_info linux_char_device;

extern struct device_manager_info* dev_manager;


#endif
