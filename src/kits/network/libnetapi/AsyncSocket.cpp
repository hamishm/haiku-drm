/*
 * Copyright 2015, Hamish Morrison, hamishm53@gmail.com.
 * Copyright 2011, Axel Dörfler, axeld@pinc-software.de.
 * All rights reserved. Distributed under the terms of the MIT License.
 */

#include <AsyncSocket.h>

#include <EventDispatcher.h>


BAsyncSocket::BAsyncSocket(BEventDispatcher* dispatcher)
	:
	fEventDispatcher(dispatcher)
{
}


BAsyncSocket::~BAsyncSocket()
{
}


status_t
BAsyncSocket::_Connect(const BNetworkAddress& peer)
{
	Disconnect();

	fInitStatus = _OpenIfNeeded(peer.Family(), SOCK_STREAM | SOCK_NONBLOCK);
	if (fInitStatus == B_OK && !IsBound()) {
		BNetworkAddress local;
		local.SetToWildcard(peer.Family());
		fInitStatus = Bind(local);
	}

	if (fInitStatus != B_OK)
		return fInitStatus;

	BNetworkAddress normalized = peer;
	int result = connect(fSocket, normalized, normalized.Length());

	if (result == -1)
		return errno;  // TODO!

	return result;
}


void
BAsyncSocket::_SetTo(int fd, const BNetworkAddress& local,
	const BNetworkAddress& peer)
{
	Disconnect();

	fInitStatus = B_OK;
	fSocket = fd;
	fLocal = local;
	fPeer = peer;
}


status_t
BAsyncSocket::_GetSocketError()
{
	int error;
	socklen_t len = sizeof(error);
	if (getsockopt(fSocket, SOL_SOCKET, SO_ERROR, (void*)&error, &len) != 0)
		return errno;

	return error;
}
