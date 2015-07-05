/*
 * Copyright 2010-2014 Haiku Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Christophe Huriaux, c.huriaux@gmail.com
 *		Niels Sascha Reedijk, niels.reedijk@gmail.com
 *		Adrien Destugues, pulkomandy@pulkomandy.tk
 */


#include <HttpRequest.h>

#include <arpa/inet.h>
#include <stdio.h>

#include <cstdlib>
#include <deque>
#include <new>

#include <AutoDeleter.h>
#include <Certificate.h>
#include <Debug.h>
#include <DynamicBuffer.h>
#include <File.h>
#include <Socket.h>
#include <SecureSocket.h>
#include <StackOrHeapArray.h>
#include <ZlibCompressionAlgorithm.h>


static const int32 kHttpBufferSize = 4096;


class CheckedSecureSocket: public BSecureSocket
{
public:
					CheckedSecureSocket(BHttpRequest* request);

	bool			CertificateVerificationFailed(BCertificate& certificate,
						const char* message);

private:
	BHttpRequest*	fRequest;
};


CheckedSecureSocket::CheckedSecureSocket(BHttpRequest* request)
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


BHttpRequest::BHttpRequest(const BUrl& url, bool ssl,
	BUrlProtocolListener* listener, BUrlContext* context)
	:
	BNetworkRequest(url, listener, context),
	fSSL(ssl),
	fRequestMethod(B_HTTP_GET),
	fHttpVersion(B_HTTP_11),
	fResult(url),
	fRequestStatus(kRequestInitialState),
	fOptHeaders(NULL),
	fOptPostFields(NULL),
	fOptInputData(NULL),
	fOptInputDataSize(-1),
	fOptRangeStart(-1),
	fOptRangeEnd(-1),
	fOptFollowLocation(true)
{
	_ResetOptions();
	fSocket = NULL;
}


BHttpRequest::BHttpRequest(const BHttpRequest& other)
	:
	BNetworkRequest(other.Url(), other.fListener, other.fContext,
		"BUrlProtocol.HTTP", other.fSSL ? "HTTPS" : "HTTP"),
	fSSL(other.fSSL),
	fRequestMethod(other.fRequestMethod),
	fHttpVersion(other.fHttpVersion),
	fResult(other.fUrl),
	fRequestStatus(kRequestInitialState),
	fOptHeaders(NULL),
	fOptPostFields(NULL),
	fOptInputData(NULL),
	fOptInputDataSize(-1),
	fOptRangeStart(other.fOptRangeStart),
	fOptRangeEnd(other.fOptRangeEnd),
	fOptFollowLocation(other.fOptFollowLocation)
{
	_ResetOptions();
		// FIXME some options may be copied from other instead.
	fSocket = NULL;
}


BHttpRequest::~BHttpRequest()
{
	Stop();

	delete fSocket;

	delete fOptInputData;
	delete fOptHeaders;
	delete fOptPostFields;
}


void
BHttpRequest::SetMethod(const char* const method)
{
	fRequestMethod = method;
}


void
BHttpRequest::SetFollowLocation(bool follow)
{
	fOptFollowLocation = follow;
}


void
BHttpRequest::SetMaxRedirections(int8 redirections)
{
	fOptMaxRedirs = redirections;
}


void
BHttpRequest::SetReferrer(const BString& referrer)
{
	fOptReferer = referrer;
}


void
BHttpRequest::SetUserAgent(const BString& agent)
{
	fOptUserAgent = agent;
}


void
BHttpRequest::SetDiscardData(bool discard)
{
	fOptDiscardData = discard;
}


void
BHttpRequest::SetDisableListener(bool disable)
{
	fOptDisableListener = disable;
}


void
BHttpRequest::SetAutoReferrer(bool enable)
{
	fOptAutoReferer = enable;
}


void
BHttpRequest::SetHeaders(const BHttpHeaders& headers)
{
	AdoptHeaders(new(std::nothrow) BHttpHeaders(headers));
}


void
BHttpRequest::AdoptHeaders(BHttpHeaders* const headers)
{
	delete fOptHeaders;
	fOptHeaders = headers;
}


