/*
 * Copyright 2015, Hamish Morrison, hamishm53@gmail.com.
 * All rights reserved. Distributed under the terms of the MIT License.
 */

#include <event_queue.h>

#include <OS.h>

#include <AutoDeleter.h>

#include <fs/fd.h>
#include <port.h>
#include <sem.h>
#include <syscalls.h>
#include <syscall_restart.h>
#include <thread.h>
#include <util/AutoLock.h>
#include <util/AVLTree.h>
#include <util/DoublyLinkedQueue.h>
#include <wait_for_objects.h>


struct select_ops {
	status_t (*select)(int32 object, struct select_info* info, bool kernel);
	status_t (*deselect)(int32 object, struct select_info* info, bool kernel);
};


enum {
	B_EVENT_QUEUED		= 0x0200,
	B_EVENT_SELECTING	= 0x0400,
	B_EVENT_DELETING	= 0x0800,
	B_EVENT_PRIVATE		= 0x0e00
};


#define EVENT_BEHAVIOUR(events) ((events) & B_EVENT_ONE_SHOT)
#define USER_EVENTS(events) ((events) & ~B_EVENT_PRIVATE)

#define B_EVENT_NON_MASKABLE (B_EVENT_INVALID | B_EVENT_ERROR \
	| B_EVENT_DISCONNECTED)
#define B_EVENT_ACTIONS (B_EVENT_SELECT | B_EVENT_MODIFY | B_EVENT_DESELECT)


static const select_ops kSelectOps[] = {
	// B_OBJECT_TYPE_FD
	{
		select_fd,
		deselect_fd
	},

	// B_OBJECT_TYPE_SEMAPHORE
	{
		select_sem,
		deselect_sem
	},

	// B_OBJECT_TYPE_PORT
	{
		select_port,
		deselect_port
	},

	// B_OBJECT_TYPE_THREAD
	{
		select_thread,
		deselect_thread
	}
};


static const uint32 kSelectOpsCount = sizeof(kSelectOps) / sizeof(select_ops);


struct select_event : select_info, AVLTreeNode,
		DoublyLinkedListLinkImpl<select_event> {
	int32				object;
	uint16				type;
	void*				user_data;
};


struct EventQueueTreeDefinition {
	typedef struct {
		int32 object;
		uint16 type;
	} 						Key;
	typedef select_event	Value;

	AVLTreeNode* GetAVLTreeNode(Value* value) const
	{
		return value;
	}

	Value* GetValue(AVLTreeNode* node) const
	{
		return static_cast<Value*>(node);
	}

	int Compare(Key a, const Value* b) const
	{
		if (a.object != b->object)
			return a.object - b->object;
		else
			return a.type - b->type;
	}

	int Compare(const Value* a, const Value* b) const
	{
		if (a->object != b->object)
			return a->object - b->object;
		else
			return a->type - b->type;
	}
};


//	#pragma mark -- EventQueue implementation


class EventQueue : public select_sync_base {
public:
						EventQueue(bool kernel);
						~EventQueue();

	void				Closed();

	status_t			Select(int32 object, uint16 type, uint16 events,
							void* userData);
	status_t			Modify(int32 object, uint16 type, uint16 events,
							void* userData);
	status_t			Deselect(int32 object, uint16 type);

	void				Notify(select_event* event, uint16 events);

	ssize_t				Wait(event_wait_info* infos, int numInfos, int32 flags,
							bigtime_t timeout);

private:
	status_t			_SelectEvent(select_event* event, uint16 events,
							void* userData, MutexLocker& locker);
	status_t			_DeselectEvent(select_event* event);

	ssize_t				_DequeueEvents(event_wait_info* infos, int numInfos);

	select_event*		_GetEvent(int32 object, uint16 type);

private:
	typedef AVLTree<EventQueueTreeDefinition> EventTree;
	typedef DoublyLinkedList<select_event> EventList;

	bool				fKernel;
	bool				fClosing;

