/* Realtek RTL8169 Family Driver
 * Copyright (C) 2004 Marcus Overhagen <marcus@overhagen.de>. All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and its 
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appear in all copies, and that both the
 * copyright notice and this permission notice appear in supporting documentation.
 *
 * Marcus Overhagen makes no representations about the suitability of this software
 * for any purpose. It is provided "as is" without express or implied warranty.
 *
 * MARCUS OVERHAGEN DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
 * ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL MARCUS
 * OVERHAGEN BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY
 * DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifndef __DEBUG_H
#define __DEBUG_H

#include <KernelExport.h>

// #define PROFILING

#ifdef DEBUG
	#define TRACE(a...) dprintf("rtl8169: " a)
	#define ASSERT(a)	if (a) ; else panic("rtl8169: ASSERT failed, " #a)
#else
	#define TRACE(a...)
	#define ASSERT(a...)
#endif

#ifdef PROFILING
	#define PROFILING_ONLY(a)	a
#else
	#define PROFILING_ONLY(a)
#endif

#define ERROR(a...) dprintf("rtl8169: ERROR " a)
#define PRINT(a...) dprintf("rtl8169: " a)

#endif
