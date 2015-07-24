/*
 * Copyright 2015, Hamish Morrison, hamishm53@gmail.com.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef _HTTP_SESSION_H
#define _HTTP_SESSION_H

#include <EventDispatcher.h>
#include <HttpHeaders.h>
#include <HttpResult.h>
#include <Url.h>

#include <http_parser.h>


class HTTPConnection;


class HTTPParser {
public:
							HTTPParser(NBHTTPRequest& request, void* buffer, size_t size);
	virtual					~HTTPParser();

			status_t		SendRequest(const BUrl& url, BString method,
								const BHttpHeaders& headers, BDataIO* body);

			status_t		ReceiveHeaders();
			status_t		ReceiveBody();

			bool			IsReusable();

			status_t		RequestStatus();

			int				StatusCode();
			BString&		StatusString();

private:
			void			_IOCompleted(ssize_t result);
			ssize_t			_NextState(ssize_t result);

			ssize_t			_SendHeaders();
			ssize_t			_SendRequestData();
			ssize_t			_SendInputData();

			ssize_t			_BeginReceive();
			ssize_t			_ReceiveResponse(ssize_t bytesRead);

			BString			_RequestLine();

	// HTTP parser callbacks
	static	int				_OnStatus(http_parser* parser, const char* data,
								size_t length);
	static	int				_OnHeaderField(http_parser* parser, const char* data,
								size_t length);
	static	int				_OnHeaderValue(http_parser* parser, const char* data,
								size_t length);
	static	int				_OnHeadersComplete(http_parser* parser);
	static	int				_OnBody(http_parser* parser, const char* data,
								size_t length);
	static	int				_OnMessageComplete(http_parser* parser);

private:
			IOCallback		fIOCallback;

			HTTPConnection*	fConnection;

			status_t		fError;

			NBHTTPRequest*	fRequest;

			size_t			fSizeStringLength;

			enum {
				INITIAL,
				SENDING_HEADERS,
				DONE_SENDING_HEADERS,
				SENDING_BODY,
				RECEIVING_HEADERS,
				RECEIVING_BODY,
				REUSABLE,
				CLOSED
			} fNextState;
		
			// Header parsing
			enum {
				HEADER_FIELD,
				HEADER_VALUE
			} fHeaderState;

			int				fStatusCode;
			BString			fStatusString;
			BString			fHeaderField;
			BString			fHeaderValue;

			char*			fBuffer;
			size_t			fBufferSize;

			BDataIO*		fRequestBody;

			http_parser		fHTTPParser;
			static http_parser_settings fHTTPParserSettings;
};


inline status_t
HTTPNetworkStream::RequestStatus()
{
	return fError;
}


inline bool
HTTPNetworkStream::IsReusable()
{
	return fNextState == REUSABLE;
}


inline int
HTTPNetworkStream::StatusCode()
{
	return fStatusCode;
}


inline BString&
HTTPNetworkStream::StatusString()
{
	return fStatusString;
}


#endif	// _HTTP_SESSION_H
