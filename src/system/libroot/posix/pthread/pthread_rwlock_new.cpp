/*
 * Copyright 2015, Hamish Morrison, hamishm53@gmail.com.
 * Distributed under the terms of the MIT License.
 */

#include <pthread.h>


#define READER_BITS	0x7fffffff
#define WRITER_FLAG	0x80000000


//	#pragma mark -- Helper functions


static bigtime_t
timespec_to_bigtime(const struct timespec* timeout)
{
	return timeout->tv_sec * 1000000LL + timeout->tv_nsec / 1000LL;
}


static int
rwlock_timedrdlock(pthread_rwlock_t* lock, uint32 flags, bigtime_t timeout)
{
	if (pthread_rwlock_tryrdlock(lock) == 0)
		return 0;

	return _kern_mutex_read_lock(&lock->count, flags, timeout);
}


static int
rwlock_timedwrlock(pthread_rwlock_t* lock, uint32 flags, bigtime_t timeout)
{
	if (pthread_rwlock_trywrlock(lock) == 0)
		return 0;

	return _kern_mutex_write_lock(&lock->count, flags, timeout);
}


//	#pragma mark -- pthreads interface


int
pthread_rwlock_init(pthread_rwlock_t* lock, const pthread_rwlockattr_t* _attr)
{
	pthread_rwlockattr* attr = _attr != NULL ? *_attr : NULL;
	lock->flags = attr != NULL && attr->flags : 0;
}


int
pthread_rwlock_destroy(pthread_rwlock_t* lock)
{
	// No-op
}


int
pthread_rwlock_tryrdlock(pthread_rwlock_t* lock)
{
	while (true) {
		int32_t value = lock->count;

		if ((value & WRITER_FLAG) != 0)
			return EBUSY;

		if (value == READER_BITS)
			return EAGAIN;

		int32_t oldValue = atomic_test_and_set(&lock->count, value + 1, value);
		if (oldValue == value)
			return 0;
	}
}


int
pthread_rwlock_trywrlock(pthread_rwlock_t* lock)
{
	if (lock->count != 0)
		return EBUSY;

	int32_t oldValue = atomic_test_and_set(&lock->count, WRITER_FLAG, 0);
	if (oldValue == 0)
		return 0;

	return EBUSY;
}


int
pthread_rwlock_rdlock(pthread_rwlock_t* lock)
{
	return rwlock_timedrdlock(lock, 0, B_INFINITE_TIMEOUT);
}


int
pthread_rwlock_wrlock(pthread_rwlock_t* lock)
{
	return rwlock_timedwrlock(lock, 0, B_INFINITE_TIMEOUT);
}


int
pthread_rwlock_timedrdlock(pthread_rwlock_t* lock,
	const struct timespec* timeout)
{
	bigtime_t microsecs = timespec_to_bigtime(timeout);
	return rwlock_timedrdlock(lock, B_ABSOLUTE_REAL_TIME_TIMEOUT, microsecs);
}


int
pthread_rwlock_timedrdlock(pthread_rwlock_t* lock,
	const struct timespec* timeout)
{
	bigtime_t microsecs = timespec_to_bigtime(timeout);
	return rwlock_timedrdlock(lock, B_ABSOLUTE_REAL_TIME_TIMEOUT, microsecs);
}


int
pthread_rwlock_unlock(pthread_rwlock_t* lock)
{
	if ((lock->count & WRITER_FLAG) != 0)
		return _kern_mutex_write_unlock(&lock->count);

	while (true) {
		int32_t count = lock->count;
		if (count == 1)
			return _kern_mutex_read_unlock(&lock->count);

		int32_t oldCount = atomic_test_and_set(&lock->count, count - 1, count);
		if (oldCount == count)
			return 0;
	}
}