	/*
	 * This flag is set in _DequeueEvents when we have to drop the lock to
	 * deselect an object to prevent another _DequeueEvents call concurrently
	 * modifying the list.
	 */
	bool				fDequeueing;

	EventList			fEventList;
	EventTree			fEventTree;

	/*
	 * Protects the queue. We cannot call select or deselect while holding
	 * this, because it will invert the locking order with EventQueue::Notify.
	 */
	mutex				fQueueLock;

	/*
	 * Notified when events are available on the queue.
	 */
	ConditionVariable	fQueueCondition;

	/*
	 * Used to wait on a changing select_event while the queue lock is dropped
	 * during a call to select/deselect.
	 */
	ConditionVariable	fEventCondition;
};


EventQueue::EventQueue(bool kernel)
	:
	fKernel(kernel),
	fClosing(false),
	fDequeueing(false)
{
	mutex_init(&fQueueLock, "Event queue lock");
	fQueueCondition.Init(this, "Event queue wait");
	fEventCondition.Init(this, "Event queue event change wait");
}


EventQueue::~EventQueue()
{
	MutexLocker locker(&fQueueLock);

	EventTree::Iterator iter = fEventTree.GetIterator();
	while (iter.HasNext()) {
		select_event* event = iter.Next();
		event->events |= B_EVENT_DELETING;

		locker.Unlock();
		_DeselectEvent(event);
		locker.Lock();

		iter.Remove();
		delete event;
	}
}


void
EventQueue::Closed()
{
	MutexLocker locker(&fQueueLock);
	fClosing = true;
	locker.Unlock();
	
	// Wake up all waiters
	fQueueCondition.NotifyAll(B_FILE_ERROR);
}


status_t
EventQueue::Select(int32 object, uint16 type, uint16 events, void* userData)
{
	if (type >= kSelectOpsCount)
		return B_BAD_VALUE;

	MutexLocker locker(&fQueueLock);

	select_event* event = _GetEvent(object, type);
	if (event != NULL)
		return B_FILE_EXISTS;

	event = new(std::nothrow) select_event;
	if (event == NULL)
		return B_NO_MEMORY;

	event->sync = this;
	event->object = object;
	event->type = type;

	status_t result = fEventTree.Insert(event);
	if (result != B_OK)
		return result;

	return _SelectEvent(event, events, userData, locker);
}


status_t
EventQueue::Modify(int32 object, uint16 type, uint16 events, void* userData)
{
	if (type >= kSelectOpsCount)
		return B_BAD_VALUE;

	MutexLocker locker(&fQueueLock);

	select_event* event = _GetEvent(object, type);
	if (event == NULL)
		return B_ENTRY_NOT_FOUND;

	// We drop the lock before calling _DeselectEvent to avoid inverting the
	// locking order with Notify(). Setting the B_EVENT_SELECTING flag prevents
	// the event from being deleted from under us.
	event->events |= B_EVENT_SELECTING;

	locker.Unlock();
	_DeselectEvent(event);
	locker.Lock();

	return _SelectEvent(event, events, userData, locker);
}


status_t
EventQueue::Deselect(int32 object, uint16 type)
{
	if (type >= kSelectOpsCount)
		return B_BAD_VALUE;

	MutexLocker locker(&fQueueLock);

	select_event* event = _GetEvent(object, type);
	if (event == NULL)
		return B_ENTRY_NOT_FOUND;

	event->events |= B_EVENT_DELETING;
	locker.Unlock();

	_DeselectEvent(event);

	locker.Lock();
	fEventTree.Remove(event);

	if ((event->events & B_EVENT_QUEUED) != 0)
		fEventList.Remove(event);

	delete event;
	locker.Unlock();

	fEventCondition.NotifyAll();
	return B_OK;
}


