/*
 * Copyright 2015, Hamish Morrison, hamishm53@gmail.com.
 * All rights reserved. Distributed under the terms of the MIT License.
 */

#include <Callback.h>
#include <HttpHeaders.h>
#include <HTTPStream.h>
#include <Url.h>


BHTTPCache::BHTTPCache()
{
}


BHTTPCache::~BHTTPCache()
{
}


status_t
BHTTPCache::CreateStream(HTTPStream*& stream, const BUrl& url,
	const BHttpHeaders& headers, BEventDispatcher* dispatcher,
	const StatusCallback& callback)
{
	HTTPNetworkStream* networkStream = new(std::nothrow) HTTPNetworkStream(
		fContext, dispatcher, url, callback);

	if (networkStream == NULL)
		return B_NO_MEMORY;

	if (networkStream->IsConnected())
		return B_OK;

	return EINPROGRESS;
}


status_t
BHTTPCache::ReleaseStream(HTTPStream* stream)
{
}
