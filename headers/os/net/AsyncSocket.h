/*
 * Copyright 2015, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _ASYNC_SOCKET_H
#define _ASYNC_SOCKET_H

#include <EventDispatcher.h>
#include <Socket.h>


class BEventDispatcher;


class BAsyncSocket : public BSocket {
public:
								BAsyncSocket(BEventDispatcher* dispatcher);
	virtual						~BAsyncSocket();

			status_t			ConnectAsync(const BNetworkAddress& peer,
									StatusCallback& callback);

			ssize_t				RecvAsync(void* buffer, size_t size, int flags,
									IOCallback& callback);

			ssize_t				SendAsync(void* buffer, size_t size, int flags,
									IOCallback& callback);

			ssize_t				SendAllAsync(void* buffer, size_t size, int flags,
									IOCallback& callback);
private:
	friend class BServerSocket;

			struct IORequest;

			ssize_t				_Recv(IORequest& request);
			ssize_t				_Send(IORequest& request);
			ssize_t				_SendAll(IORequest& request);

			void				_HandleEvents(int events);
			void				_HandleRecv();
			void				_HandleConnect();
			void				_HandleSend();
			void				_HandleSendAll();

			void				_SetTo(int fd, const BNetworkAddress& local,
									const BNetworkAddress& peer);

			status_t			_Connect(const BNetworkAddress& peer);

			status_t			_GetSocketError();

private:
			BEventDispatcher*	fDispatcher;
			EventCallback		fEventCallback;

			struct IORequest {
				void* buffer;
				size_t size;
				int flags;
				IOCallback* callback;
			};

			bool				fWaitingRecv : 1;
			bool				fWaitingSend : 1;
			bool				fWaitingSendAll : 1;
			bool				fWaitingConnect : 1;

			IORequest			fSendRequest;
			IORequest			fRecvRequest;

			StatusCallback*		fConnectCallback;
};


#endif	// _ASYNC_SOCKET_H
