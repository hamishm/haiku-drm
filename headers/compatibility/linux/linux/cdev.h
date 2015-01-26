/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013, 2014 Mellanox Technologies, Ltd.
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

#ifndef	_LINUX_CDEV_H_
#define	_LINUX_CDEV_H_

#include <linux/kobject.h>
#include <linux/kdev_t.h>
#include <linux/list.h>

struct file_operations;
struct inode;
struct module;

struct cdev {
	struct kobject kobj;
	struct module* owner;
	const struct file_operations* ops;

	dev_t dev;
	unsigned count;
	struct list_head entry;
};


extern int cdev_add(struct cdev* cdev, dev_t dev, unsigned count);


static inline void
cdev_release(struct kobject *kobj)
{
	struct cdev* cdev;

	cdev = container_of(kobj, struct cdev, kobj);
	kfree(cdev);
}

static inline void
cdev_static_release(struct kobject *kobj)
{
}

static struct kobj_type cdev_ktype = {
	.release = cdev_release,
};

static struct kobj_type cdev_static_ktype = {
	.release = cdev_static_release,
};

static inline void
cdev_init(struct cdev* cdev, const struct file_operations *ops)
{
	kobject_init(&cdev->kobj, &cdev_static_ktype);
	cdev->ops = ops;
}

static inline struct cdev*
cdev_alloc(void)
{
	struct cdev* cdev;

	cdev = kzalloc(sizeof(struct cdev), GFP_KERNEL);
	if (cdev != NULL)
		kobject_init(&cdev->kobj, &cdev_ktype);
	return cdev;
}

static inline void
cdev_put(struct cdev *p)
{
	kobject_put(&p->kobj);
}



static inline void
cdev_del(struct cdev* cdev)
{
	kobject_put(&cdev->kobj);
}


#endif	/* _LINUX_CDEV_H_ */
