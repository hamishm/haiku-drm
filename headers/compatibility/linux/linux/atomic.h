/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013-2015 François Tigeot
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

#ifndef	_ASM_ATOMIC_H_
#define	_ASM_ATOMIC_H_

#include <SupportDefs.h>
#include <arch/atomic.h>
#include <sys/types.h>
#include <linux/compiler.h>

typedef struct {
	uint32 counter;
} atomic_t;

typedef struct {
	uint64 counter;
} atomic64_t;

#define	atomic_inc_return(v)		atomic_add_return(1, (v))
#define	atomic_add_negative(i, v)	(atomic_add_return((i), (v)) < 0)
#define	atomic_sub_and_test(i, v)	(atomic_sub_return((i), (v)) == 0)
#define	atomic_dec_and_test(v)		(atomic_sub_return(1, (v)) == 0)
#define	atomic_inc_and_test(v)		(atomic_add_return(1, (v)) == 0)
#define atomic_dec_return(v)             atomic_sub_return(1, (v))

#define atomic_xchg(p, v)		atomic_get_and_set(&((p)->counter), v)
#define atomic64_xchg(p, v)		atomic_get_and_set64(&((p)->counter), v)

static inline int
atomic_add_return(int i, atomic_t *v)
{
	return i + atomic_add(&v->counter, i);
}

static inline int
atomic_sub_return(int i, atomic_t *v)
{
	return atomic_add(&v->counter, -i) - i;
}

static inline void
linux_atomic_set(atomic_t *v, int i)
{
	atomic_set(&v->counter, i);
}

static inline void
atomic64_set(atomic64_t *v, long i)
{
	atomic_set64(&v->counter, i);
}

static inline int
atomic_read(atomic_t *v)
{
	return atomic_get(&v->counter);
}

static inline int64_t
atomic64_read(atomic64_t *v)
{
	return atomic_get64(&v->counter);
}

static inline int
atomic_inc(atomic_t *v)
{
	return atomic_add(&v->counter, 1) + 1;
}

static inline int
atomic_dec(atomic_t *v)
{
	return atomic_add(&v->counter, -1) - 1;
}

static inline int atomic_add_unless(atomic_t *v, int a, int u)
{
        int c, old;
        c = atomic_read(v);
        for (;;) {
                if (unlikely(c == (u)))
                        break;
                // old = atomic_cmpxchg((v), c, c + (a)); /*Linux*/
                old = atomic_test_and_set(&v->counter, c + (a), c);
                if (likely(old == c))
                        break;
                c = old;
        }
        return c != (u);
}

#define atomic_inc_not_zero(v) atomic_add_unless((v), 1, 0)

static inline void
atomic_clear_mask(int mask, atomic_t *v)
{
	atomic_and(&v->counter, ~mask);
}

static inline void
atomic_set_mask(int mask, atomic_t *v)
{
	atomic_or(&v->counter, mask);
}

#define	atomic_sub(i, v)		atomic_sub_return((i), (v))

/*
#undef atomic_add
#undef atomic_set
#define	atomic_add(i, v)		atomic_add_return((i), (v))
#define atomic_set(v, i)		linux_atomic_set((v), (i))
*/

#endif	/* _ASM_ATOMIC_H_ */
