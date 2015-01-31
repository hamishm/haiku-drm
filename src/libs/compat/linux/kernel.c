/*
 * Copyright 2015, Hamish Morrison, hamishm53@gmail.com.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#include <linux/kernel.h>
#include <linux/slab.h>

#include <stdio.h>

#include <heap.h>


char *kvasprintf(gfp_t gfp, const char *fmt, va_list ap)
{
	unsigned int len;
	char *p = NULL;
	va_list aq;

	__va_copy(aq, ap);
	len = vsnprintf(NULL, 0, fmt, aq);
	va_end(aq);

	if (len < 0)
		return NULL;

	p = (char*)kmalloc(len + 1, gfp);
	if (p == NULL)
		return NULL;

	vsnprintf(p, len + 1, fmt, ap);

	return p;
}

static inline char *kasprintf(gfp_t gfp, const char *fmt, ...)
{
	va_list ap;
	char *p;

	va_start(ap, fmt);
	p = kvasprintf(gfp, fmt, ap);
	va_end(ap);

	return p;
}



