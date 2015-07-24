/*
 * Copyright 2015, Hamish Morrison, hamishm53@gmail.com.
 * Copyright 2011, Axel Dörfler, axeld@pinc-software.de.
 * All rights reserved. Distributed under the terms of the MIT License.
 */

#include <AsyncSocket.h>

#include <EventDispatcher.h>


BAsyncSocket::BAsyncSocket(BEventDispatcher* dispatcher)
	:
	fDispatcher(dispatcher),
	fWaitingRecv(false),
	fWaitingSend(false),
	fWaitingSendAll(false),
	fWaitingConnect(false)
{
	fEventCallback = std::bind(&BAsyncSocket::_HandleEvents, this,
		std::placeholders::_1);
}


BAsyncSocket::~BAsyncSocket()
{
}


status_t
BAsyncSocket::ConnectAsync(const BNetworkAddress& peer,
	StatusCallback& callback)
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

	if (result == 0)
		return B_OK;

	if (errno != EWOULDBLOCK && errno != EAGAIN)
		return errno;

	result = fDispatcher->WaitForFD(fSocket, B_EVENT_WRITE, fEventCallback);
	if (result != B_OK)
		return result;

	fConnectCallback = &callback;
	fWaitingConnect = true;
	return EINPROGRESS;
}


ssize_t
BAsyncSocket::RecvAsync(void* buffer, size_t size, int flags,
	IOCallback& callback)
{
	if (fWaitingRecv)
		return EALREADY;

	fRecvRequest.buffer = buffer;
	fRecvRequest.size = size;
	fRecvRequest.flags = flags;
	fRecvRequest.callback = &callback;

	ssize_t result = _Recv(fSendRequest);
	if (result != EINPROGRESS)
		return result;

	fWaitingRecv = true;
	return EINPROGRESS;
}


ssize_t
BAsyncSocket::SendAsync(void* buffer, size_t size, int flags,
	IOCallback& callback)
{
	if (fWaitingSend || fWaitingSendAll || fWaitingConnect)
		return EALREADY;

	fSendRequest.buffer = buffer;
	fSendRequest.size = size;
	fSendRequest.flags = flags;
	fSendRequest.callback = &callback;

	ssize_t result = _Send(fSendRequest);
	if (result != EINPROGRESS)
		return result;

	fWaitingSendAll = true;
	return EINPROGRESS;
}


ssize_t
BAsyncSocket::SendAllAsync(void* buffer, size_t size, int flags,
	IOCallback& callback)
{
	if (fWaitingSend || fWaitingSendAll || fWaitingConnect)
		return EALREADY;

	fSendRequest.buffer = buffer;
	fSendRequest.size = size;
	fSendRequest.flags = flags;
	fSendRequest.callback = &callback;

	ssize_t result = _SendAll(fSendRequest);
	if (result != EINPROGRESS)
		return result;

	fWaitingSend = true;
	return EINPROGRESS;
}


ssize_t
BAsyncSocket::_Recv(IORequest& request)
{
	ssize_t bytesReceived = recv(fSocket, request.buffer, request.size,
		request.flags);
	if (bytesReceived >= 0)
		return bytesReceived;

	if (errno != EWOULDBLOCK && errno != EAGAIN)
		return errno;

	status_t result = fDispatcher->WaitForFD(fSocket, B_EVENT_READ,
		fEventCallback);
	if (result != B_OK)
		return result;

	return EINPROGRESS;
}


ssize_t
BAsyncSocket::_Send(IORequest& request)
{
	ssize_t bytesSent = send(fSocket, request.buffer, request.size,
		request.flags);
	if (bytesSent >= 0)
		return bytesSent;

	if (errno != EWOULDBLOCK && errno != EAGAIN)
		return errno;

	status_t result = fDispatcher->WaitForFD(fSocket, B_EVENT_WRITE,
		fEventCallback);
	if (result != B_OK)
		return result;

	return EINPROGRESS;
}


ssize_t
BAsyncSocket::_SendAll(IORequest& request)
{
	ssize_t bytesSent = send(fSocket, request.buffer, request.size,
		request.flags);
	if ((size_t)bytesSent == request.size)
		return bytesSent;

	if (bytesSent == -1 && errno != EWOULDBLOCK && errno != EAGAIN)
		return errno;

	if (bytesSent == -1)
		bytesSent = 0;

	request.buffer = (void*)((char*)request.buffer + bytesSent);
	request.size -= bytesSent;

	status_t result = fDispatcher->WaitForFD(fSocket, B_EVENT_WRITE,
		fEventCallback);
	if (result != B_OK)
		return result;

	return EINPROGRESS;
}


void
BAsyncSocket::_HandleEvents(int events)
{
	if ((events & B_EVENT_READ) != 0 && fWaitingRecv)
		_HandleRecv();

	if ((events & B_EVENT_WRITE) != 0) {
		if (fWaitingConnect)
			_HandleConnect();
		else if (fWaitingSendAll)
			_HandleSendAll();
		else if (fWaitingSend)
			_HandleSend();
	}
}


void
BAsyncSocket::_HandleRecv()
{
	ssize_t bytesReceived = _Recv(fRecvRequest);
	if (bytesReceived == EINPROGRESS)
		return;

	fWaitingRecv = false;
	(*fRecvRequest.callback)(bytesReceived);
}


void
BAsyncSocket::_HandleSend()
{
	ssize_t bytesSent = _Send(fSendRequest);
	if (bytesSent == EINPROGRESS)
		return;

	fWaitingSend = false;
	(*fSendRequest.callback)(bytesSent);
}


void
BAsyncSocket::_HandleSendAll()
{
	ssize_t bytesSent = _SendAll(fSendRequest);
	if (bytesSent == EINPROGRESS)
		return;

	fWaitingSendAll = false;
	(*fSendRequest.callback)(bytesSent);
}

void
BAsyncSocket::_HandleConnect()
{
	fWaitingConnect = false;
	(*fConnectCallback)(0);
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

	fWaitingRecv = false;
	fWaitingSend = false;
	fWaitingSendAll = false;
	fWaitingConnect = false;
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
