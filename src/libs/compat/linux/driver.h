/*
 * Copyright 2015, Hamish Morrison, hamishm53@gmail.com.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef _LINUX_DRIVER_H
#define _LINUX_DRIVER_H

#include <device_manager.h>


// TODO!
static const char* LINUX_PCI_DRIVER_MODULE = "drivers/graphics/drm/pci_driver_v1";
static const char* LINUX_DEV_DRIVER_MODULE = "drivers/graphics/drm/dev_driver_v1";
static const char* LINUX_CHAR_DEVICE_MODULE = "drivers/graphics/drm/char_device_v1";

static const char* LINUX_DEVICE_MAJOR = "linux/major";
static const char* LINUX_DEVICE_MINOR = "linux/minor";

extern const driver_module_info linux_compat_pci_driver;
extern const driver_module_info linux_compat_dev_driver;
extern const device_module_info linux_compat_char_device;

extern device_manager_info* dev_manager;


#endif
