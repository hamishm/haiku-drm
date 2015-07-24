/*
 * Copyright 2015, Hamish Morrison, hamishm53@gmail.com.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#include <HTTPNetworkStream.h>

#include <HTTPConnectionManager.h>
#include <HttpRequest.h>

#include <stdio.h>

#include <http_parser.h>


#define PARSER_TO_SESSION(parser)	\
	((intptr_t)(parser) - offsetof(HTTPNetworkStream, fHTTPParser))


namespace ph = std::placeholders;


http_parser_settings HTTPNetworkStream::fHTTPParserSettings = {
	NULL,
	NULL,
	HTTPNetworkStream::_OnStatus,
	HTTPNetworkStream::_OnHeaderField,
	HTTPNetworkStream::_OnHeaderValue,
	HTTPNetworkStream::_OnHeadersComplete,
	HTTPNetworkStream::_OnBody,
	HTTPNetworkStream::_OnMessageComplete,
	NULL,
	NULL
};


HTTPNetworkStream::HTTPNetworkStream(HTTPConnection* connection,
	HTTPRequest* request)
	:
	fIOCallback(std::bind(&HTTPNetworkStream::_IOCompleted, this, ph::_1)),
	fConnection(connection),
	fError(B_OK),
	fRequest(request),
	fNextState(INITIAL)
{
	static_assert((kHTTPBufferSize & (kHTTPBufferSize - 1)) == 0,
		"Buffer size must be power of 2");
	fSizeStringLength = ffs(kHTTPBufferSize) / 4;
}


status_t
HTTPNetworkStream::SendRequest(const BUrl& url, BString method,
	BHttpHeaders* headers, BDataIO* body)
{
	if (fNextState != INITIAL && fNextState != REUSABLE)
		return B_BAD_VALUE;

	fURL = url;
	fRequestMethod = method;
	fRequestHeaders = headers;
	fRequestBody = body;

	fNextState = SENDING_HEADERS;
	fHeaderState = HEADER_FIELD;

	ssize_t result = _NextState(B_OK);
	return result == EINPROGRESS ? B_OK : result;
}


void
HTTPNetworkStream::_IOCompleted(ssize_t result)
{
	result = _NextState(result);
	if (result != B_OK && result != EINPROGRESS) {
		fRequest->ErrorOccurred(result);
		fError = result;
	}
}


ssize_t
HTTPNetworkStream::_NextState(ssize_t result)
{
	while (fNextState != CLOSED && fNextState != REUSABLE) {
		if (result != B_OK)
			return result;

		switch (fNextState) {
			case SENDING_HEADERS:
				result = _SendHeaders();
				break;
			case DONE_SENDING_HEADERS:
				result = _SendRequestData();
				break;
			case SENDING_BODY:
				result = _SendInputData();
				break;
			case RECEIVING_HEADERS:
			case RECEIVING_BODY:
				result = _ReceiveResponse(result);
				break;
			default:
				debugger("Invalid state");
				break;
		}
	}
}


ssize_t
HTTPNetworkStream::_SendHeaders()
{
	BString request = _RequestLine();

	// Write output headers to output stream
	for (int32 headerIndex = 0; headerIndex < fRequestHeaders->CountHeaders();
			headerIndex++) {
		const char* header = fRequestHeaders->HeaderAt(headerIndex).Header();
		request << header << "\r\n";
	}

	// TODO: if the request body is small we should also append it
	// here and send it all in one go.

	fNextState = DONE_SENDING_HEADERS;
	return fConnection->Socket().SendAllAsync(request.String(),
		request.Length(), 0, fIOCallback);
}


ssize_t
HTTPNetworkStream::_SendRequestData()
{
	if (fOptInputData != NULL)
		return _SendInputData();
	else {
		fNextState = RECEIVING_HEADERS;
		return _BeginReceive();
	}
}


ssize_t
HTTPNetworkStream::_SendInputData()
{
	size_t toRead = fResponseBufferSize;
	char* buffer = fBuffer;

	if (fSendChunked) {
		// If we're sending chunked we need to reserve space in our buffer for
		// the size and the \r\n separators.
		buffer += fSizeStringLength + 2;
		toRead -= fSizeStringLength + 4;
	}

	ssize_t numRead = fRequestBody.Read(buffer, toRead);
	if (numRead < 0)
		return numRead;

	if (numRead == 0) {
		fNextState = RECEIVING_HEADERS;
		return _BeginReceive();
	}

	size_t toSend = numRead;

	if (fSendChunked) {
		// Write our size string.
		sprintf(fBuffer, "%*zx\r\n", fSizeStringLength, numRead);

		// Write the \r\n separator at the end.
		buffer[numRead] = '\r';
		buffer[numRead + 1] == '\n';

		toSend += fSizeStringLength + 4;
		// Skip any padding at the start of the buffer
		for (buffer = fBuffer; buffer == ' '; buffer++)
			toSend--;
	}

	return fConnection->Socket().SendAllAsync(buffer, toSend, 0, fIOCallback);
}


ssize_t
HTTPNetworkStream::_BeginReceive()
{
	http_parser_init(&fHTTPParser, HTTP_RESPONSE);
	return fConnection->Socket().RecvAsync(fBuffer, kHTTPBufferSize, 0,
		fIOCallback);
}


ssize_t
HTTPNetworkStream::_ReceiveResponse(ssize_t bytesRead)
{
	size_t parsed = http_parser_execute(&fHTTPParser, &fHttpSettings,
		fResponseBuffer, bytesRead);
	if (parsed != bytesRead) {
		_OnHTTPParserError(parser->http_errno);
		return B_ERROR;
	}

	if (fNextState == REUSABLE || fNextState == CLOSED)
		return B_OK;

	return fSocket->RecvAsync(fResponse, kHTTPBufferSize, 0, fIOCallback);
}


/* static */ int
HTTPNetworkStream::_OnStatus(http_parser* parser, const char* data,
	size_t length)
{
	HTTPNetworkStream* session = PARSER_TO_SESSION(parser);
	session->fStatusString.Append(data, length);
	return 0;
}


