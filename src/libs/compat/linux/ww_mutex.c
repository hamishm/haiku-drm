/*
 * Copyright 2015, Hamish Morrison, hamishm53@gmail.com.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#include <linux/ww_mutex.h>

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/wait.h>


int
__ww_mutex_lock(struct ww_mutex* mutex, struct ww_acquire_ctx* context,
	int state)
{
	int32 old;
	DEFINE_WAIT(wait);

	waitqueue_lock(&mutex->waiters);

	while (true) {
		if (context != NULL) {
			ASSERT(mutex->context != NULL);

			if (mutex->context == context) {
				waitqueue_unlock(&mutex->waiters);
				return -EALREADY;
			}

			if (mutex->context->ticket < context->ticket) {
				waitqueue_unlock(&mutex->waiters);
				return -EDEADLK;
			}
		}

		// Set it contended because we're going to wait
		old = atomic_get_and_set(&mutex->locked, __WW_CONTENDED);

		// It may have been unlocked in the meantime
		if (old == __WW_UNLOCKED)
			break;

		__waitqueue_prepare_locked(&mutex->waiters, &wait, state, true);

		waitqueue_unlock(&mutex->waiters);
		schedule();
		waitqueue_lock(&mutex->waiters);

		__finish_wait_locked(&wait);
	}

	if (context != NULL) {
		mutex->context = context;
		mutex->context->acquired++;

		// It's possible that other waiters should return with -EDEADLK now,
		// so wake up everyone.
		__waitqueue_wake_locked(&mutex->waiters, 0, 0, NULL);
	}

	waitqueue_unlock(&mutex->waiters);
	return 1;
}


void
ww_mutex_unlock(struct ww_mutex* mutex)
{
	int32 old;

	if (mutex->context) {
		ASSERT(mutex->context->acquired > 0);
		mutex->context->acquired--;
		mutex->context = NULL;
	}

	old = atomic_get_and_set(&mutex->locked, __WW_UNLOCKED);
	if (old == __WW_CONTENDED) {
		__waitqueue_wake(&mutex->waiters, 1, 0, NULL);
	}	
}
