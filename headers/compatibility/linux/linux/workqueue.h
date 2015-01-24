/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2015 Hamish Morrison
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef	_LINUX_WORKQUEUE_H_
#define	_LINUX_WORKQUEUE_H_

#include <linux/list.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/timer.h>


struct workqueue_struct;

typedef void (*work_func_t)(struct work_struct *work);

struct work_struct {
	struct workqueue_struct* queue;
	struct list_head list;
	bool linked;
	work_func_t fn;
};

struct delayed_work {
	struct work_struct work;
	struct timer event;
};


#define	INIT_WORK(work, func) 	 			\
do {										\
	(work)->fn = (func);					\
	(work)->taskqueue = NULL;				\
	INIT_LIST_HEAD(&(work)->list);			\
} while (0)

#define	INIT_DELAYED_WORK 		INIT_WORK
#define	INIT_DEFERRABLE_WORK	INIT_DELAYED_WORK


/* TODO!!
#define	schedule_work(work)						\
do {									\
	(work)->taskqueue = taskqueue_thread[mycpuid];				\
	taskqueue_enqueue(taskqueue_thread[mycpuid], &(work)->work_task);	\
} while (0)

#define	flush_scheduled_work()	flush_taskqueue(taskqueue_thread[mycpuid])

static inline bool schedule_delayed_work(struct delayed_work *dwork,
                                         unsigned long delay)
{
        struct workqueue_struct wq;
        wq.taskqueue = taskqueue_thread[mycpuid];
        return queue_delayed_work(&wq, dwork, delay);
}
*/


extern bool queue_work(struct workqueue_struct *wq, struct work_struct *work);

extern bool queue_delayed_work(struct workqueue_struct *wq,
	struct delayed_work *work, unsigned long delay);

extern struct workqueue_struct * _workqueue_create(const char* name,
	int threads);

static inline struct workqueue_struct *
create_singlethread_workqueue(const char *name) {
	return common_create_workqueue(name, 1);
}

static inline struct workqueue_struct *
create_workqueue(const char *name) {
	return common_create_workqueue(name, smp_get_num_cpus());
}

extern void destroy_workqueue(struct workqueue_struct *wq);
extern void flush_workqueue(struct workqueue_struct *wq);

extern bool cancel_work_sync(struct work_struct *work);
extern bool cancel_delayed_work(struct delayed_work *work);
extern bool cancel_delayed_work_sync(struct delayed_work *work);

#endif	/* _LINUX_WORKQUEUE_H_ */
