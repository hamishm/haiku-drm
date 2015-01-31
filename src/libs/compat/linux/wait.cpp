/*
 * Copyright 2015, Hamish Morrison, hamishm53@gmail.com
 * All rights reserved. Distributed under the terms of the MIT License.
 */

extern "C" {
#include <linux/wait.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/sched.h>
}

#include <KernelExport.h>
#include <thread.h>
#include <util/AutoLock.h>


#define __WQ_FLAG_INTERRUPTIBLE 0x02


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
__waitqueue_wake_locked(wait_queue_head_t* queue, int count, unsigned int mode,
	void* key)
{
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
}


static void
__waitqueue_prepare_locked(wait_queue_head_t* queue, wait_queue_t* waiter, int state, bool exclusive)
{
	if (exclusive)
		list_add_tail(&waiter->entry, &queue->waiters);
	else
		list_add(&waiter->entry, &queue->waiters);

	ASSERT(waiter->thread == NULL);

	Thread* thread = thread_get_current_thread();
	waiter->thread = (void*)thread;
	waiter->flags = (exclusive ? WQ_FLAG_EXCLUSIVE : 0) |
		(state == TASK_INTERRUPTIBLE ? __WQ_FLAG_INTERRUPTIBLE : 0);

	thread_prepare_to_block(thread, state, THREAD_BLOCK_TYPE_OTHER,
		"linux waitqueue");
}


}
