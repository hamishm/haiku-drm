/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2014 François Tigeot
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
#ifndef _LINUX_TIMER_H_
#define _LINUX_TIMER_H_

#include <KernelExport.h>
#include <linux/types.h>

#define TIMER_STATE_INACTIVE 0
#define TIMER_STATE_ACTIVE 1
#define TIMER_STATE_FIRED 2

struct timer_list {
	struct timer event;
	void (*function)(unsigned long);
	unsigned long data;
	unsigned long expires;
	int state;
};

static inline int32
_timer_fn(timer* event)
{
	struct timer_list *timer = (struct timer_list *)event->user_data;
	timer->state = TIMER_STATE_FIRED;
	timer->function(timer->data);
	return 0;
}

#define	setup_timer(timer, func, dat)				\
do {												\
	(timer)->function = (func);						\
	(timer)->data = (dat);							\
	(timer)->state = TIMER_STATE_INACTIVE;			\
	(timer)->event.user_data = (timer);				\
} while (0)

#define	init_timer(timer)							\
do {												\
	(timer)->function = NULL;						\
	(timer)->data = 0;								\
	(timer)->state = TIMER_STATE_INACTIVE;			\
	(timer)->event.user_data = (timer);				\
} while (0)

#define	mod_timer(timer, exp)						\
do {												\
	(timer)->expires = (exp);						\
	if ((timer)->state == TIMER_STATE_ACTIVE)		\
		cancel_timer(&(timer)->event);				\
	add_timer(&(timer)->event, _timer_fn, (exp),	\
		B_ONE_SHOT_RELATIVE_TIMER);					\
	(timer)->state = TIMER_STATE_ACTIVE;			\
} while (0)

#define	add_timer(timer)							\
do {												\
	add_timer(&(timer)->event, _timer_fn, (timer)->expires,	\
		B_ONE_SHOT_RELATIVE_TIMER);					\
	(timer)->state = TIMER_STATE_ACTIVE;			\
} while (0)

static inline void
del_timer(struct timer_list *timer)
{
	cancel_timer(&(timer)->event);
	(timer)->state = TIMER_STATE_INACTIVE;
}

#define	del_timer_sync(timer) del_timer(timer)

#define	timer_pending(timer) ((timer)->state != TIMER_STATE_FIRED)

static inline unsigned long
round_jiffies(unsigned long j)
{
	return j;
}

static inline unsigned long
round_jiffies_up(unsigned long j)
{
	return j;
}

static inline unsigned long
round_jiffies_up_relative(unsigned long j)
{
	return j;
}

#endif /* _LINUX_TIMER_H_ */
