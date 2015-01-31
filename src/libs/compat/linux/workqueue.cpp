/*
 * Copyright 2015, Hamish Morrison, hamishm53@gmail.com
 * All rights reserved. Distributed under the terms of the MIT License.
 */

extern "C" {
#include <linux/workqueue.h>
#include <linux/err.h>
#include <linux/list.h>
}

#include <condition_variable.h>
#include <lock.h>
#include <thread.h>


static work_struct* to_work(struct list_head* entry)
{
	return container_of(entry, struct work_struct, list);
}

struct workqueue_struct {
	const char* name;
	mutex lock;
	ConditionVariable wake;

	// Work list
	struct list_head list;
	bool closed;

	// Work threads
	int threads;
	thread_id thread[];
};


extern "C" {


static status_t
workqueue_thread(void* arg) {
	struct workqueue_struct* wq = (struct workqueue_struct*)arg;

	mutex_lock(&wq->lock);
	while (true) {
		if (list_empty(&wq->list)) {
			if (wq->closed) {
				mutex_unlock(&wq->lock);
				break;
			}

			ConditionVariableEntry entry;
			wq->wake.Add(&entry);

			mutex_unlock(&wq->lock);
			wq->wake.Wait();
			mutex_lock(&wq->lock);
			continue;
		}

		// Linked work needs to be executed on the same thread so flush_work
		// can gaurantee that the callback has finished executing.
		struct list_head linked_work;

		struct work_struct *work, *tmp;
		list_for_each_entry_safe(work, tmp, &wq->list, list) {
			list_move_tail(&work->list, &linked_work);
			if (!work->linked)
				break;
		}

		mutex_unlock(&wq->lock);
		list_for_each_entry(work, &linked_work, list) {
			work->fn(work);
		}
		mutex_lock(&wq->lock);

	}

	return B_OK;
}


struct workqueue_struct*
_workqueue_create(const char* name, int threads) {
	struct workqueue_struct* wq = (struct workqueue_struct*)
		malloc(sizeof(struct workqueue_struct) + sizeof(thread_id) * threads);
	if (wq == NULL)
		return NULL;

	wq->name = name;
	wq->wake.Init((void*)wq, name);
	wq->closed = false;
	wq->threads = threads;

	mutex_init(&wq->lock, "linux wq");
	INIT_LIST_HEAD(&wq->list);

	for (int i = 0; i < threads; i++) {
		wq->thread[i] = spawn_kernel_thread(workqueue_thread, "lwq",
			B_NORMAL_PRIORITY, (void*)wq);
		if (wq->thread[i] < 0)
			return (struct workqueue_struct*)ERR_PTR(wq->thread[i]);

		resume_thread(wq->thread[i]);
	}

	return wq;
}


static bool
on_workqueue_thread(struct workqueue_struct* wq)
{	
	thread_id self = thread_get_current_thread_id();
	for (int i = 0; i < wq->threads; i++) {
		if (wq->thread[i] == self)
			return true;
	}
	return false;
}


bool
queue_work(struct workqueue_struct* wq, struct work_struct* work)
{
	mutex_lock(&wq->lock);
	if (work->queue != NULL)
		return false;

	// Work items are allowed to queue more work even when the queue
	// is closing.
	if (work->queue->closed && !on_workqueue_thread(wq)) {
		mutex_unlock(&wq->lock);
		return false;
	}

	list_add_tail(&wq->list, &work->list);
	mutex_unlock(&wq->lock);

	wq->wake.NotifyOne();
	return true;
}


static inline int32
_delayed_work_timer(struct timer* event)
{
	struct delayed_work *work = container_of(event, struct delayed_work, event);
	queue_work(work->work.queue, &work->work);
	return B_OK;
}


bool
queue_delayed_work(struct workqueue_struct *wq, struct delayed_work *work,
    unsigned long delay)
{
	if (work->work.queue != NULL)
		return false;

	if (delay != 0) {
		add_timer(&work->event, _delayed_work_timer, delay,
			B_ONE_SHOT_RELATIVE_TIMER);
	} else {
		queue_work(wq, &work->work);
	}

	return true;
}


void
destroy_workqueue(struct workqueue_struct* wq)
{
	mutex_lock(&wq->lock);
	wq->closed = true;
	mutex_unlock(&wq->lock);

	wq->wake.NotifyAll();

	for (int i = 0; i < wq->threads; i++) {
		wait_for_thread(wq->thread[i], NULL);
	}
}


struct flush_work {
	struct work_struct work;
	ConditionVariable sync;
};


static void
flush_sync(struct work_struct* work)
{
	struct flush_work* flush = container_of(work, struct flush_work, work);
	flush->sync.NotifyAll();
}


static void
wait_work_locked(struct workqueue_struct* wq, struct work_struct* work)
{
	struct flush_work flush;
	INIT_WORK(&flush.work, flush_sync);
	flush.sync.Init(&flush, "lwq cancel");

	work->linked = true;
	list_add(&work->list, &flush.work.list);

	ConditionVariableEntry entry;
	flush.sync.Add(&entry);

	mutex_unlock(&wq->lock);
	entry.Wait();
}


bool
cancel_work_sync(struct work_struct* work)
{
	if (work->queue == NULL)
		return false;

	mutex_lock(&work->queue->lock);
	if (!list_empty(&work->list)) {
		list_del_init(&work->list);
		mutex_unlock(&work->queue->lock);
		return true;
	}

	wait_work_locked(work->queue, work);
	return false;
}


void
flush_workqueue(struct workqueue_struct* wq)
{
	mutex_lock(&wq->lock);
	if (list_empty(&wq->list)) {
		mutex_unlock(&wq->lock);
		return;
	}

	wait_work_locked(wq, to_work(wq->list.prev));
}


bool
mod_delayed_work(struct workqueue_struct* wq, struct delayed_work* work,
	unsigned long delay)
{
	bool fired = cancel_timer(&work->event);
	if (!fired) {
		queue_delayed_work(wq, work, delay);
		return true;
	}

	// Not convinced this is the same semantics as the Linux version
	return queue_delayed_work(wq, work, delay);
}


bool
flush_work(struct work_struct* work)
{
	if (work->queue == NULL)
		return false;

	mutex_lock(&work->queue->lock);
	if (list_empty(&work->list)) {
		mutex_unlock(&work->queue->lock);
		return false;
	}

	wait_work_locked(work->queue, work);
	return true;
}


bool 
flush_delayed_work(struct delayed_work* work)
{
	if (!cancel_timer(&work->event)) {
		queue_work(work->work.queue, &work->work);
	}

	return flush_work(&work->work);
}

bool
cancel_delayed_work(struct delayed_work* work)
{
	if (!cancel_timer(&work->event))
		return true;

	if (work->work.queue == NULL)
		return false;

	bool result = false;

	mutex_lock(&work->work.queue->lock);
	if (!list_empty(&work->work.list)) {
		list_del_init(&work->work.list);
		result = true;
	}

	mutex_unlock(&work->work.queue->lock);
	return false;
}


bool
cancel_delayed_work_sync(struct delayed_work* work)
{
	if (!cancel_timer(&work->event))
		return true;

	return cancel_work_sync(&work->work);
}

}
