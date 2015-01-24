/*
 * Copyright 2015 Hamish Morrison, hamishm53@gmail.com.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef _LINUX_KDEV_T_H
#define _LINUX_KDEV_T_H

#define MAJOR(devt) ((devt) >> 20)
#define MINOR(devt) ((devt) & 0xfffff)
#define MKDEV(major, minor) (((major) << 20) | (minor))

#endif
