/*
 * Copyright 2015, Haiku Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _HTTP_CACHE_H
#define _HTTP_CACHE_H

#include <Callback.h>
#include <HttpHeaders.h>
#include <HTTPStream.h>
#include <Url.h>


class BHTTPCache {
public:
								BHTTPCache();
	virtual						~BHTTPCache();

			status_t			CreateStream(HTTPStream*& stream,
									const BUrl& url,
									const BHttpHeaders& headers,
									const StatusCallback& callback);

			status_t			ReleaseStream(HTTPStream* stream);
};


#endif // _HTTP_CACHE_H
