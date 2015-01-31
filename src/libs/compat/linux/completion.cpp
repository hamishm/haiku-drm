/*
 * Copyright 2015, Hamish Morrison, hamishm53@gmail.com.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
extern "C" {
#include <linux/completion.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/wait.h>
}

#include <thread.h>


extern "C" {

long
__wait_for_completion(struct completion* c, int state, long timeout)
{
	wait_queue_t wait;
	long result = 0;

	init_wait(&wait);

	for (;;) {
		if (thread_is_interrupted(thread_get_current_thread(), state)) {
			result = -ERESTARTSYS;
			break;
		}

		waitqueue_lock(&c->wait);
		if (c->done) {
			c->done--;
			waitqueue_unlock(&c->wait);
			break;
		}

		__waitqueue_prepare_locked(&c->wait, &wait, state, false);
		waitqueue_unlock(&c->wait);

		result = schedule_timeout(timeout);
		if (result <= 0)
			break;
	}

	finish_wait(&c->wait, &wait);
	return result;
}

}
