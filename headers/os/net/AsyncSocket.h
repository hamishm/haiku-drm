/*
 * Copyright 2015, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _ASYNC_SOCKET_H
#define _ASYNC_SOCKET_H

#include <EventDispatcher.h>
#include <Socket.h>


class BEventDispatcher;


namespace ph = std::placeholders;


class BAsyncSocket : public BSocket {
public:
								BAsyncSocket(BEventDispatcher* dispatcher);
	virtual						~BAsyncSocket();

			template<typename Callback>
			status_t			ConnectAsync(const BNetworkAddress& peer,
									Callback&& callback);

			template<typename Callback>
			ssize_t				ReadAsync(void* buffer, size_t size,
									Callback&& callback);

			template<typename Callback>
			ssize_t				WriteAsync(void* buffer, size_t size,
									Callback&& callback);

			template<typename Callback>
			ssize_t				WriteAllAsync(void* buffer, size_t size,
									Callback&& callback);
private:
	friend class BServerSocket;

			void				_SetTo(int fd, const BNetworkAddress& local,
									const BNetworkAddress& peer);

			status_t			_Connect(const BNetworkAddress& peer);

			status_t			_GetSocketError();

			template<typename Callback>
			void				_HandleRead(int events, void* buffer,
									size_t size, Callback&& callback);

			template<typename Callback>
			void				_HandleWrite(int events, void* buffer,
									size_t size, Callback&& callback);

			template<typename Callback>
			void				_HandleWriteAll(int events, void* buffer,
									size_t size, Callback&& callback);

private:
			BEventDispatcher*	fEventDispatcher;
};


template<typename Callback>
inline status_t
BAsyncSocket::ConnectAsync(const BNetworkAddress& peer, Callback&& callback)
{
	status_t result = _Connect(peer);
	if (result != B_WOULD_BLOCK)
		return result;

	result = fEventDispatcher->WaitForFD(fSocket, B_EVENT_WRITE,
		[this, peer, callback](int events) -> void {
			if ((events & B_EVENT_ERROR) != 0)
				callback(_GetSocketError());
			else {
				fInitStatus = B_OK;
				fIsConnected = true;
				fPeer = peer;
				_UpdateLocalAddress();
				callback(0);
			}
		});

	if (result != B_OK)
		return result;

	return B_WOULD_BLOCK;
}


template<typename Callback>
inline ssize_t
BAsyncSocket::ReadAsync(void* buffer, size_t size, Callback&& callback)
{
	ssize_t bytesReceived = recv(fSocket, buffer, size, 0);
	if (bytesReceived >= 0)
		return bytesReceived;

	if (errno != EWOULDBLOCK && errno != EAGAIN)
		return errno;  // TODO!

	status_t result = fEventDispatcher->WaitForFD(fSocket, B_EVENT_READ,
		std::bind(_HandleRead, this, ph::_1, buffer, size, callback));

	if (result != B_OK)
		return result;

	return B_WOULD_BLOCK;
}


template<typename Callback>
inline status_t
BAsyncSocket::WriteAsync(void* buffer, size_t size, Callback&& callback)
{
	ssize_t bytesSent = send(fSocket, buffer, size, 0);
	if (bytesSent >= 0)
		return bytesSent;

	if (errno != EWOULDBLOCK && errno != EAGAIN)
		return errno;  // TODO!

	status_t result = fEventDispatcher->WaitForFD(fSocket, B_EVENT_READ,
		std::bind(_HandleWrite, this, ph::_1, buffer, size, callback));

	if (result != B_OK)
		return result;

	return B_WOULD_BLOCK;
}


template<typename Callback>
inline status_t
BAsyncSocket::WriteAllAsync(void* buffer, size_t size, Callback&& callback)
{

	ssize_t bytesSent = send(fSocket, buffer, size, 0);
	if (bytesSent == size || bytesSent == 0)
		return bytesSent;

	if (errno != EWOULDBLOCK && errno != EAGAIN)
		return errno;  // TODO!

	ssize_t remaining = bytesSent > 0 ? size - bytesSent : size;

	status_t result = fEventDispatcher->WaitForFD(fSocket, B_EVENT_READ,
		std::bind(_HandleWriteAll, this, ph::_1, buffer, remaining, callback));

	if (result != B_OK)
		return result;

	return B_WOULD_BLOCK;	
}


template<typename Callback>
inline void
BAsyncSocket::_HandleRead(int events, void* buffer, size_t size,
	Callback&& callback)
{
	if ((events & B_EVENT_ERROR) != 0) {
		callback(_GetSocketError());
		return;
	}

	// Try again
	ssize_t bytesRead = ReadAsync(buffer, size, callback);
	if (bytesRead == B_WOULD_BLOCK)
		return;

	callback(bytesRead);
}


template<typename Callback>
inline void
BAsyncSocket::_HandleWrite(int events, void* buffer, size_t size,
	Callback&& callback)
{
	if ((events & B_EVENT_ERROR) != 0) {
		callback(_GetSocketError());
		return;
	}

	// Try again
	ssize_t bytesWritten = WriteAsync(buffer, size, callback);
	if (bytesWritten == B_WOULD_BLOCK)
		return;

	callback(bytesWritten);
}


template<typename Callback>
inline void
BAsyncSocket::_HandleWriteAll(int events, void* buffer, size_t size,
	Callback&& callback)
{
	if ((events & B_EVENT_ERROR) != 0) {
		callback(_GetSocketError());
		return;
	}

	// Try again
	ssize_t bytesWritten = WriteAsyncAll(buffer, size, callback);
	if (bytesWritten == B_WOULD_BLOCK)
		return;

	callback(bytesWritten);
}


#endif	// _ASYNC_SOCKET_H