void
BHttpRequest::SetPostFields(const BHttpForm& fields)
{
	AdoptPostFields(new(std::nothrow) BHttpForm(fields));
}


void
BHttpRequest::AdoptPostFields(BHttpForm* const fields)
{
	delete fOptPostFields;
	fOptPostFields = fields;

	if (fOptPostFields != NULL)
		fRequestMethod = B_HTTP_POST;
}


void
BHttpRequest::AdoptInputData(BDataIO* const data, const ssize_t size)
{
	delete fOptInputData;
	fOptInputData = data;
	fOptInputDataSize = size;
}


void
BHttpRequest::SetUserName(const BString& name)
{
	fOptUsername = name;
}


void
BHttpRequest::SetPassword(const BString& password)
{
	fOptPassword = password;
}


/*static*/ bool
BHttpRequest::IsInformationalStatusCode(int16 code)
{
	return (code >= B_HTTP_STATUS__INFORMATIONAL_BASE)
		&& (code <  B_HTTP_STATUS__INFORMATIONAL_END);
}


/*static*/ bool
BHttpRequest::IsSuccessStatusCode(int16 code)
{
	return (code >= B_HTTP_STATUS__SUCCESS_BASE)
		&& (code <  B_HTTP_STATUS__SUCCESS_END);
}


/*static*/ bool
BHttpRequest::IsRedirectionStatusCode(int16 code)
{
	return (code >= B_HTTP_STATUS__REDIRECTION_BASE)
		&& (code <  B_HTTP_STATUS__REDIRECTION_END);
}


/*static*/ bool
BHttpRequest::IsClientErrorStatusCode(int16 code)
{
	return (code >= B_HTTP_STATUS__CLIENT_ERROR_BASE)
		&& (code <  B_HTTP_STATUS__CLIENT_ERROR_END);
}


/*static*/ bool
BHttpRequest::IsServerErrorStatusCode(int16 code)
{
	return (code >= B_HTTP_STATUS__SERVER_ERROR_BASE)
		&& (code <  B_HTTP_STATUS__SERVER_ERROR_END);
}


/*static*/ int16
BHttpRequest::StatusCodeClass(int16 code)
{
	if (BHttpRequest::IsInformationalStatusCode(code))
		return B_HTTP_STATUS_CLASS_INFORMATIONAL;
	else if (BHttpRequest::IsSuccessStatusCode(code))
		return B_HTTP_STATUS_CLASS_SUCCESS;
	else if (BHttpRequest::IsRedirectionStatusCode(code))
		return B_HTTP_STATUS_CLASS_REDIRECTION;
	else if (BHttpRequest::IsClientErrorStatusCode(code))
		return B_HTTP_STATUS_CLASS_CLIENT_ERROR;
	else if (BHttpRequest::IsServerErrorStatusCode(code))
		return B_HTTP_STATUS_CLASS_SERVER_ERROR;

	return B_HTTP_STATUS_CLASS_INVALID;
}


const BUrlResult&
BHttpRequest::Result() const
{
	return fResult;
}


status_t
BHttpRequest::Stop()
{
	fSocket->CancelAsync();
	return BNetworkRequest::Stop();
}


void
BHttpRequest::_ResetOptions()
{
	delete fOptPostFields;
	delete fOptHeaders;

	fOptFollowLocation = true;
	fOptMaxRedirs = 8;
	fOptReferer = "";
	fOptUserAgent = "Services Kit (Haiku)";
	fOptUsername = "";
	fOptPassword = "";
	fOptAuthMethods = B_HTTP_AUTHENTICATION_BASIC | B_HTTP_AUTHENTICATION_DIGEST
		| B_HTTP_AUTHENTICATION_IE_DIGEST;
	fOptHeaders = NULL;
	fOptPostFields = NULL;
	fOptSetCookies = true;
	fOptDiscardData = false;
	fOptDisableListener = false;
	fOptAutoReferer = true;
}


