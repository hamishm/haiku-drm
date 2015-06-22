/*
 * Copyright 2005, Ingo Weinhold, bonefish@users.sf.net.
 * Distributed under the terms of the MIT License.
 */
#ifndef _KERNEL_SELECT_SYNC_POOL_H
#define _KERNEL_SELECT_SYNC_POOL_H

#include <SupportDefs.h>


typedef struct selectsync selectsync;
typedef struct mutex mutex;


typedef struct select_sync_pool {
	selectsync*	first;
	mutex*		lock;
} select_sync_pool;


#ifdef __cplusplus
extern "C" {
#endif


// New style select_sync_pool API

static void
select_sync_init_pool(select_sync_pool* pool, mutex* lock)
{
	pool->first = NULL;
	pool->lock = lock;
}

void select_sync_notify_pool(select_sync_pool* pool, int32 events);
void select_sync_add_pool_entry(select_sync_pool* pool, selectsync* sync);


// Old select_sync_pool API

void notify_select_event_pool(select_sync_pool* pool, uint8 event);
void add_select_sync_pool_entry(select_sync_pool* pool, selectsync* sync, uint8 event);


#ifdef __cplusplus
}
#endif


#endif	// _KERNEL_SELECT_SYNC_POOL_H