/* static */ int
HTTPNetworkStream::_OnHeaderField(http_parser* parser, const char* data,
	size_t length)
{
	HTTPNetworkStream* session = PARSER_TO_SESSION(parser);

	if (session->fHeaderState == HEADER_VALUE) {
		// Copy the previous header into the result headers
		bool result = session->fResponse.Headers().Add(session->fHeaderField,
			session->fHeaderValue);

		session->fHeaderField.Truncate(0);
		session->fHeaderValue.Truncate(0);
		if (!result)
			return B_NO_MEMORY;
	}

	session->fHeaderState = HEADER_FIELD;
	session->fHeaderField.Append(data, length);
	return 0;
}


/* static */ int
HTTPNetworkStream::_OnHeaderValue(http_parser* parser, const char* data,
	size_t length)
{
	HTTPNetworkStream* session = PARSER_TO_SESSION(parser);

	session->fHeaderState = HEADER_VALUE;
	session->fHeaderValue.Append(data, length);
	return 0;
}


/* static */ int
HTTPNetworkStream::_OnHeadersComplete(http_parser* parser)
{
	HTTPNetworkStream* session = PARSER_TO_SESSION(parser);

	if (request->fHeaderState == HEADER_VALUE) {
		bool result = request->fRequestHeaders.Add(request->fHeaderField,
			request->fHeaderValue);

		session->fHeaderField.Truncate(0);
		session->fHeaderValue.Truncate(0);
		if (!result)
			return B_NO_MEMORY;
	}

	session->fStatusCode = parser->status_code;
	session->fNextState = RECEIVING_BODY;
	return session->fRequest->_HeadersComplete();
}


/* static */ int
HTTPNetworkStream::_OnBody(http_parser* parser, const char* data,
	size_t length)
{
	HTTPNetworkStream* session = PARSER_TO_SESSION(parser);
	return session->fRequest->_HandleData(data, length);
}


/* static */ int
HTTPNetworkStream::_OnMessageComplete(http_parser* parser)
{
	HTTPNetworkStream* session = PARSER_TO_SESSION(parser);

	bool keepAlive = http_should_keep_alive(parser) != 0;
	session->fNextState = keepAlive ? REUSABLE : CLOSED;

	return session->fRequest->_RequestComplete();
}


BString
HTTPNetworkStream::_RequestLine()
{
	BString request(kRequestMethodStrings[fRequestMethod]);
	request << ' ';

	if (fConnection->IsProxy()) {
		// When there is a proxy, the request must include the host and port so
		// the proxy knows where to send the request.
		request << fURL.Protocol() << "://" << fURL.Host();
		if (fURL.HasPort())
			request << ':' << fURL.Port();
	}

	if (fURL.HasPath())
		request << fURL.Path();
	else
		request << '/';

	if (fURL.HasRequest())
		request << '?' << fURL.Request();

	switch (fHttpVersion) {
		case B_HTTP_11:
			request << " HTTP/1.1\r\n";
			break;

		case B_HTTP_10:
		default:
			request << " HTTP/1.0\r\n";
			break;
	}

	return request;
}
