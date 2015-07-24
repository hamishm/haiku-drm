/*
 * Copyright 2015, Hamish Morrison, hamishm53@gmail.com.
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#ifndef _HTTP_CONNECTION_MANAGER_H
#define _HTTP_CONNECTION_MANAGER_H


#include <AsyncSocket.h>


/*
 * A simple container class for an active HTTP socket connection.
 */
class HTTPConnection {
public:
	HTTPConnection(const BString& host)
		:
		fHost(host)
	{
	}

	BString& Host()
	{
		return fHost;
	}

	BAsyncSocket& Socket()
	{
		return fSocket;
	}

	// HashTableDefinition

	typedef BString			KeyType;
	typedef HTTPConnection	ValueType;

	size_t HashKey(BString host) const
	{
		return host.HashValue();
	}

	size_t Hash(HTTPConnection* connection) const
	{
		return HashKey(connection);
	}

	bool Compare(BString host, HTTPConnection* connection) const
	{
		return host == connection->fHost;
	}

	HTTPConnection*& GetLink(HTTPConnection* connection) const
	{
		return connection->fNext;
	}

private:
	BString			fHost;
	BAsyncSocket	fSocket;
	HTTPConnection*	fNext;
};


class HTTPConnectionManager {
public:
					HTTPConnectionManager();
	virtual			~HTTPConnectionManager();

	status_t		GetConnection(HTTPConnection*& _connection,
						const BNetworkAddress& host,
						BEventDispatcher* dispatcher,
						const CompletionCallback& callback);

	status_t		PutConnection(HTTPConnection* connection);

	status_t		ExpireConnections();

private:
	typedef MultiHashTable<HTTPConnection> ConnectionMap;

	ConnectionMap	fConnectionMap;
};


#endif // _HTTP_CONNECTION_MANAGER_H
