/*
 * Copyright 2010-2014 Haiku Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Christophe Huriaux, c.huriaux@gmail.com
 *		Niels Sascha Reedijk, niels.reedijk@gmail.com
 *		Adrien Destugues, pulkomandy@pulkomandy.tk
 */


#include <NetworkRequest.h>

#include <AbstractSocket.h>


BNetworkRequest::BNetworkRequest(const BUrl& url, BUrlProtocolListener* listener,
		BUrlContext* context)
	:
	BUrlRequest(url, listener, context),
	fSocket(NULL)
{
}


status_t
BNetworkRequest::Stop()
{
	status_t threadStatus = BUrlRequest::Stop();

	if (threadStatus != B_OK)
		return threadStatus;

	send_signal(fThreadId, SIGUSR1); // unblock blocking syscalls.
	wait_for_thread(fThreadId, &threadStatus);
	return threadStatus;
}


void
BNetworkRequest::SetTimeout(bigtime_t timeout)
{
	// TODO!
}


bool
BNetworkRequest::_ResolveHostName(BString host, uint16_t port)
{
	_EmitDebug(B_URL_PROTOCOL_DEBUG_TEXT, "Resolving %s",
		fUrl.UrlString().String());

	fRemoteAddr = BNetworkAddress(host, port);
	if (fRemoteAddr.InitCheck() != B_OK)
		return false;

	//! ProtocolHook:HostnameResolved
	if (fListener != NULL)
		fListener->HostnameResolved(this, fRemoteAddr.ToString().String());

	_EmitDebug(B_URL_PROTOCOL_DEBUG_TEXT, "Hostname resolved to: %s",
		fRemoteAddr.ToString().String());

	return true;
}


static void
empty(int)
{
}


void
BNetworkRequest::_ProtocolSetup()
{
}


status_t
BNetworkRequest::_GetLine(BString& destString)
{
	// Find a complete line in inputBuffer
	uint32 characterIndex = 0;

	while ((characterIndex < fInputBuffer.Size())
		&& ((fInputBuffer.Data())[characterIndex] != '\n'))
		characterIndex++;

	if (characterIndex == fInputBuffer.Size())
		return B_ERROR;

	char* temporaryBuffer = new(std::nothrow) char[characterIndex + 1];
	if (temporaryBuffer == NULL)
		return B_NO_MEMORY;

	fInputBuffer.RemoveData(temporaryBuffer, characterIndex + 1);

	// Strip end-of-line character(s)
	if (temporaryBuffer[characterIndex - 1] == '\r')
		destString.SetTo(temporaryBuffer, characterIndex - 1);
	else
		destString.SetTo(temporaryBuffer, characterIndex);

	delete[] temporaryBuffer;
	return B_OK;
}


