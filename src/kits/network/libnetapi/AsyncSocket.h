/*
 * Copyright 2015, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */

#include <AsyncSocket.h>

#include <EventDispatcher.h>


BAsyncSocket::BAsyncSocket(BEventDispatcher* dispatcher)
	:
	fDispatcher(dispatcher)
{
}


BAsyncSocket::~BAsyncSocket()
{
}


status_t
BAsyncSocket::Connect(const BNetworkAddress& peer, int type,
	ConnectCallback callback, bigtime_t timeout)
{
	Disconnect();

	fInitStatus = _OpenIfNeeded(peer.Family(), type | SOCK_NONBLOCK)
	if (fInitStatus == B_OK && !IsBound()) {
		BNetworkAddress local;
		local.SetToWildcard(peer.Family());
		fInitStatus = Bind(local);
	}

	if (fInitStatus != B_OK)
		return fInitStatus;

	BNetworkAddress normalized = peer;
	if (connect(fSocket, normalized, normalized.Length()) != 0) {
		if (errno != EAGAIN && errno != EWOULDBLOCK)
			return fInitStatus = errno;

		fDispatcher->WaitForFD(fSocket, B_EVENT_WRITE,
			
		TRACE("%p: connecting to %s: %s\n", this,
			normalized.ToString().c_str(), strerror(errno));
}


	typedef std::function<void (ssize_t)> ReadWriteCallback;

	virtual	ssize_t				ReadAsync(void* buffer, size_t size,
									ReadWriteCallback callback,
									bigtime_t timeout = B_INFINITE_TIMEOUT);

	virtual	ssize_t				WriteAsync(void* buffer, size_t size,
									ReadWriteCallbck callback,
									bigtime_t timeout = B_INFINITE_TIMEOUT);
private:
	friend class BServerSocket;

			void				_SetTo(int fd, const BNetworkAddress& local,
									const BNetworkAddress& peer);

			BEventDispatcher*	fDispatcher;
};


#endif	// _SOCKET_H
