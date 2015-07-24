/*
 * Copyright 2015, Hamish Morrison, hamishm53@gmail.com.
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include <HTTPConnectionManager.h>


static const size_t kInitialMapSize = 20;


HTTPConnectionManager::HTTPConnectionManager()
{
	fInitStatus = fConnectionMap.Init(kInitialMapSize);
}


HTTPConnectionManager::~HTTPConnectionManager()
{
}


status_t
HTTPConnectionManager::GetConnection(HTTPConnection*& _connection,
	const BNetworkAddress& host, BEventDispatcher* dispatcher,
	const Callback& callback)
{
	BString hostString = host.ToString();

	ConnectionMap::ValueIterator iter = fConnectionMap.Lookup(hostString);
	while (iter.HasNext()) {
		HTTPConnection* connection = iter.Next();
		iter.Remove();
		*_connection = connection;
		return B_OK;
	}

	HTTPConnection* connection = new(std::nothrow) HTTPConnection(dispatcher,
		hostString);
	if (connection == NULL)
		return B_NO_MEMORY;

	*_connection = connection;
	return connection->sock.ConnectAsync(host, callback);
}


status_t
HTTPConnectionManager::PutConnection(HTTPConnection* connection)
{
	// TODO: check connection is acceptable for reuse and set expiry time
	// on the connection.
	fConnectionMap.Insert(connection);
	return B_OK;
}

status_t
HTTPConnectionManager::ExpireConnections()
{
	// TODO
	return B_OK;
}