void
EventQueue::Notify(select_event* event, uint16 events)
{
	if ((events & event->selected_events) == 0)
		return;

	int32 previousEvents = atomic_or(&event->events, events | B_EVENT_QUEUED);

	// If the event is already being deleted, we should ignore this
	// notification.
	if ((previousEvents & B_EVENT_DELETING) != 0)
		return;

	// If the event is already queued, and it is not becoming invalid, we
	// don't need to do anything more.
	if ((previousEvents & B_EVENT_QUEUED) != 0
			&& (events & B_EVENT_INVALID) == 0)
		return;

	{
		MutexLocker _(&fQueueLock);

		// We need to recheck B_EVENT_DELETING now we have the lock.
		if ((event->events & B_EVENT_DELETING) != 0)
			return;

		// If we get B_EVENT_INVALID it means the object we were monitoring was
		// deleted. The object's ID may now be reused, so we must remove it
		// from the event tree.
		if ((events & B_EVENT_INVALID) != 0)
			fEventTree.Remove(event);

		// If it's not already queued, it's our responsibility to queue it.
		if ((previousEvents & B_EVENT_QUEUED) == 0) {
			fEventList.Add(event);
			fQueueCondition.NotifyAll();
		}
	}
}


status_t
EventQueue::Wait(event_wait_info* infos, int numInfos, int32 flags,
	bigtime_t timeout)
{
	MutexLocker queueLocker(&fQueueLock);

	while ((fDequeueing || fEventList.IsEmpty()) && !fClosing) {
		ConditionVariableEntry entry;
		fQueueCondition.Add(&entry);

		queueLocker.Unlock();
		status_t status = entry.Wait(flags | B_CAN_INTERRUPT, timeout);
		queueLocker.Lock();

		if (status != B_OK && fEventList.IsEmpty())
			return status;
	}

	if (fClosing)
		return B_FILE_ERROR;

	if (numInfos == 0)
		return B_OK;

	return _DequeueEvents(infos, numInfos);
}


status_t
EventQueue::_SelectEvent(select_event* event, uint16 events, void* userData,
	MutexLocker& locker)
{
	ObjectDeleter<select_event> eventDeleter(event);

	event->user_data = userData;
	event->events = 0;
	event->pool_next = NULL;
	event->object_next = NULL;
	event->pool = NULL;

	event->events |= EVENT_BEHAVIOUR(events) | B_EVENT_SELECTING;
	event->selected_events = USER_EVENTS(events) | B_EVENT_NON_MASKABLE;

	locker.Unlock();

	int32 resultEvents = kSelectOps[event->type].select(event->object, event,
		fKernel);

	if (resultEvents < 0) {
		locker.Lock();
		fEventTree.Remove(event);
		fEventCondition.NotifyAll();
		return resultEvents;
	}

	eventDeleter.Detach();

	if (resultEvents > 0)
		Notify(event, resultEvents);

	atomic_and(&event->events, ~B_EVENT_SELECTING);
	fEventCondition.NotifyAll();

	return B_OK;
}


status_t
EventQueue::_DeselectEvent(select_event* event)
{
	return kSelectOps[event->type].deselect(event->object, event, fKernel);
}


ssize_t
EventQueue::_DequeueEvents(event_wait_info* infos, int numInfos)
{
	ssize_t count = 0;

	bool notifyWaiters = false;

	select_event* event;

	// We do not use the DoublyLinkedList iterator because it will not work
	// with a concurrent deletion in Deselect().
	for (event = fEventList.Head(); event != NULL && count < numInfos; count++) {
		int32 events = atomic_and(&event->events, ~B_EVENT_QUEUED);

		infos[count].object = event->object;
		infos[count].type = event->type;
		infos[count].user_data = event->user_data;
		infos[count].events = USER_EVENTS(events);

		if ((events & (B_EVENT_ONE_SHOT | B_EVENT_INVALID)) != 0) {
			fDequeueing = true;
			event->events = B_EVENT_DELETING;

			mutex_unlock(&fQueueLock);
			_DeselectEvent(event);
			mutex_lock(&fQueueLock);

			fDequeueing = false;
			notifyWaiters = true;

			// If the event is invalid, it has already been removed from the
			// tree.
			if ((events & B_EVENT_INVALID) == 0)
				fEventTree.Remove(event);

			// Remove from the list, delete, and advance to next event.
			select_event* next = fEventList.GetNext(event);
			fEventList.Remove(event);
			delete event;

			event = next;
			continue;
		}

		event = fEventList.GetNext(event);
	}

	// Dequeue the elements we copied out.
	if (event != NULL) {
		fEventList.RemoveAllBefore(event);
	} else {
		fEventList.RemoveAll();
	}

	// Wake any threads we blocked while dropping the lock to deselect events.
	if (notifyWaiters) {
		fEventCondition.NotifyAll();
		fQueueCondition.NotifyAll();
	}

	return count;
}


