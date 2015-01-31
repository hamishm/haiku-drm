/*
 * Copyright (c) 2014 François Tigeot
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

#ifndef LINUX_PCI_H
#define LINUX_PCI_H


#include <bus/PCI.h>

#include <linux/device.h>
#include <linux/io.h>

#include <linux/pci_ids.h>


#define PCI_ANY_ID	(~0u)

struct pci_driver;

struct pci_dev {
	uint16 vendor;
	uint16 device;
	uint16 subvendor;
	uint16 subdevice;
	uint32 class;

	// Private
	struct pci_device* dev;
	struct pci_device_module_info* module;
	struct pci_device_id* matching_id;
	struct pci_driver* driver;
	struct device_node* node;
};


struct pci_device_id {
	uint16 vendor;
	uint16 device;
	uint16 subvendor;
	uint16 subdevice;
	uint32 class;
	uint32 class_mask;
};

#define PCI_DEVICE(vend, dev)	\
	.vendor = (vend),			\
	.device = (dev),			\
	.subvendor = PCI_ANY_ID,	\
	.subdevice = PCI_ANY_ID

#define PCI_DEVICE_SUB(vend, dev, subv, subd)	\
	.vendor = (vend),							\
	.device = (dev),							\
	.subvendor = (subv),						\
	.subdevice = (subd)

#define PCI_DEVICE_CLASS(cls, mask)	\
	.vendor = PCI_ANY_ID,			\
	.device = PCI_ANY_ID,			\
	.subvendor = PCI_ANY_ID,		\
	.device = PCI_ANY_ID,			\
	.class = (cls),					\
	.class_mask = (mask)


#define DEFINE_PCI_DEVICE_TABLE(table) \
	const struct pci_device_id table[]


struct pci_driver {
	struct list_head list;

	const char* name;
	const struct pci_device_id* id_table;
	int (*probe)(struct pci_dev* dev, const struct pci_device_id* id);
	int (*remove)(struct pci_dev* dev, const struct pci_device_id* id);

	/* Unused by DRM:
	 * int (*suspend)(struct pci_dev* dev, pm_message_t state);
	 * int (*suspend_late)(struct pci_dev* dev, pm_message_t state);
	 * int (*resume_early)(struct pci_dev* dev);
	 * int (*resume)(struct pci_dev* dev);
	 * void (*shutdown)(struct pci_dev* dev);
	 */

	struct device_driver driver;
};


extern int pci_register_driver(struct pci_driver* driver);


#define to_pci_driver(driver) container_of(driver, struct pci_driver, driver)


static inline int
pci_read_config_byte(struct pci_dev *pdev, int where, u8 *val)
{
	*val = (u16)pdev->module->read_pci_config(pdev->dev, where, 1);
	return 0;
}

static inline int
pci_read_config_word(struct pci_dev *pdev, int where, u16 *val)
{
	*val = (u16)pdev->module->read_pci_config(pdev->dev, where, 2);
	return 0;
}

static inline int
pci_read_config_dword(struct pci_dev *pdev, int where, u32 *val)
{
	*val = (u32)pdev->module->read_pci_config(pdev->dev, where, 4);
	return 0;
}

static inline int
pci_write_config_byte(struct pci_dev *pdev, int where, u8 val)
{
	pdev->module->write_pci_config(pdev->dev, where, val, 1);
	return 0;
}

static inline int
pci_write_config_word(struct pci_dev *pdev, int where, u16 val)
{
	pdev->module->write_pci_config(pdev->dev, where, val, 2);
	return 0;
}

static inline int
pci_write_config_dword(struct pci_dev *pdev, int where, u32 val)
{
	pdev->module->write_pci_config(pdev->dev, where, val, 4);
	return 0;
}


static inline struct pci_dev *
pci_dev_get(struct pci_dev *dev)
{
	/* Linux increments a reference count here */
	return dev;
}

static inline struct pci_dev *
pci_dev_put(struct pci_dev *dev)
{
	/* Linux decrements a reference count here */
	return dev;
}


static inline int
pci_set_dma_mask(struct pci_dev *dev, u64 mask)
{
	return -EIO;
}

static inline int
pci_set_consistent_dma_mask(struct pci_dev *dev, u64 mask)
{
	return -EIO;
}

#endif /* LINUX_PCI_H */
