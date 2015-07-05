/*
 * Copyright 2015, Hamish Morrison, hamishm53@gmail.com.
 * All rights reserved. Distributed under the terms of the MIT License.
 */

#include <HttpRequest.h>

#include <arpa/inet.h>
#include <stdio.h>

#include <cstdlib>
#include <deque>
#include <new>

#include <Certificate.h>
#include <Debug.h>
#include <DynamicBuffer.h>
#include <File.h>
#include <Socket.h>
#include <SecureSocket.h>
#include <StackOrHeapArray.h>
#include <ZlibCompressionAlgorithm.h>


static const int32 kHttpBufferSize = 4096;


class CheckedSecureSocket : public BSecureSocket
{
public:
					CheckedSecureSocket(BHTTPSession* session);

	bool			CertificateVerificationFailed(BCertificate& certificate,
						const char* message);

private:
	BHTTPSession*	fRequest;
};


CheckedSecureSocket::CheckedSecureSocket(BHTTPSession* request)
	:
	BSecureSocket(),
	fRequest(request)
{
}


bool
CheckedSecureSocket::CertificateVerificationFailed(BCertificate& certificate,
	const char* message)
{
	return fRequest->_CertificateVerificationFailed(certificate, message);
}


BHTTPSession::BHTTPSession()
	:
	fSocket(NULL),
	fRequestStatus(kRequestInitialState),
	fCanBeReused(false)
{
}


status_t
BHTTPSession::MakeRequest(BUrl* url, HttpMethod method, HttpHeaders* headers,
	BDataIO* body)
{
	if (RequestInProgress())
		return EINPROGRESS;  // TODO

	if (fSocket == NULL) {
		// TODO: socket should be retrieved from some kind of socket pool
		fSocket = new(std::nothrow) BAsyncSocket();
		if (fSocket == NULL)
			return B_NO_MEMORY;
	}

	fURL = url;
	fRequestMethod = method;
	fRequestHeaders = headers;
	fRequestBody = body;

	fSocket->ConnectAsync(fRemoteAddr, std::bind(_OnConnect, this));
	return B_OK;
}


void
BHTTPSession::_OnConnect(status_t result)
{
	if (result != B_OK)
		_OnError(result);

	BString request = _RequestLine();

	fSocket->AsyncWriteAll(request.String(), request.Length(),
		std::bind(_SendHeaders, this));
}


void
BHTTPSession::_SendHeaders(ssize_t written)
{
	if (written <= 0)
		_OnError(result);

	// Write output headers to output stream
	BString headerData;

	for (int32 headerIndex = 0; headerIndex < fHeaders->CountHeaders();
			headerIndex++) {
		const char* header = fHeaders->HeaderAt(headerIndex).Header();
		headerData << header << "\r\n";
	}

	fSocket->AsyncWriteAll(headerData.String(), headerData.Length(),
		std::bind(_SendData, this));
}


