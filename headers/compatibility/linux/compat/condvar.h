/*
 * Copyright 2015, Hamish Morrison, hamishm53@gmail.com
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef _LINUX_COMPAT_CONDVAR_H
#define _LINUX_COMPAT_CONDVAR_H

#include <ConditionVariable.h>


typedef struct condition_variable {
	ConditionVariable cond;
} condition_variable;


#ifdef __cplusplus
extern "C" {
#endif


void cv_init(condition_variable* var, const char* name);
void cv_wait(condition_variable* var, mutex* lock, uint32 flags, bigtime_t timeout);
void cv_wake_one(condition_variable* var);
void cv_wake_all(condition_variable* var);


#ifdef __cplusplus
}
#endif


#endif