status_t
BHttpRequest::_ProtocolLoop()
{
	// Initialize the request redirection loop
	int8 maxRedirs = fOptMaxRedirs;
	bool newRequest;

	do {
		newRequest = false;

		// Result reset
		fHeaders.Clear();
		_ResultHeaders().Clear();

		BString host = fUrl.Host();
		int port = fSSL ? 443 : 80;

		if (fUrl.HasPort())
			port = fUrl.Port();

		if (fContext->UseProxy()) {
			host = fContext->GetProxyHost();
			port = fContext->GetProxyPort();
		}

		status_t result = fInputBuffer.InitCheck();
		if (result != B_OK)
			return result;

		if (!_ResolveHostName(host, port)) {
			_EmitDebug(B_URL_PROTOCOL_DEBUG_ERROR,
				"Unable to resolve hostname (%s), aborting.",
					fUrl.Host().String());
			return B_SERVER_NOT_FOUND;
		}

		status_t requestStatus = _MakeRequest();
		if (requestStatus != B_OK)
			return requestStatus;

		// Prepare the referer for the next request if needed
		if (fOptAutoReferer)
			fOptReferer = fUrl.UrlString();

		switch (StatusCodeClass(fResult.StatusCode())) {
			case B_HTTP_STATUS_CLASS_INFORMATIONAL:
				// Header 100:continue should have been
				// handled in the _MakeRequest read loop
				break;

			case B_HTTP_STATUS_CLASS_SUCCESS:
				break;

			case B_HTTP_STATUS_CLASS_REDIRECTION:
			{
				// Redirection has been explicitly disabled
				if (!fOptFollowLocation)
					break;

				int code = fResult.StatusCode();
				if (code == B_HTTP_STATUS_MOVED_PERMANENTLY
					|| code == B_HTTP_STATUS_FOUND
					|| code == B_HTTP_STATUS_SEE_OTHER
					|| code == B_HTTP_STATUS_TEMPORARY_REDIRECT) {
					BString locationUrl = fHeaders["Location"];

					fUrl = BUrl(fUrl, locationUrl);

					// 302 and 303 redirections also convert POST requests to GET
					// (and remove the posted form data)
					if ((code == B_HTTP_STATUS_FOUND
						|| code == B_HTTP_STATUS_SEE_OTHER)
						&& fRequestMethod == B_HTTP_POST) {
						SetMethod(B_HTTP_GET);
						delete fOptPostFields;
						fOptPostFields = NULL;
						delete fOptInputData;
						fOptInputData = NULL;
						fOptInputDataSize = 0;
					}

					if (--maxRedirs > 0) {
						newRequest = true;

						// Redirections may need a switch from http to https.
						if (fUrl.Protocol() == "https")
							fSSL = true;
						else if (fUrl.Protocol() == "http")
							fSSL = false;

						_EmitDebug(B_URL_PROTOCOL_DEBUG_TEXT,
							"Following: %s\n",
							fUrl.UrlString().String());
					}
				}
				break;
			}

			case B_HTTP_STATUS_CLASS_CLIENT_ERROR:
				if (fResult.StatusCode() == B_HTTP_STATUS_UNAUTHORIZED) {
					BHttpAuthentication* authentication
						= &fContext->GetAuthentication(fUrl);
					status_t status = B_OK;

					if (authentication->Method() == B_HTTP_AUTHENTICATION_NONE) {
						// There is no authentication context for this
						// url yet, so let's create one.
						BHttpAuthentication newAuth;
						newAuth.Initialize(fHeaders["WWW-Authenticate"]);
						fContext->AddAuthentication(fUrl, newAuth);

						// Get the copy of the authentication we just added.
						// That copy is owned by the BUrlContext and won't be
						// deleted (unlike the temporary object above)
						authentication = &fContext->GetAuthentication(fUrl);
					}

					newRequest = false;
					if (fOptUsername.Length() > 0 && status == B_OK) {
						// If we received an username and password, add them
						// to the request. This will either change the
						// credentials for an existing request, or set them
						// for a new one we created just above.
						//
						// If this request handles HTTP redirections, it will
						// also automatically retry connecting and send the
						// login information.
						authentication->SetUserName(fOptUsername);
						authentication->SetPassword(fOptPassword);
						newRequest = true;
					}
				}
				break;

			case B_HTTP_STATUS_CLASS_SERVER_ERROR:
				break;

			default:
			case B_HTTP_STATUS_CLASS_INVALID:
				break;
		}
	} while (newRequest);

	_EmitDebug(B_URL_PROTOCOL_DEBUG_TEXT,
		"%ld headers and %ld bytes of data remaining",
		fHeaders.CountHeaders(), fInputBuffer.Size());

	if (fResult.StatusCode() == 404)
		return B_RESOURCE_NOT_FOUND;

	return B_OK;
}


