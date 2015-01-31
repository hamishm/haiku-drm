/*
 * Copyright (c) 2015 Hamish Morrison
 * Copyright (c) 2014-2015 François Tigeot
 * Copyright (c) 2014 Imre Vadász
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

#ifndef _LINUX_WAIT_H_
#define _LINUX_WAIT_H_

#include <KernelExport.h>
#include <linux/list.h>


#define WQ_FLAG_EXCLUSIVE 0x01

struct _wait_queue;


typedef bool (*wait_queue_func_t)(struct _wait_queue* entry, unsigned int mode,
	int flags, void* key);


typedef struct _wait_queue {
	struct list_head entry;
	wait_queue_func_t func;
	void* thread;
	int flags;
} wait_queue_t;

typedef struct {
	spinlock lock;
	struct list_head waiters;
} wait_queue_head_t;

	
static void waitqueue_lock(wait_queue_head_t* queue)
{
	acquire_spinlock(&queue->lock);
}

static void waitqueue_unlock(wait_queue_head_t* queue)
{
	release_spinlock(&queue->lock);
}


/*
extern int default_wake_function(wait_queue_t* entry, unsigned int mode,
	int flags, void* key);
*/

extern bool autoremove_wake_function(wait_queue_t* wait, unsigned int mode, int sync, void* key);

extern void __waitqueue_wake_locked(wait_queue_head_t* queue, int count,
	unsigned int mode, void* key);

extern void __waitqueue_prepare_locked(wait_queue_head_t* queue, wait_queue_t* waiter,
	int state, bool exclusive);


static void
__waitqueue_wake(wait_queue_head_t* queue, int count, unsigned int mode,
	void* key)
{
	waitqueue_lock(queue);
	__waitqueue_wake_locked(queue, count, mode, key);
	waitqueue_unlock(queue);
}


static void
__finish_wait_locked(wait_queue_t* waiter)
{
	if (!list_empty(&waiter->entry))
		list_del_init(&waiter->entry);
}

static void
finish_wait(wait_queue_head_t* queue, wait_queue_t* waiter)
{
	waitqueue_lock(queue);
	__finish_wait_locked(waiter);
	waitqueue_unlock(queue);
}

static void
__waitqueue_prepare(wait_queue_head_t* queue, wait_queue_t* waiter, int state,
	bool exclusive)
{
	waitqueue_lock(queue);
	__waitqueue_prepare_locked(queue, waiter, state, exclusive);
	waitqueue_unlock(queue);
}




#define prepare_to_wait(q, w, s) __waitqueue_prepare((q), (w), (s), false)
#define prepare_to_wait_exclusive(q, w, s) __waitqueue_prepare((q), (w), (s), true)

#define wake_up(q)						__waitqueue_wake(q, 1, 0, NULL)
#define wake_up_nr(q, n)				__waitqueue_wake(q, n, 0, NULL)
#define wake_up_all(q)					__waitqueue_wake(q, 0, 0, NULL)
#define wake_up_interruptible(q)		__waitqueue_wake(q, 1, TASK_INTERRUTIPLE, NULL)
#define wake_up_interruptible_nr(q, n)	__waitqueue_wake(q, n, TASK_INTERRUTIPLE, NULL)
#define wake_up_interruptible_all(q)	__waitqueue_wake(q, 0, TASK_INTERRUTIPLE, NULL)



/*
extern void abort_exclusive_wait(wait_queue_head_t* queue, wait_queue_t* waiter,
	unsigned int mode, void* key);
*/



static inline void
init_waitqueue_head(wait_queue_head_t* queue)
{
	B_INITIALIZE_SPINLOCK(&queue->lock);
	INIT_LIST_HEAD(&queue->waiters);
}

#define DECLARE_WAIT_QUEUE_HEAD(queue) 				\
	wait_queue_head_t queue = {						\
		.lock = B_SPINLOCK_INITIALIZER,				\
		.waiters = LIST_HEAD_INIT(&(queue).waiters)	\
	}

#define DEFINE_WAIT_FUNC(wait, fn)					\
	wait_queue_t (wait) = {							\
		.entry = LIST_HEAD_INIT((wait).entry),		\
		.func = (fn),								\
		.thread = NULL								\
	}

#define DEFINE_WAIT(wait) DEFINE_WAIT_FUNC(wait, autoremove_wake_function)


static inline void
init_wait(wait_queue_t* waiter)
{
	INIT_LIST_HEAD(&waiter->entry);
	waiter->func = autoremove_wake_function;
	waiter->thread = NULL;
}



static inline int
waitqueue_active(wait_queue_head_t* queue)
{
	return !list_empty(&queue->waiters);
}


#define __wait_event(wq, cond, flags, excl)					\
({															\
	int ret = 0;											\
															\
	DEFINE_WAIT(wait);										\
	while (1) {												\
		if (signal_pending_state(flags, current)) {			\
			ret = -ERESTARTSYS;								\
			break;											\
		}													\
		__waitqueue_prepare((wq), wait, (flags), (excl));	\
		if (cond)											\
			break;											\
															\
		schedule();											\
	}														\
	finish_wait((wq), wait);								\
	ret;													\
})



#define __wait_event_timeout(wq, cond, flags, excl, timeout)	\
({																\
	long ret = timeout;											\
																\
	DEFINE_WAIT(wait);											\
	while (1) {													\
		if (signal_pending_state(flags, current)) {				\
			ret = -ERESTARTSYS;									\
			break;												\
		}														\
		__waitqueue_prepare((wq), wait, (flags), (excl));		\
		if (cond)												\
			break;												\
																\
		ret = schedule_timeout(ret);							\
		if (ret <= 0)											\
			break;												\
	}															\
	finish_wait((wq), wait);									\
	ret;														\
})


#define wait_event(wq, cond)											\
		__wait_event(wq, cond, 0, false)

#define wait_event_interruptible(wq, cond)								\
		__wait_event(wq, cond, TASK_INTERRUPTIBLE, false)

#define wait_event_timeout(wq, cond, timeout)							\
		__wait_event_timeout(wq, cond, 0, false, timeout)

#define wait_event_interruptible_timeout(wq, cond, timeout)				\
		__wait_event_timeout(wq, cond, TASK_INTERRUPTIBLE, 0, timeout)

#endif	/* _LINUX_WAIT_H_ */
