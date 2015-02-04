/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013, 2014 Mellanox Technologies, Ltd.
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
#ifndef	_LINUX_SLAB_H_
#define	_LINUX_SLAB_H_

#include <heap.h>
#include <slab/Slab.h>

#include <linux/types.h>
#include <linux/gfp.h>



static inline void* kmalloc(size_t size, uint32 flags)
{
	void* addr = malloc_etc(size, flags);
	if (addr != NULL && (flags & __GFP_ZERO) != 0)
		memset(addr, 0, size);

	return addr;
}


#define PAGE_KERNEL 0x0001


#define	kvmalloc(size)					kmalloc((size), 0)
#define	kzalloc(size, flags)			kmalloc((size), (flags) | __GFP_ZERO)
#define	kzalloc_node(size, flags, node)	kzalloc(size, flags)
#define	kfree(ptr)						free((ptr))
#define	krealloc(ptr, size, flags)		realloc((ptr), (size))
#define	kcalloc(n, size, flags)	    	kmalloc((n) * (size), (flags) | __GFP_ZERO)
#define	vzalloc(size)					kzalloc((size), GFP_KERNEL)
#define	vfree(arg)						kfree((arg))
#define	kvfree(arg)						kfree((arg))
#define	vmalloc(size)					kmalloc((size), GFP_KERNEL)
#define	vmalloc_node(size, node)		kmalloc((size), GFP_KERNEL)

// XXX
#define __vmalloc(size, flags, prot)	kmalloc((size), (flags))
#define is_vmalloc_addr(ptr) 			(true)


struct kmem_cache {
	object_cache* cache;
};


#define	SLAB_HWCACHE_ALIGN	0x0001


static inline status_t
kmem_ctor(void* cookie, void* object)
{
	void (*ctor)(void *) = cookie;
	ctor(object);
	return 0;
}


static inline struct kmem_cache *
kmem_cache_create(char *name, size_t size, size_t align, u_long flags,
    void (*ctor)(void *))
{
	struct kmem_cache *c = (struct kmem_cache*)malloc(sizeof(struct kmem_cache));

	c->cache = create_object_cache(name, size, align,
		ctor != NULL ? (void*)kmem_ctor : NULL, kmem_ctor, NULL);
	return c;
}

static inline void *
kmem_cache_alloc(struct kmem_cache *c, int flags)
{
	return object_cache_alloc(c->cache, flags);
}

static inline void
kmem_cache_free(struct kmem_cache *c, void *m)
{
	object_cache_free(c->cache, m, 0);
}

static inline void
kmem_cache_destroy(struct kmem_cache *c)
{
	delete_object_cache(c->cache);
	free(c);
}

#endif	/* _LINUX_SLAB_H_ */
