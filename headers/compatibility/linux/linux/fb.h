/*
 * Copyright 2015, Hamish Morrison, hamishm53@gmail.com.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef _LINUX_FB_H
#define _LINUX_FB_H


enum display_flags {
	DUMMY = 0
};

struct videomode {
	unsigned long pixelclock;
	uint32 hactive;
	uint32 hfront_porch;
	uint32 hback_porch;
	uint32 hsync_len;
	uint32 vactive;
	uint32 vfront_porch;
	uint32 vback_porch;
	uint32 vsync_len;

	enum display_flags flags;
};

#endif
