/*
 * Copyright 2015, Hamish Morrison, hamishm53@gmail.com
 * All rights reserved. Distributed under the terms of the MIT License.
 */

extern "C" {
#include <linux/wait.h>
#include <linux/list.h>
}

#include <KernelExport.h>
#include <thread.h>
#include <util/AutoLock.h>


extern "C" {


bool
autoremove_wake_function(wait_queue_t* waiter, unsigned int mode, int sync,
	void* key)
{
	if (mode == TASK_INTERRUPTIBLE
			&& (waiter->flags & __WQ_FLAG_INTERRUPTIBLE) == 0)
		return false;

	InterruptsSpinLocker _(&((Thread*)waiter->thread)->scheduler_lock);
	if (!thread_is_blocked((Thread*)waiter->thread))
		return false;

	list_del_init(&waiter->entry);
	thread_unblock_locked((Thread*)waiter->thread, B_OK);

	return true;
}


void
__waitqueue_wake(wait_queue_head_t* queue, int count, unsigned int mode,
	void* key)
{
	acquire_spinlock(&queue->lock);

	int woken = 0;

	wait_queue_t *entry, *temp;
	list_for_each_entry_safe(entry, temp, &queue->waiters, entry) {
		bool result = entry->func(entry, mode, 0, key);
		entry->thread = NULL;

		if (!result && (entry->flags & WQ_FLAG_EXCLUSIVE) != 0
				&& ++woken == count) {
			break;
		}
	}

	release_spinlock(&queue->lock);
}


static void
__waitqueue_prepare(wait_queue_head_t* queue, wait_queue_t* waiter, int state, bool exclusive)
{
	acquire_spinlock(&queue->lock);

	if (exclusive)
		list_add_tail(&waiter->entry, &queue->waiters);
	else
		list_add(&waiter->entry, &queue->waiters);

	waiter->flags = exclusive ? WQ_WAIT_EXCLUSIVE : 0;
	ASSERT(waiter->thread == NULL);

	Thread* thread = thread_get_current();
	waiter->thread = (void*)thread;
	waiter->flags = flags;

	thread_prepare_to_block(thread, state == TASK_INTERRUPTIBLE ?
		B_CAN_INTERRUPT : 0, THREAD_BLOCK_TYPE_OTHER, "linux waitqueue");

	release_spinlock(&queue->lock);
}


void
finish_wait(wait_queue_head_t* queue, wait_queue_t* waiter)
{
	acquire_spinlock(&queue->lock);

	if (!list_empty(&waiter->entry))
		list_del_init(&waiter->entry);

	release_spinlock(&queue->lock);
}


}
