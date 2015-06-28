/*
 * Copyright 2015, Hamish Morrison, hamishm53@gmail.com.
 * All rights reserved. Distributed under the terms of the MIT License.
 */

#include <OS.h>
#include <syscalls.h>


int
event_queue_create(int openFlags)
{
	return _kern_event_queue_create(openFlags);
}


status_t
event_queue_select(int queue, event_wait_info* infos, int numInfos)
{
	return _kern_event_queue_select(queue, infos, numInfos);
}


ssize_t
event_queue_wait(int queue, event_wait_info* infos, int numInfos, uint32 flags,
	bigtime_t timeout)
{
	return _kern_event_queue_wait(queue, infos, numInfos, flags, timeout);
}
