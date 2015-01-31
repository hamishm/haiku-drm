/*
 * Copyright (c) 2015 Hamish Morrison
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
#ifndef	_LINUX_COMPLETION_H_
#define	_LINUX_COMPLETION_H_

#include <linux/wait.h>

struct completion {
	unsigned int done;
	wait_queue_head_t wait;
};

static inline void
init_completion(struct completion *c)
{
	c->done = 0;
	init_waitqueue_head(&c->wait);
}

#define	INIT_COMPLETION(c)	(c.done = 0)

static inline void
complete(struct completion *c)
{
	acquire_spinlock(&c->wait.lock);
	c->done++;
	release_spinlock(&c->wait.lock);
	wake_up(&c->wait);
}

static inline void
complete_all(struct completion *c)
{
	acquire_spinlock(&c->wait.lock);
	c->done++;
	release_spinlock(&c->wait.lock);
	wake_up_all(&c->wait);
}

extern long __wait_for_completion(struct completion* c, int state, long timeout);

#define wait_for_completion(c)							\
	__wait_for_completion(c, TASK_UNINTERRUPTIBLE, MAX_SCHEUDLE_TIMEOUT)
#define wait_for_completion_interruptible(c) 			\
	__wait_for_completion(c, TASK_INTERRUPTIBLE, MAX_SCHEUDLE_TIMEOUT)
#define wait_for_completion_timeout(c, t)				\
	__wait_for_completion(c, TASK_UNINTERRUPTIBLE, t)
#define wait_for_completion_interruptible_timeout(c, t)	\
	__wait_for_completion(c, TASK_INTERRUPTIBLE, t)

#endif	/* _LINUX_COMPLETION_H_ */
