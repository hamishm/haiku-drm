/*
 * Copyright 2015, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _EVENT_DISPATCHER_H
#define _EVENT_DISPATCHER_H

#include <OS.h>

#include <functional>


typedef std::function<void (int)>		EventCallback;
typedef std::function<void (ssize_t)>	IOCallback;
typedef std::function<void (status_t)>	StatusCallback;


class BEventDispatcher {
public:
						BEventDispatcher();
	virtual				~BEventDispatcher();

			status_t	InitCheck();

			status_t	RunOnce();

			template<typename Callback>
	inline	status_t	WaitForFD(int fd, int events, Callback&& callback,
							bool oneShot = true);

			template<typename Callback>
	inline	status_t	WaitForPort(port_id port, int events,
							Callback&& callback, bool oneShot = true);

			template<typename Callback>
	inline	status_t	WaitForSemaphore(sem_id semaphore, int events,
							Callback&& callback, bool oneShot = true);

			template<typename Callback>
	inline	status_t	WaitForThread(thread_id thread, int events,
							Callback&& callback, bool oneShot = true);

			template<typename Callback>
	inline	status_t	WaitForObject(int32 object, uint16 type, uint16 events,
							Callback&& callback, bool oneShot = true);

#if 0
			template<typename Function>
			void		ExecuteAt(Function function, bigtime_t time);

			template<typename Function>
			void		ExecuteLater(Function function);
#endif
private:
	typedef std::function<void(int)> Wrapper;

			bigtime_t	_DetermineTimeout();
			void		_DispatchTimers();

			status_t	_WaitForObject(int32 object, uint16 type,
							uint16 events, Wrapper* waiter,
							bool oneShot);

private:
#if 0
	template<typename Function>
	struct Timer : HeapLinkImpl<Timer, bigtime_t> {
		Function function;
	};
#endif


	template<typename Callback>
	struct EventWaiter {
		EventWaiter(Callback&& callback)
		{
			fCallback = callback;
			fWrapper = std::ref(callback);
		}

		void DoCallback(int events)
		{
			fCallback(events);
			delete this;
		}

		Callback	fCallback;
		Wrapper		fWrapper;
	};

#if 0
	typedef Heap<Timer, bigtime_t, HeapLesserCompare<bigtime_t>> TimerHeap;

			TimerHeap	fTimers;
#endif

			int			fEventQueue;
};


template<typename Callback>
status_t
BEventDispatcher::WaitForFD(int fd, int events, Callback&& callback,
	bool oneShot)
{
	return WaitForObject(fd, B_OBJECT_TYPE_FD, events, callback, oneShot);

}


template<typename Callback>
status_t
BEventDispatcher::WaitForPort(port_id port, int events, Callback&& callback,
	bool oneShot)
{
	return WaitForObject(port, B_OBJECT_TYPE_PORT, events, callback, oneShot);
}


template<typename Callback>
status_t
BEventDispatcher::WaitForSemaphore(sem_id semaphore, int events,
	Callback&& callback, bool oneShot)
{
	return WaitForObject(semaphore, B_OBJECT_TYPE_SEMAPHORE, events, callback,
		oneShot);
}


template<typename Callback>
status_t
BEventDispatcher::WaitForThread(thread_id thread, int events,
	Callback&& callback, bool oneShot)
{
	return WaitForObject(thread, B_OBJECT_TYPE_THREAD, events, callback,
		oneShot);
}


template<typename Callback>
status_t
BEventDispatcher::WaitForObject(int32 object, uint16 type, uint16 events,
	Callback&& callback, bool oneShot)
{
	EventWaiter<Callback>* waiter = new(std::nothrow)
		EventWaiter<Callback>(callback);
	if (waiter == NULL)
		return B_NO_MEMORY;

	status_t status = _WaitForObject(object, type, events, &waiter->fWrapper,
		oneShot);
	if (status != B_OK) {
		delete waiter;
		return status;
	}

	status;
}


#endif // _EVENT_DISPATCHER_H
