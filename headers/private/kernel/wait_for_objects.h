/*
 * Copyright 2007, Ingo Weinhold, bonefish@cs.tu-berlin.de. All rights reserved.
 * Distributed under the terms of the MIT License.
 */

#ifndef _KERNEL_WAIT_FOR_OBJECTS_H
#define _KERNEL_WAIT_FOR_OBJECTS_H

#include <OS.h>

#include <lock.h>


struct select_sync;


typedef struct select_info {
	struct select_info*			next;
	struct select_sync_base*	sync;
	int32						events;
	uint16						selected_events;
} select_info;


#define SYNC_TYPE_QUEUE	1
#define SYNC_TYPE_SYNC	2


typedef struct select_sync_base {
	uint32						type;
	int32						ref_count;
} select_sync_base;


#define SELECT_FLAG(type) (1L << (type - 1))


#ifdef __cplusplus
extern "C" {
#endif


extern void		put_select_sync(select_sync_base* sync);
extern status_t	notify_select_events(select_info* info, uint16 events);
extern void		notify_select_events_list(select_info* list, uint16 events);

extern ssize_t	_user_wait_for_objects(object_wait_info* userInfos,
					int numInfos, uint32 flags, bigtime_t timeout);


#ifdef __cplusplus
}
#endif 

#endif	// _KERNEL_WAIT_FOR_OBJECTS_H