status_t
BHttpRequest::_MakeRequest()
{
	delete fSocket;
	fSocket = new(std::nothrow) BAsyncSocket();

	if (fSocket == NULL)
		return B_NO_MEMORY;

	_EmitDebug(B_URL_PROTOCOL_DEBUG_TEXT, "Connection to %s on port %d.",
		fUrl.Authority().String(), fRemoteAddr.Port());

	fSocket->ConnectAsync(fRemoteAddr, std::bind(_OnConnect, this));
	return B_OK;
}


void
BHttpRequest::_OnConnect(status_t result)
{
	if (result != B_OK)
		_OnError(result);

	BString request = _RequestLine();

	fSocket->AsyncWriteAll(request.String(), request.Length(),
		std::bind(_SendHeaders, this));
}


void
BHttpRequest::_SendHeaders(ssize_t written)
{
	if (written <= 0)
		_OnError(result);

	// Write output headers to output stream
	BString headerData;

	for (int32 headerIndex = 0; headerIndex < outputHeaders.CountHeaders();
			headerIndex++) {
		const char* header = outputHeaders.HeaderAt(headerIndex).Header();

		headerData << header;
		headerData << "\r\n";

		_EmitDebug(B_URL_PROTOCOL_DEBUG_HEADER_OUT, "%s", header);
	}

	fSocket->AsyncWriteAll(headerData.String(), headerData.Length(),
		std::bind(_SendData, this));
}


void
BHttpRequest::_SendData(ssize_t written)
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
BHttpRequest::_SendPostFields()
{
	/* TODO:
	 * fOptInputDataSize = fOptPostFields.Length();
	 * fOptInputData = new BHTTPFormReader(fOptPostFields);
	 * ... use regular BDataIO code path ...
	 */
}


void
BHttpRequest::_SendInputData(ssize_t written)
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
BHttpRequest::_ReceiveResponse()
{
	http_parser_init(&fHttpParser, HTTP_RESPONSE);

	fSocket->ReadAsync(fResponseBuffer, fResponseBufferSize,
		std::bind(_OnRead, this));
}


