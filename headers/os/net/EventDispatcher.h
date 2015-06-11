/*
 * Copyright 2015, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT license.
 */
#ifndef _EVENT_DISPATCHER_H
#define _EVENT_DISPATCHER_H


#include <util/DoublyLinkedList.h>
#include <util/Heap.h>

#include <functional>


class BEventDispatcher {
public:
						BEventDispatcher();
	virtual				~BEventDispatcher();

	virtual	status_t	RunOnce();

	typedef std::function<void (int)> Callback;
/*
	void				WaitForFD(int fd, int events, Callback callback,
							bool oneShot = true);
	void				WaitForPort(port_id port, int events, Callback callback,
							bool oneShot = true);
	void				WaitForSem(sem_id sem, int events, Callback callback,
							bool oneShot = true);
	void				WaitForThread(thread_id thread, int events,
							Callback callback, bool oneShot = true);
*/
	typedef std::function<void (void)> Function;

	void				Wait(int32 object, uint16 type, uint16 events,
							Callback callback, bool oneShot);

	void				ExecuteAt(Function function, bigtime_t time);
	void				ExecuteLater(Function function);

private:
	bigtime_t			_DetermineTimeout();
	void				_DispatchTimers();


private:
	struct Waiter : DoublyLinkedListImpl<Waiter> {
		int32 object;
		uint16 type;
		uint16 events;

		bool one_shot;
		Callback callback;
	};

	struct Timer : HeapLinkImpl<Timer, bigtime_t> {
		Function function;
	};

	typedef Heap<Timer, bigtime_t, HeapLesserCompare<bigtime_t>> TimerHeap;
	typedef DoublyLinkedList<Waiter> WaiterList;

	TimerHeap 			fTimers;
	WaiterList			fWaiters;
	unsigned			fWaiterCount;
};


#endif // _H
