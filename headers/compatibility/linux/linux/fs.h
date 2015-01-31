/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013 Mellanox Technologies, Ltd.
 * Copyright (c) 2015 Hamish Morrison
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
#ifndef	_LINUX_FS_H_
#define	_LINUX_FS_H_

#include <linux/types.h>

struct inode;
struct module;
struct vm_area_struct;
struct poll_table_struct;


#define	S_IRUGO	(S_IRUSR | S_IRGRP | S_IROTH)
#define	S_IWUGO	(S_IWUSR | S_IWGRP | S_IWOTH)


struct file_operations;

struct file {
	const struct file_operations* f_op;
	void* private_data;
	int f_flags;
	int f_mode;

	// Private
	struct inode* inode;
};

struct inode {
	umode_t i_mode;
	unsigned int i_flags;
	dev_t i_rdev;
	union {
		struct cdev* i_cdev;
	};
	void* i_private;
};


struct file_operations {
	struct module* owner;

	int (*open)(struct inode *, struct file *);
	int (*release)(struct inode *, struct file *);
	ssize_t (*read)(struct file *, char __user *, size_t, loff_t *);
	ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *);

	long (*unlocked_ioctl)(struct file *, unsigned int, unsigned long);
	loff_t (*llseek)(struct file *, loff_t, int);
	unsigned int (*poll) (struct file *, struct poll_table_struct *);
	int (*mmap)(struct file *, struct vm_area_struct *);
};

#define	fops_get(fops) (fops)
#define replace_fops(file, fops) ((file)->f_op = (fops))


#define	FMODE_READ	FREAD
#define	FMODE_WRITE	FWRITE
#define	FMODE_EXEC	FEXEC


extern int register_chrdev(unsigned int major, const char* name,
	const struct file_operations* ops);
extern void unregister_chrdev(unsigned int major, const char* name);

static inline int
register_chrdev_region(dev_t dev, unsigned range, const char*name)
{
	return 0;
}

static inline void
unregister_chrdev_region(dev_t dev, unsigned range)
{
	return;
}

static inline int
alloc_chrdev_region(dev_t* dev, unsigned baseminor, unsigned count,
	const char* name)
{
	// TODO
	return 0;
}

static inline dev_t iminor(struct inode* inode)
{
	return MINOR(inode->i_rdev);
}

static inline dev_t imajor(struct inode* inode)
{
	return MAJOR(inode->i_rdev);
}

static inline struct inode* igrab(struct inode* inode)
{
	return inode;
}

static inline void iput(struct inode* inode)
{
}

#endif /* _LINUX_FS_H_ */
