/*
 * Copyright 2015, Hamish Morrison, hamishm53@gmail.com.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef _WW_MUTEX_H
#define _WW_MUTEX_H

#include <debug.h>
#include <KernelExport.h>
#include <SupportDefs.h>

#include <linux/sched.h>
#include <linux/wait.h>


#define __WW_UNLOCKED	0
#define __WW_LOCKED		1
#define __WW_CONTENDED	2


struct ww_class {
	int64 ticket;
};

struct ww_acquire_ctx {
	struct ww_class* class;
	int64 ticket;
	unsigned acquired;
};

struct ww_mutex {
	int32 locked;
	spinlock lock;
	struct ww_acquire_ctx* context;
	struct ww_class* class;
	wait_queue_head_t waiters;
};


static void ww_acquire_init(struct ww_acquire_ctx* context, struct ww_class* class)
{
	context->class = class;
	context->ticket = atomic_add64(&class->ticket, 1);
	context->acquired = 0;
}

static void ww_acquire_done(struct ww_acquire_ctx* context)
{
}

static void ww_acquire_fini(struct ww_acquire_ctx* context)
{
}


static void ww_mutex_init(struct ww_mutex* mutex, struct ww_class* class)
{
	B_INITIALIZE_SPINLOCK(&mutex->lock);
	init_waitqueue_head(&mutex->waiters);
	mutex->class = class;
}

static void ww_mutex_destroy(struct ww_mutex* mutex)
{
}

static bool ww_mutex_is_locked(struct ww_mutex* mutex)
{
	return mutex->locked > 0;
}

static int ww_mutex_trylock(struct ww_mutex* mutex)
{
	return atomic_test_and_set(&mutex->locked, __WW_LOCKED, __WW_UNLOCKED)
		== __WW_UNLOCKED;
}

extern int __ww_mutex_lock(struct ww_mutex* mutex, struct ww_acquire_ctx* context, int state);

static int
ww_mutex_lock(struct ww_mutex* mutex, struct ww_acquire_ctx* context)
{
	if (context == NULL && ww_mutex_trylock(mutex))
		return 1;

	return __ww_mutex_lock(mutex, context, TASK_UNINTERRUPTIBLE);
}

static int
ww_mutex_lock_interruptible(struct ww_mutex* mutex, struct ww_acquire_ctx* context)
{
	if (context == NULL && ww_mutex_trylock(mutex))
		return 1;

	return __ww_mutex_lock(mutex, context, TASK_INTERRUPTIBLE);
}

extern int __ww_mutex_lock_slow(struct ww_mutex* mutex, struct ww_acquire_ctx* context, int state);


static void
ww_mutex_lock_slow(struct ww_mutex* mutex, struct ww_acquire_ctx* context)
{
	ASSERT(context != NULL && context->acquired == 0);
	__ww_mutex_lock(mutex, context, TASK_UNINTERRUPTIBLE);
}

static void
ww_mutex_lock_slow_interruptible(struct ww_mutex* mutex, struct ww_acquire_ctx* context)
{
	ASSERT(context != NULL && context->acquired == 0);
	__ww_mutex_lock(mutex, context, TASK_INTERRUPTIBLE);
}


extern void ww_mutex_unlock(struct ww_mutex* mutex);

#endif
