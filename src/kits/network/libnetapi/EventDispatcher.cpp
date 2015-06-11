/*
 * Copyright 2015, Hamish Morrison, hamishm53@gmail.com.
 * All rights reserved. Released under the terms of the MIT license.
 */

#include <EventDispatcher.h>

#include <Debug.h>
#include <StackOrHeapArray.h>
#include <util/Heap.h>


struct BEventDispatcher::Waiter {
	int32 object;
	uint16 type;
	uint16 events;

	bool one_shot;
	std::function<void (int)> callback;
};


BEventDispatcher::BEventDispatcher()
	:
	fTimers(20),
	fWaiters(20)
{
}


BEventDispatcher::~BEventDispatcher()
{
}


void
BEventDispatcher::_DispatchTimers()
{
	bigtime_t current = real_time_clock_usecs();

	while (Timer* timer = fTimers.PeekRoot()) {
		if (fTimers.GetKey(timer) > current)
			break;

		timer->function();
		fTimers.RemoveRoot();
	}
}


bigtime_t
BEventDispatcher::_DetermineTimeout()
{
	Timer* first = fTimers.PeekRoot();
	return first != NULL ? fTimers.GetKey(first) : B_INFINITE_TIMEOUT;
}


status_t
BEventDispatcher::RunOnce()
{
	_DispatchTimers();

	int32 waiters = fWaiters.CountItems();

	BStackOrHeapArray<object_wait_info, 50> array(waiters);
	if (!array.IsValid())
		return B_NO_MEMORY;

	for (ConstIterator 
	for (int i = 0; i < waiters; i++) {
		array[i].object = fWaiters.ItemAt(i)->object;
		array[i].type = fWaiters.ItemAt(i)->type;
		array[i].events = fWaiters.ItemAt(i)->events;
	}

	bigtime_t timeout = _DetermineTimeout();
	ssize_t result = wait_for_objects_etc(array, waiters, B_ABSOLUTE_TIMEOUT,
		timeout);

	if (result == B_TIMED_OUT || result == B_WOULD_BLOCK)
		result = B_OK;

	if (result != B_OK)
		return result;

	ssize_t remaining = result;
	for (int i = 0, j = 0; i < waiters && remaining > 0; i++, j++) {
		if (array[i].events != 0) {
			remaining--;

			Waiter* waiter = fWaiters.ItemAt(j);
			if (waiter->oneShot) {
				fWaiters.RemoveItemAt(i);
				j--;
			}

			waiter->callback(array[i].events);
		}
	}

	return result;
}

/*
void
WaitForFD(int fd, int events, Callback callback, bool oneShot)
{
	return _WaitForObject(fd, B_OBJECT_TYPE_FD, events, callback, oneShot);
}


void
WaitForSem(sem_id sem, int events, Callback callback, bool oneShot)
{
	return _WaitForObject(sem, B_OBJECT_TYPE_SEMAPHORE, events, callback,
		oneShot);
}


void
WaitForPort(port_id port, int events, Callback callback, bool oneShot)
{
	return _WaitForObject(port, B_OBJECT_TYPE_PORT, events, callback, oneShot);
}


void
WaitForThread(thread_id thread, int events, Callback callback, bool oneShot)
{
	return _WaitForObject(thread, B_OBJECT_TYPE_THREAD, events, callback,
		oneShot);
}
*/

void
WaitForObject(int32 object, uint16 type, uint16 events, Callback callback,
	bool oneShot)
{
	Waiter* waiter = new(std::nothrow) Waiter();
	waiter->object = object;
	waiter->type = type;
	waiter->events = events;
	waiter->callback = callback;
	waiter->one_shot = oneShot;

	fWaiters.Add(waiter);
}
