/*
 * Copyright 2015 Hamish Morrison, hamishm53@gmail.com.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef _LINUX_INIT_H
#define _LINUX_INIT_H


typedef int (*initcall_t)(void);


#define __init

#define module_init(func) \
	static initcall_t __attribute__ ((section(".module_init"))) \
		__init_##func = func;


#endif
