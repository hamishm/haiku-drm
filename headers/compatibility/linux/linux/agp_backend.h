/*
 * Copyright 2015, Hamish Morrison, hamishm53@gmail.com.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef _LINUX_AGP_BACKEND_H
#define _LINUX_AGP_BACKEND_H

#include <SupportDefs.h>

enum chipset_type {
	NOT_SUPPORTED,
	SUPPORTED
};

struct agp_version {
	uint16 major;
	uint16 minor;
};

struct agp_kern_info {
	struct agp_version version;
	struct pci_dev* dev;
	enum chipset_type chipset;
	unsigned long mode;
	unsigned long aper_base;
	size_t aper_size;
	int max_memory;
	int current_memory;
	bool cant_use_aperture;
	unsigned long page_mask;
	// const struct vm_operations_struct* vm_ops;
};

struct agp_bridge_data;

struct agp_memory {
};

#endif