void
BHTTPSession::_SendData(ssize_t written)
{
	if (written <= 0)
		_OnError(result);

	if (fRequestMethod == B_HTTP_POST && fOptPostFields != NULL)
		_SendPostFields();
	else if (fOptInputData != NULL) {
		_SendInputData(1);
	else
		_ReceiveRequest();
}


void
BHTTPSession::_SendPostFields()
{
	/* TODO:
	 * fOptInputDataSize = fOptPostFields.Length();
	 * fOptInputData = new BHTTPFormReader(fOptPostFields);
	 * ... use regular BDataIO code path ...
	 */
}


void
BHTTPSession::_SendInputData(ssize_t written)
{
	if (written <= 0) {
		OnError();
		return;
	}

	size_t toRead = fResponseBufferSize;
	char* buffer = fResponseBuffer;

	if (fSendChunked) {
		// If we're sending chunked we need to reserve space in our buffer for
		// the size and the \r\n separators.
		buffer += fSizeStringLength + 2;
		toRead -= fSizeStringLength + 4;
	}

	ssize_t numRead = fOptInputData.Read(buffer, toRead);
	if (numRead < 0) {
		OnError(numRead);
		return;
	}

	if (numRead == 0) {
		_ReceiveRequest();
		return;
	}

	size_t toSend = numRead;

	if (fSendChunked) {
		// Write our size string.
		sprintf("%*zx\r\n", fSizeStringLength, numRead);

		// Write the \r\n separator at the end.
		buffer[numRead] = '\r';
		buffer[numRead + 1] == '\n';

		toSend += fSizeStringLength + 4;
		// Skip any padding at the start of the buffer
		for (buffer = fResponseBuffer; buffer == ' '; buffer++)
			toSend--;
	}

	fSocket->WriteAllAsync(buffer, toSend, std::bind(_SendInputData, this));
}


void
BHTTPSession::_ReceiveResponse()
{
	http_parser_init(&fHttpParser, HTTP_RESPONSE);

	fSocket->ReadAsync(fResponseBuffer, fResponseBufferSize,
		std::bind(_OnRead, this));
}


void
BHTTPSession::_OnRead(ssize_t bytesRead)
{
	if (bytesRead < 0) {
		_OnError(result);
		return;
	}

	size_t parsed = http_parser_execute(&fHttpParser, &fHttpSettings,
		fResponseBuffer, bytesRead);
	if (parsed != bytesRead) {
		_OnHttpParseError(parser->http_errno);
		return;
	}

	if (bytesRead == 0) {
		return;
	}

	fSocket->ReadAsync(fResponseBuffer, fResponseBufferSize,
		std::bind(_OnRead, this));
}


/* static */ int
BHTTPSession::_OnHeaderField(http_parser* parser, const char* data,
	size_t length)
{
	BHTTPSession* request = PARSER_TO_REQUEST(parser);

	if (request->fHeaderState == HEADER_VALUE) {
		// Copy the previous header into the result headers
		bool result = fHeaders.Add(fHeaderField, fHeaderValue);
		if (!result)
			return B_NO_MEMORY;  // presumably
	}

	request->fHeaderState = HEADER_FIELD;
	request->fHeaderField.Append(data, length);
	return 0;
}


/* static */ int
BHTTPSession::_OnHeaderValue(http_parser* parser, const char* data,
	size_t length)
{
	BHTTPSession* request = PARSER_TO_REQUEST(parser);

	request->fHeaderState = HEADER_VALUE;
	request->fHeaderValue.Append(data, length);
	return 0;
}


/* static */ int
BHTTPSession::_OnHeadersComplete(http_parser* parser)
{
	BHTTPSession* request = PARSER_TO_REQUEST(parser);

	if (request->fHeaderState == HEADER_VALUE) {
		bool result = fHeaders.Add(fHeaderField, fHeaderValue);
		if (!result)
			return B_NO_MEMORY;  // presumably
	}

	request->_HeadersComplete();
	return 0;
}


/* static */ int
BHTTPSession::_OnBody(http_parser* parser, const char* data,
	size_t length)
{
	BHTTPSession* request = PARSER_TO_REQUEST(parser);

	return request->_HandleData(data, length);
}


BString
BHTTPSession::_RequestLine()
{
	BString request(kRequestMethodStrings[fRequestMethod]);
	request << ' ';

	if (fContext->UseProxy()) {
		// When there is a proxy, the request must include the host and port so
		// the proxy knows where to send the request.
		request << Url().Protocol() << "://" << Url().Host();
		if (Url().HasPort())
			request << ':' << Url().Port();
	}

	if (Url().HasPath())
		request << Url().Path();
	else
		request << '/';

	if (Url().HasRequest())
		request << '?' << Url().Request();

	// Not bothering with HTTP 1.0 for the moment.
	request << " HTTP/1.1\r\n";
	return request;
}


BHttpHeaders&
BHTTPSession::_ResultHeaders()
{
	return fResult.fHeaders;
}


void
BHTTPSession::_SetResultStatusCode(int32 statusCode)
{
	fResult.fStatusCode = statusCode;
}


BString&
BHTTPSession::_ResultStatusText()
{
	return fResult.fStatusString;
}


bool
BHTTPSession::_CertificateVerificationFailed(BCertificate& certificate,
	const char* message)
{
	if (fListener != NULL) {
		return fListener->CertificateVerificationFailed(this, certificate,
			message);
	}

	return false;
}


bool
BHTTPSession::_IsDefaultPort()
{
	if (fSSL && Url().Port() == 443)
		return true;
	if (!fSSL && Url().Port() == 80)
		return true;
	return false;
}