/*
 * Get the select_event for the given object and type. Must be called with the
 * queue lock held. This method will sleep if the event is undergoing selection
 * or deletion.
 */
select_event*
EventQueue::_GetEvent(int32 object, uint16 type)
{
	EventQueueTreeDefinition::Key key = { object, type };

	while (true) {
		select_event* event = fEventTree.Find(key);

		if (event == NULL)
			return NULL;

		if ((event->events & (B_EVENT_SELECTING | B_EVENT_DELETING)) == 0)
			return event;

		ConditionVariableEntry entry;
		fEventCondition.Add(&entry);

		mutex_unlock(&fQueueLock);
		fEventCondition.Wait();
		mutex_lock(&fQueueLock);

		// At this point the select_event might have been deleted, so we
		// need to refetch it.
	}
}


//	#pragma mark -- File descriptor ops



static status_t
event_queue_close(file_descriptor* descriptor)
{
	EventQueue* queue = (EventQueue*)descriptor->u.queue;
	queue->Closed();
	return B_OK;
}


static void
event_queue_free(file_descriptor* descriptor)
{
	EventQueue* queue = (EventQueue*)descriptor->u.queue;
	delete queue;
}


static status_t
get_queue_descriptor(int fd, bool kernel, file_descriptor*& descriptor)
{
	if (fd < 0)
		return EBADF;

	descriptor = get_fd(get_current_io_context(kernel), fd);
	if (descriptor == NULL)
		return EBADF;

	if (descriptor->type != FDTYPE_EVENT_QUEUE) {
		put_fd(descriptor);
		return B_BAD_VALUE;
	}

	return B_OK;
}


#define GET_QUEUE_FD_OR_RETURN(fd, kernel, descriptor)	\
	do {												\
		status_t getError = get_queue_descriptor(fd, kernel, descriptor); \
		if (getError != B_OK)							\
			return getError;							\
	} while (false)


static struct fd_ops sEventQueueFDOps = {
	NULL,	// fd_read
	NULL,	// fd_write
	NULL,	// fd_seek
	NULL,	// fd_ioctl
	NULL,	// fd_set_flags
	NULL,	// fd_select
		// The event queue could be made selectable - although that may raise
		// some problems with loops of event queues monitoring each other
		// causing infinite notifications.
	NULL,	// fd_deselect
	NULL,	// fd_read_dir
	NULL,	// fd_rewind_dir
	NULL,	// fd_read_stat
	NULL,	// fd_write_stat
	&event_queue_close,
	&event_queue_free
};


struct FDPutter {
	FDPutter(file_descriptor* descriptor)
		: descriptor(descriptor)
	{
	}

	~FDPutter()
	{
		if (descriptor != NULL)
			put_fd(descriptor);
	}

	file_descriptor*	descriptor;
};


//	#pragma mark - User syscalls


