/*
 * Copyright 2015, Hamish Morrison, hamishm53@gmail.com
 * All rights reserved. Distributed under the terms of the MIT License.
 */

extern "C" {
#include <linux/sched.h>
}

#include <thread.h>


extern "C" {

long
schedule_timeout(long timeout)
{
	if (timeout < 0)
		return 0;

	bigtime_t start = system_time();
	thread_block_with_timeout(B_RELATIVE_TIMEOUT, timeout);
	bigtime_t now = system_time();

	return max_c(timeout - (now - start), 0L);
}


void
schedule()
{
	thread_block();
}

}
