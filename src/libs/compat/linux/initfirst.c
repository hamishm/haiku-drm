/*
 * Copyright 2015, Hamish Morrison, hamishm53@gmail.com
 * All rights reserved. Distributed under the terms of the MIT License.
 */

#include <linux/init.h>
#include <stddef.h>


static initcall_t __attribute__ ((section(".module_init")))
	__dummy_first_init = NULL;
