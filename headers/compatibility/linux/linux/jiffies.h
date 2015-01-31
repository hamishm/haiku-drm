/*
 * Copyright (c) 2014-2015 François Tigeot
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

#ifndef _LINUX_JIFFIES_H_
#define _LINUX_JIFFIES_H_

#include <linux/math64.h>
#include <linux/time.h>

#define HZ 1000000

#define jiffies_to_msecs(x)	(((int64_t)(x)) * 1000 / HZ)
#define msecs_to_jiffies(x)	(((int64_t)(x)) * HZ / 1000)
#define jiffies			ticks
#define time_after(a,b)		((long)(b) - (long)(a) < 0)
#define time_after_eq(a,b)	((long)(b) - (long)(a) <= 0)

static inline unsigned long
timespec_to_jiffies(const struct timespec *ts)
{
	unsigned long result;

	result = ((unsigned long)HZ * ts->tv_sec) + (ts->tv_nsec / NSEC_PER_SEC);
	if (result > LONG_MAX)
		result = LONG_MAX;

	return result;
}

static inline
unsigned long usecs_to_jiffies(const unsigned int u)
{
	unsigned long result;

	result = (u * HZ) / 1000000;
	if (result < 1)
		return 1;
	else
		return result;
}

#endif	/* _LINUX_JIFFIES_H_ */