int
_user_event_queue_create(int openFlags)
{
	EventQueue* queue = new(std::nothrow) EventQueue(false);
	if (queue == NULL)
		return B_NO_MEMORY;

	ObjectDeleter<EventQueue> deleter(queue);

	queue->type = SYNC_TYPE_QUEUE;

	file_descriptor* descriptor = alloc_fd();
	if (descriptor == NULL)
		return B_NO_MEMORY;

	descriptor->type = FDTYPE_EVENT_QUEUE;
	descriptor->ops = &sEventQueueFDOps;
	descriptor->u.queue = (struct event_queue*)queue;
	descriptor->open_mode = O_RDWR | openFlags;

	io_context* context = get_current_io_context(false);
	int fd = new_fd(context, descriptor);
	if (fd < 0) {
		free(descriptor);
		return fd;
	}

	mutex_lock(&context->io_mutex);
	fd_set_close_on_exec(context, fd, (openFlags & O_CLOEXEC) != 0);
	mutex_unlock(&context->io_mutex);

	deleter.Detach();
	return fd;		
}


status_t
_user_event_queue_select(int queue, event_wait_info* userInfos, int numInfos)
{
	if (numInfos <= 0)
		return B_BAD_VALUE;

	if (userInfos == NULL || !IS_USER_ADDRESS(userInfos))
		return B_BAD_ADDRESS;

	file_descriptor* descriptor;
	GET_QUEUE_FD_OR_RETURN(queue, false, descriptor);
	FDPutter _(descriptor);

	EventQueue* eventQueue = (EventQueue*)descriptor->u.queue;

	event_wait_info* infos = new event_wait_info[numInfos];
	ArrayDeleter<event_wait_info> deleter(infos);

	if (user_memcpy(infos, userInfos, sizeof(event_wait_info) * numInfos)
			!= B_OK)
		return B_BAD_ADDRESS;

	status_t result = B_OK;

	for (int i = 0; i < numInfos; i++) {
		int32 action = infos[i].events & B_EVENT_ACTIONS;
		status_t error;
		switch (action) {
			case B_EVENT_SELECT:
				error = eventQueue->Select(infos[i].object, infos[i].type,
					infos[i].events, infos[i].user_data);
				break;
			case B_EVENT_MODIFY:
				error = eventQueue->Modify(infos[i].object, infos[i].type,
					infos[i].events, infos[i].user_data);
				break;
			case B_EVENT_DESELECT:
				error = eventQueue->Deselect(infos[i].object, infos[i].type);
				break;
			default:
				error = B_BAD_VALUE;
		}

		if (error != B_OK) {
			user_memcpy(&userInfos[i].events, &error, sizeof(&userInfos[i].events));
			result = B_ERROR;
		}
	}

	return result;
}


ssize_t
_user_event_queue_wait(int queue, event_wait_info* userInfos, int numInfos,
	uint32 flags, bigtime_t timeout)
{
	syscall_restart_handle_timeout_pre(flags, timeout);

	if (numInfos < 0)
		return B_BAD_VALUE;

	if (numInfos > 0 && (userInfos == NULL || !IS_USER_ADDRESS(userInfos)))
		return B_BAD_ADDRESS;

	file_descriptor* descriptor;
	GET_QUEUE_FD_OR_RETURN(queue, false, descriptor);
	FDPutter _(descriptor);

	EventQueue* eventQueue = (EventQueue*)descriptor->u.queue;

	// TODO: could allocate these infos on the stack if they are few enough.
	// Or perhaps the data should be individually user_memcpy'd from the
	// queue.
	event_wait_info* infos = new event_wait_info[numInfos];

	ssize_t result = eventQueue->Wait(infos, numInfos, flags, timeout);
	if (result < 0)
		return syscall_restart_handle_timeout_post(result, timeout);

	status_t status = user_memcpy(userInfos, infos, sizeof(event_wait_info)
		* numInfos);
	return status == B_OK ? result : status;
}


//	#pragma mark - Private kernel API


void
notify_event_queue(select_info* info, uint16 events)
{
	ASSERT(info->sync->type == SYNC_TYPE_QUEUE);

	EventQueue* queue = static_cast<EventQueue*>(info->sync);
	select_event* event = static_cast<select_event*>(info);

	queue->Notify(event, events);
}
