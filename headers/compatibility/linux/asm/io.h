/*
 * Copyright (c) 2015 Hamish Morrison
 * Copyright (c) 2014 François Tigeot
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

#ifndef _ASM_IO_H_
#define _ASM_IO_H_


#define ioread8(addr)		*(volatile uint8_t *)((char *)addr)
#define ioread16(addr)		*(volatile uint16_t *)((char *)addr)
#define ioread32(addr)		*(volatile uint32_t *)((char *)addr)

#define iowrite8(data, addr)	*(volatile uint8_t *)((char *)addr) = data;
#define iowrite16(data, addr)	*(volatile uint16_t *)((char *)addr) = data;
#define iowrite32(data, addr)	*(volatile uint32_t *)((char *)addr) = data;

static inline void __iomem *map_common(resource_size_t addr, unsigned long size,
	uint32 flags)
{
	void* address;
	area_id area = map_physical_memory("lio", addr, size, flags,
		B_READ_AREA | B_WRITE_AREA, &address);
	if (area < 0)
		return NULL;

	return address;
}

/* ioremap: map bus memory into CPU space */
static inline void __iomem *ioremap(resource_size_t phys_addr, unsigned long size)
{
	return map_common(phys_addr, size, B_ANY_KERNEL_ADDRESS | B_MTR_UC);
}

static inline void __iomem *ioremap_wc(resource_size_t phys_addr, unsigned long size)
{
	return map_common(phys_addr, size, B_ANY_KERNEL_ADDRESS | B_MTR_WC);
}

#endif	/* _ASM_IO_H_ */
