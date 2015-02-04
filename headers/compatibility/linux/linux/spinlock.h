/*
 * Copyright 2015, Hamish Morrison, hamishm53@gmail.com.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef _LINUX_SPINLOCK_H
#define _LINUX_SPINLOCK_H

#include <KernelExport.h>


struct spinlock {
	spinlock lock;
	cpu_status state;
};

struct rwlock {
	rw_spinlock lock;
	cpu_status state;
};

typedef struct spinlock spinlock_t;
typedef struct rwlock rwlock_t;


#define DEFINE_SPINLOCK(name)	\
	spinlock_t (name) = { B_SPINLOCK_INITIALIZER, 0 }


static inline void
spin_lock_init(spinlock_t* lock)
{
	B_INITIALIZE_SPINLOCK(&lock->lock);
}

static inline void
spin_lock(spinlock_t* lock)
{
	acquire_spinlock(&lock->lock);
}

static inline void
spin_unlock(spinlock_t* lock)
{
	release_spinlock(&lock->lock);
}

static inline void
spin_lock_irq(spinlock_t* lock)
{
	lock->state = disable_interrupts();
	acquire_spinlock(&lock->lock);
}

static inline void
spin_unlock_irq(spinlock_t* lock)
{
	release_spinlock(&lock->lock);
	restore_interrupts(lock->state);
}

#define spin_lock_irqsave(lock, flags)				\
do {												\
	(flags) = (unsigned long)disable_interrupts();	\
	acquire_spinlock(&(lock)->lock);				\
while (0)

static inline void
spin_unlock_irqrestore(spinlock_t* lock, unsigned long flags)
{
	release_spinlock(&lock->lock);
	restore_interrupts((cpu_status)flags);
}


#define spin_lock_bh	spin_lock_irq
#define spin_unlock_bh	spin_unlock_irq


#define DEFINE_RWLOCK(name)			\
	rwlock_t (name) = { B_RW_SPINLOCK_INITIALIZER, 0 }


static inline void
rwlock_init(rwlock_t* lock)
{
	B_INITIALIZE_RW_SPINLOCK(&lock->lock);
}


static inline void
read_lock(rwlock_t* lock)
{
	acquire_read_spinlock(&lock->lock);
}

static inline void
write_lock(rwlock_t* lock)
{
	acquire_write_spinlock(&lock->lock);
}

static inline void
read_unlock(rwlock_t* lock)
{
	release_read_spinlock(&lock->lock);
}

static inline void
write_unlock(rwlock_t* lock)
{
	release_write_spinlock(&lock->lock);
}

static inline void
read_trylock(rwlock_t* lock)
{
	try_acquire_read_spinlock(&lock->lock);
}

static inline void
write_trylock(rwlock_t* lock)
{
	try_acquire_write_spinlock(&lock->lock);
}

static inline void
write_lock_irq(rwlock_t* lock)
{
	lock->state = disable_interrupts();
	write_lock(lock);
}

static inline void
write_unlock_irq(rwlock_t* lock)
{
	write_unlock(lock);
	restore_interrupts(lock->state);
}

static inline void
read_lock_irq(rwlock_t* lock)
{
	lock->state = disable_interrupts();
	write_lock(lock);
}

static inline void
read_unlock_irq(rwlock_t* lock)
{
	read_unlock(lock);
	restore_interrupts(lock->state);
}

#define read_lock_irqsave(lock, flags)		\
	do {									\
		(flags) = disable_interrupts();		\
		read_lock(lock);					\
	} while (0)

#define write_lock_irqsave(lock, flags)		\
	do {									\
		(flags) = disable_interrupts();		\
		write_lock(lock);					\
	} while (0)

static inline void
read_unlock_irqrestore(rwlock_t* lock, unsigned long flags)
{
	read_unlock(lock);
	restore_interrupts((cpu_status)flags);
}

static inline void
write_unlock_irqrestore(rwlock_t* lock, unsigned long flags)
{
	write_unlock(lock);
	restore_interrupts((cpu_status)flags);
}

#define write_lock_bh	write_lock_irq
#define write_unlock_bh	write_unlock_irq
#define read_lock_bh	read_lock_irq
#define read_unlock_bh	read_unlock_irq

#endif
