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

#include <compat/condvar.h>
#include <linux/list.h>


#define WQ_FLAG_EXCLUSIVE 0x01


typedef bool (*wait_queue_func_t)(wait_queue_t* entry, unsigned int mode,
	int flags, void* key);

/*
extern int default_wake_function(wait_queue_t* entry, unsigned int mode,
	int flags, void* key);
*/

typedef struct {
	struct list_head entry;
	wait_queue_func_t func;
	void* thread;
	int flags;
} wait_queue_t;


typedef struct {
	spinlock lock;
	struct list_head waiters;
} wait_queue_head_t;


extern void __waitqueue_wake(wait_queue_head_t* queue, int count,
	unsigned int mode, void* key);

extern void __waitqueue_prepare(wait_queue_head_t* queue, wait_queue_t* waiter,
	int state, bool exclusive)


#define prepare_to_wait(q, w, s) __waitqueue_prepare((q), (w), (s), false)
#define prepare_to_wait_exclusive(q, w, s) __waitqueue_prepare((q), (w), (s), true)

extern void finish_wait(wait_queue_head_t* queue, wait_queue_t* waiter);

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


extern bool autoremove_wake_function(wait_queue_t* wait, unsigned int mode, int sync, void* key);


static inline void
init_waitqueue_head(wait_queue_head_t* queue)
{
	spin_lock_init(&queue->lock);
	INIT_LIST_HEAD(&queue->waiters);
}

#define DECLARE_WAIT_QUEUE_HEAD(queue) 				\
	wait_queue_head_t queue = {						\
		.lock = B_SPINLOCK_INITIALIZER,				\
		.waiters = LIST_HEAD_INIT(&(queue).waiters)	\
	}

#define DEFINE_WAIT_FUNC(wait, fn)					\
	wait_queue_t (wait) = {							\
		.entry = LIST_HEAD_INIT(&(wait).entry),		\
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


#define __wait_event_common(wq, cond, flags, excl)		\
({														\
	bool interrupted = false;							\
	status_t ret = B_OK;								\
														\
	DEFINE_WAIT(wait);									\
	while (1) {											\
		prepare_to_wait((wq), wait, (flags), (excl));	\
		if (cond)										\
			break;										\
														\
		ret = thread_block();							\
		if (ret == B_INTERRUPTED)						\
			break;										\
	}													\
	finish_wait((wq), wait);							\
	ret;												\
})


/*
 * wait_event_interruptible_timeout:
 * - The process is put to sleep until the condition evaluates to true.
 * - The condition is checked each time the waitqueue wq is woken up.
 * - wake_up has to be called after changing any variable that could change
 * the result of the wait condition.
 *
 * returns:
 *   - 0 if the timeout elapsed
 *   - the remaining jiffies if the condition evaluated to true before
 *   the timeout elapsed.
 *   - remaining jiffies are always at least 1
 *   - -ERESTARTSYS if interrupted by a signal (when PCATCH is set in flags)
*/
#define __wait_event_common(wq, condition, timeout_jiffies, flags)	\
({									\
	int start_jiffies, elapsed_jiffies, remaining_jiffies, ret;	\
	bool timeout_expired = false;					\
	bool interrupted = false;					\
	long retval;							\
									\
	DEFINE_WAIT(wait);				\
	start_jiffies = ticks;						\
									\
	while (1) {							\
		prepare_to_wait(
		if (condition)						\
			break;						\
									\
		ret = cv_wait(&wq.condvar, &wq.lock, flags,			\
					"lwe", timeout_jiffies);	\
		if (ret == EINTR || ret == ERESTART) {			\
			interrupted = true;				\
			break;						\
		}							\
		if (ret == EWOULDBLOCK) {				\
			timeout_expired = true;				\
			break;						\
		}							\
	}								\
	mutex_unlock(&wq.lock);					\
									\
	elapsed_jiffies = ticks - start_jiffies;			\
	remaining_jiffies = timeout_jiffies - elapsed_jiffies;		\
	if (remaining_jiffies <= 0)					\
		remaining_jiffies = 1;					\
									\
	if (timeout_expired)						\
		retval = 0;						\
	else if (interrupted)						\
		retval = ERESTARTSYS;					\
	else if (timeout_jiffies > 0)					\
		retval = remaining_jiffies;				\
	else								\
		retval = 1;						\
									\
	retval;								\
})

#define wait_event(wq, condition)					\
		__wait_event_common(wq, condition, 0, false)

#define wait_event_interruptible(wq, condition)				\
		__wait_event_common(wq, condition, TASK_INTERRUPTIBLE, false)

#define wait_event_timeout(wq, condition, timeout)			\
		__wait_event_common(wq, condition, timeout, 0)

#define wait_event_interruptible_timeout(wq, condition, timeout)	\
		__wait_event_common(wq, condition, timeout, B_CAN_INTERRUPT)

#endif	/* _LINUX_WAIT_H_ */
