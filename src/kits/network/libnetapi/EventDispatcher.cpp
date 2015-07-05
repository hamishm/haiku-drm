/*
 * Copyright 2015, Hamish Morrison, hamishm53@gmail.com.
 * All rights reserved. Distributed under the terms of the MIT License.
 */

#include <EventDispatcher.h>

#include <OS.h>

#if 0
#include <util/Heap.h>
#endif

#define EVENTS_TO_READ	50


BEventDispatcher::BEventDispatcher()
{
	fEventQueue = event_queue_create(O_CLOEXEC);
}


BEventDispatcher::~BEventDispatcher()
{
}


status_t
BEventDispatcher::InitCheck()
{
	return fEventQueue;
}


void
BEventDispatcher::_DispatchTimers()
{
	//bigtime_t current = real_time_clock_usecs();
#if 0
	while (Timer* timer = fTimers.PeekRoot()) {
		if (fTimers.GetKey(timer) > current)
			break;

		timer->function();
		fTimers.RemoveRoot();
	}
#endif
}


bigtime_t
BEventDispatcher::_DetermineTimeout()
{
#if 0
	Timer* first = fTimers.PeekRoot();
	return first != NULL ? fTimers.GetKey(first) : B_INFINITE_TIMEOUT;
#else
	return B_INFINITE_TIMEOUT;
#endif
}


status_t
BEventDispatcher::RunOnce()
{
	_DispatchTimers();


	bigtime_t timeout = _DetermineTimeout();

	event_wait_info infos[EVENTS_TO_READ];

	ssize_t result = event_queue_wait(fEventQueue, infos, EVENTS_TO_READ,
		B_ABSOLUTE_REAL_TIME_TIMEOUT, timeout);

	if (result < B_OK)
		return result;

	for (ssize_t i = 0; i < result; i++) {
		int32 events = infos[i].events;

		Wrapper* wrapper = (Wrapper*)infos[i].user_data;
		(*wrapper)(events);
	}

	return result;
}


status_t
BEventDispatcher::_WaitForObject(int32 object, uint16 type, uint16 events,
	Wrapper* wrapper, bool oneShot)
{
	event_wait_info info;
	info.object = object;
	info.type = type;
	info.events = events | B_EVENT_SELECT | (oneShot ? B_EVENT_ONE_SHOT : 0);
	info.user_data = (void*)wrapper;

	status_t result = event_queue_select(fEventQueue, &info, 1);

	// Somewhat ugly: if the error is 'B_ERROR' then the error is stored in the
	// events field of the event_wait_info.
	if (result == B_ERROR)
		return info.events;
	else if (result != B_OK)
		return result;

	return B_OK;
}
