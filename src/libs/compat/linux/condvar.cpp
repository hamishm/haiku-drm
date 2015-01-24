/*
 * Copyright 2015, Hamish Morrison, hamishm53@gmail.com
 * All rights reserved. Distributed under the terms of the MIT License.
 */

extern "C" {
#include <compat/condvar.h>
}

#include <ConditionVariable.h>

extern "C" {

void
cv_init(condition_variable* var, const char* name)
{
	var->cond.Init(var, name);
}


void
cv_wait(condition_variable* var, mutex* lock, uint32 flags, bigtime_t timeout)
{
	ConditionVariableEntry entry;

	var->cond.Add(&entry);
	mutex_unlock(lock);

	var->cond.Wait(flags, timeout);

	mutex_lock(lock);
}


void
cv_wake_one(condition_variable* var)
{
	var->cond.NotifyOne();
}


void
cv_wake_all(condition_variable* var)
{
	var->cond.NotifyAll();
}

}