void
BHttpRequest::_OnRead(ssize_t bytesRead)
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
BHttpRequest::_OnHeaderField(http_parser* parser, const char* data,
	size_t length)
{
	BHttpRequest* request = PARSER_TO_REQUEST(parser);

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
BHttpRequest::_OnHeaderValue(http_parser* parser, const char* data,
	size_t length)
{
	BHttpRequest* request = PARSER_TO_REQUEST(parser);

	request->fHeaderState = HEADER_VALUE;
	request->fHeaderValue.Append(data, length);
	return 0;
}


/* static */ int
BHttpRequest::_OnHeadersComplete(http_parser* parser)
{
	BHttpRequest* request = PARSER_TO_REQUEST(parser);

	if (request->fHeaderState == HEADER_VALUE) {
		bool result = fHeaders.Add(fHeaderField, fHeaderValue);
		if (!result)
			return B_NO_MEMORY;  // presumably
	}

	request->_HeadersComplete();
	return 0;
}


/* static */ int
BHttpRequest::_OnBody(http_parser* parser, const char* data,
	size_t length)
{
	BHttpRequest* request = PARSER_TO_REQUEST(parser);

	return request->_HandleData(data, length);
}


BString
BHttpRequest::_RequestLine()
{
	BString request(fRequestMethod);
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

	switch (fHttpVersion) {
		case B_HTTP_11:
			request << " HTTP/1.1\r\n";
			break;

		default:
		case B_HTTP_10:
			request << " HTTP/1.0\r\n";
			break;
	}

	return request;
}

void
BHttpRequest::_SendHeaders()
{
	BHttpHeaders outputHeaders;

	// HTTP 1.1 additional headers
	if (fHttpVersion == B_HTTP_11) {
		BString host = Url().Host();
		if (Url().HasPort() && !_IsDefaultPort())
			host << ':' << Url().Port();

		outputHeaders.AddHeader("Host", host);

		outputHeaders.AddHeader("Accept", "*/*");
		outputHeaders.AddHeader("Accept-Encoding", "gzip");
			// Allows the server to compress data using the "gzip" format.
			// "deflate" is not supported, because there are two interpretations
			// of what it means (the RFC and Microsoft products), and we don't
			// want to handle this. Very few websites support only deflate,
			// and most of them will send gzip, or at worst, uncompressed data.

		outputHeaders.AddHeader("Connection", "close");
			// Let the remote server close the connection after response since
			// we don't handle multiple request on a single connection
	}

	// Classic HTTP headers
	if (fOptUserAgent.CountChars() > 0)
		outputHeaders.AddHeader("User-Agent", fOptUserAgent.String());

	if (fOptReferer.CountChars() > 0)
		outputHeaders.AddHeader("Referer", fOptReferer.String());

	// Authentication
	if (fContext != NULL) {
		BHttpAuthentication& authentication = fContext->GetAuthentication(fUrl);
		if (authentication.Method() != B_HTTP_AUTHENTICATION_NONE) {
			if (fOptUsername.Length() > 0) {
				authentication.SetUserName(fOptUsername);
				authentication.SetPassword(fOptPassword);
			}

			BString request(fRequestMethod);
			outputHeaders.AddHeader("Authorization",
				authentication.Authorization(fUrl, request));
		}
	}

	// Required headers for POST data
	if (fOptPostFields != NULL && fRequestMethod == B_HTTP_POST) {
		BString contentType;

		switch (fOptPostFields->GetFormType()) {
			case B_HTTP_FORM_MULTIPART:
				contentType << "multipart/form-data; boundary="
					<< fOptPostFields->GetMultipartBoundary() << "";
				break;

			case B_HTTP_FORM_URL_ENCODED:
				contentType << "application/x-www-form-urlencoded";
				break;
		}

		outputHeaders.AddHeader("Content-Type", contentType);
		outputHeaders.AddHeader("Content-Length",
			fOptPostFields->ContentLength());
	} else if (fOptInputData != NULL
			&& (fRequestMethod == B_HTTP_POST
			|| fRequestMethod == B_HTTP_PUT)) {
		if (fOptInputDataSize >= 0)
			outputHeaders.AddHeader("Content-Length", fOptInputDataSize);
		else
			outputHeaders.AddHeader("Transfer-Encoding", "chunked");
	}

	// Optional headers specified by the user
	if (fOptHeaders != NULL) {
		for (int32 headerIndex = 0; headerIndex < fOptHeaders->CountHeaders();
				headerIndex++) {
			BHttpHeader& optHeader = (*fOptHeaders)[headerIndex];
			int32 replaceIndex = outputHeaders.HasHeader(optHeader.Name());

			// Add or replace the current option header to the
			// output header list
			if (replaceIndex == -1)
				outputHeaders.AddHeader(optHeader.Name(), optHeader.Value());
			else
				outputHeaders[replaceIndex].SetValue(optHeader.Value());
		}
	}

	// Context cookies
	if (fOptSetCookies && fContext != NULL) {
		BString cookieString;

		BNetworkCookieJar::UrlIterator iterator
			= fContext->GetCookieJar().GetUrlIterator(fUrl);
		const BNetworkCookie* cookie = iterator.Next();
		if (cookie != NULL) {
			while (true) {
				cookieString << cookie->RawCookie(false);
				cookie = iterator.Next();
				if (cookie == NULL)
					break;
				cookieString << "; ";
			}

			outputHeaders.AddHeader("Cookie", cookieString);
		}
	}

	// Write output headers to output stream
	BString headerData;

	for (int32 headerIndex = 0; headerIndex < outputHeaders.CountHeaders();
			headerIndex++) {
		const char* header = outputHeaders.HeaderAt(headerIndex).Header();

		headerData << header;
		headerData << "\r\n";

		_EmitDebug(B_URL_PROTOCOL_DEBUG_HEADER_OUT, "%s", header);
	}

	fSocket->Write(headerData.String(), headerData.Length());
}


void
BHttpRequest::_SendPostData()
{
	if (fRequestMethod == B_HTTP_POST && fOptPostFields != NULL) {
		if (fOptPostFields->GetFormType() != B_HTTP_FORM_MULTIPART) {
			BString outputBuffer = fOptPostFields->RawData();
			_EmitDebug(B_URL_PROTOCOL_DEBUG_TRANSFER_OUT,
				"%s", outputBuffer.String());
			fSocket->Write(outputBuffer.String(), outputBuffer.Length());
		} else {
			for (BHttpForm::Iterator it = fOptPostFields->GetIterator();
				const BHttpFormData* currentField = it.Next();
				) {
				_EmitDebug(B_URL_PROTOCOL_DEBUG_TRANSFER_OUT,
					it.MultipartHeader().String());
				fSocket->Write(it.MultipartHeader().String(),
					it.MultipartHeader().Length());

				switch (currentField->Type()) {
					default:
					case B_HTTPFORM_UNKNOWN:
						ASSERT(0);
						break;

					case B_HTTPFORM_STRING:
						fSocket->Write(currentField->String().String(),
							currentField->String().Length());
						break;

					case B_HTTPFORM_FILE:
						{
							BFile upFile(currentField->File().Path(),
								B_READ_ONLY);
							char readBuffer[kHttpBufferSize];
							ssize_t readSize;

							readSize = upFile.Read(readBuffer,
								sizeof(readBuffer));
							while (readSize > 0) {
								fSocket->Write(readBuffer, readSize);
								readSize = upFile.Read(readBuffer,
									sizeof(readBuffer));
							}

							break;
						}
					case B_HTTPFORM_BUFFER:
						fSocket->Write(currentField->Buffer(),
							currentField->BufferSize());
						break;
				}

				fSocket->Write("\r\n", 2);
			}

			BString footer = fOptPostFields->GetMultipartFooter();
			fSocket->Write(footer.String(), footer.Length());
		}
	} else if ((fRequestMethod == B_HTTP_POST || fRequestMethod == B_HTTP_PUT)
		&& fOptInputData != NULL) {

		// If the input data is seekable, we rewind it for each new request.
		BPositionIO* seekableData
			= dynamic_cast<BPositionIO*>(fOptInputData);
		if (seekableData)
			seekableData->Seek(0, SEEK_SET);

		for (;;) {
			char outputTempBuffer[kHttpBufferSize];
			ssize_t read = fOptInputData->Read(outputTempBuffer,
				sizeof(outputTempBuffer));

			if (read <= 0)
				break;

			if (fOptInputDataSize < 0) {
				// Chunked transfer
				char hexSize[16];
				size_t hexLength = sprintf(hexSize, "%ld", read);

				fSocket->Write(hexSize, hexLength);
				fSocket->Write("\r\n", 2);
				fSocket->Write(outputTempBuffer, read);
				fSocket->Write("\r\n", 2);
			} else {
				fSocket->Write(outputTempBuffer, read);
			}
		}

		if (fOptInputDataSize < 0) {
			// Chunked transfer terminating sequence
			fSocket->Write("0\r\n\r\n", 5);
		}
	}

}


BHttpHeaders&
BHttpRequest::_ResultHeaders()
{
	return fResult.fHeaders;
}


void
BHttpRequest::_SetResultStatusCode(int32 statusCode)
{
	fResult.fStatusCode = statusCode;
}


BString&
BHttpRequest::_ResultStatusText()
{
	return fResult.fStatusString;
}


bool
BHttpRequest::_CertificateVerificationFailed(BCertificate& certificate,
	const char* message)
{
	if (fListener != NULL) {
		return fListener->CertificateVerificationFailed(this, certificate,
			message);
	}

	return false;
}


bool
BHttpRequest::_IsDefaultPort()
{
	if (fSSL && Url().Port() == 443)
		return true;
	if (!fSSL && Url().Port() == 80)
		return true;
	return false;
}


