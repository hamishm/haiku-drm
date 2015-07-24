/*
 * Copyright 2015, Hamish Morrison, hamishm53@gmail.com.
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
					CheckedSecureSocket(BHTTPRequest* request);

	bool			CertificateVerificationFailed(BCertificate& certificate,
						const char* message);

private:
	BHTTPRequest*	fRequest;
};


CheckedSecureSocket::CheckedSecureSocket(BHTTPRequest* request)
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


BHTTPRequest::BHTTPRequest(const BUrl& url, BUrlProtocolListener* listener,
	BUrlContext* context)
	:
	BURLRequest(url, listener, context),
	fRequestMethod(B_HTTP_GET),
	fHTTPVersion(B_HTTP_11),
	fResult(url),
	fRequestStatus(INITIAL),
	fOptPostFields(NULL),
	fOptInputData(NULL),
	fOptInputDataSize(-1),
	fOptRangeStart(-1),
	fOptRangeEnd(-1),
	fOptFollowLocation(true)
{
	_ResetOptions();
}


BHTTPRequest::~BHTTPRequest()
{
	Stop();

	delete fOptInputData;
	delete fOptPostFields;
}


void
BHTTPRequest::AdoptPostFields(BHttpForm* const fields)
{
	delete fOptPostFields;
	fOptPostFields = fields;

	if (fOptPostFields != NULL)
		fRequestMethod = B_HTTP_POST;
}


void
BHTTPRequest::AdoptInputData(BDataIO* const data, const int64 size)
{
	delete fOptInputData;
	fOptInputData = data;
	fOptInputDataSize = size;
}


const BUrlResult&
BHTTPRequest::Result() const
{
	return fResult;
}


status_t
BHTTPRequest::Stop()
{
	if (fHTTPStream != NULL)
		fHTTPStream->Cancel();

	fState = COMPLETE;
	return B_OK;
}


void
BHTTPRequest::_ResetOptions()
{
	delete fOptPostFields;
	delete fOptHeaders;

	fOptFollowLocation = true;
	fOptMaxRedirects = 8;
	fOptReferer = "";
	fOptUserAgent = "Services Kit (Haiku)";
	fOptUsername = "";
	fOptPassword = "";
	fOptAuthMethods = B_HTTP_AUTHENTICATION_BASIC | B_HTTP_AUTHENTICATION_DIGEST
		| B_HTTP_AUTHENTICATION_IE_DIGEST;
	fOptHeaders = NULL;
	fOptPostFields = NULL;
	fOptSetCookies = true;
	fOptAutoReferer = true;
}


void
BHTTPRequest::_OnComplete(status_t result)
{
	result = _NextState(result);

	if (result != B_OK && result != EINPROGRESS) {
		fRequestStatus = result;
		fListener->RequestFailed(result);
	}
}


status_t
BHTTPRequest::_NextState(status_t result)
{
	while (fState != COMPLETE) {
		if (result != B_OK)
			return result;

		switch (fState) {
			case CREATING_STREAM:
				fState = SENDING_REQUEST;
				result = fHTTPStream->SendRequest(fURL, fMethod,
					&fRequestHeaders, fRequestBody);
				break;
			case RESPONSE_RECEIVED:
				result = _NextRequest();
				break;
			default:
				debugger("Invalid state");
				break;
		}
	}
}


status_t
BHTTPRequest::_NextRequest()
{
	int statusCode = fResult.StatusCode();

	switch (StatusCodeClass(statusCode)) {
		case B_HTTP_STATUS_CLASS_INFORMATIONAL:
			debugger("Should be handled by HTTPStream");

		case B_HTTP_STATUS_CLASS_REDIRECTION:
			if (!fOptFollowLocation)
				return _CompleteRequest();

			return _HandleRedirection(statusCode);

		case B_HTTP_STATUS_CLASS_CLIENT_ERROR:
			if (statusCode != B_HTTP_STATUS_UNAUTHORIZED)
				return _CompleteRequest;

			return _HandleUnauthorized();

		case B_HTTP_STATUS_CLASS_SUCCESS:
		case B_HTTP_STATUS_CLASS_SERVER_ERROR:
		case B_HTTP_STATUS_CLASS_INVALID:
		default:
			return _CompleteRequest();
			fState = COMPLETE;
			return B_OK;
	}	
}


status_t
BHTTPRequest::_HandleRedirection()
{
	if (code != B_HTTP_STATUS_MOVED_PERMANENTLY
			&& code != B_HTTP_STATUS_FOUND
			&& code != B_HTTP_STATUS_SEE_OTHER
			&& code != B_HTTP_STATUS_TEMPORARY_REDIRECT)
		return B_OK;

	if (fRedirectCount++ == fMaxRedirects) {
		// TODO: report max redirections error
		_CompleteRequest();
	}
	
	BString locationURL = fResult.fHeaders["Location"];

	fURL = BUrl(fURL, locationURL);

	// 302 and 303 redirections also convert POST requests to GET (and remove
	// any request body).
	if (code == B_HTTP_STATUS_FOUND	|| code == B_HTTP_STATUS_SEE_OTHER) {
		SetMethod(B_HTTP_GET);
		AdoptPostFields(NULL);
		AdoptInputData(NULL, 0);
	}

	return _MakeRequest();
}


status_t
BHTTPRequest::_HandleUnauthorized()
{
	BHttpAuthentication* authentication	= &fContext->GetAuthentication(fUrl);

	if (authentication->Method() == B_HTTP_AUTHENTICATION_NONE) {
		if (fOptUsername.Length() == 0) {
			// No username was provided and there is no saved authentication.
			return _CompleteRequest();
		}
	
		// There is no authentication context for this URL yet, so let's create
		// one.
		BHttpAuthentication newAuthentication;
		newAuthentication.Initialize(fHeaders["WWW-Authenticate"]);
		fContext->AddAuthentication(fURL, newAuthentication);

		// Get the copy of the authentication we just added. This copy is owned
		// by the BUrlContext and won't be deleted (unlike the temporary object
		// above)
		authentication = &fContext->GetAuthentication(fURL);
	}

	if (fOptUsername.Length() > 0) {
		// If we received an username and password, add them to the request.
		// This will either change the credentials for an existing request, or
		// set them for a new one we created just above.
		//
		// If this request handles HTTP redirections, it will also
		// automatically retry connecting and send the login information.
		authentication->SetUserName(fOptUsername);
		authentication->SetPassword(fOptPassword);
	}

	// TODO: we should probably just reuse the existing HTTPStream -- it must
	// be a network stream if we are resending with authentication.
	return _MakeRequest();
}


status_t
BHTTPRequest::_MakeRequest()
{
	_PrepareHeaders();
	_PrepareRequestBody();

	if (fContext->UseProxy()) {
		host = fContext->GetProxyHost();
		port = fContext->GetProxyPort();
	} else {
		host = fURL.Host();
		port = _GetPort();
	}

	fState = CREATING_STREAM;

	status_t status;

	if (fContext != NULL && fContext->HTTPCache() != NULL) {
		return fContext->HTTPCache()->CreateStream(fHTTPStream, url,
			&fRequestHeaders, _fCompletionCallback);		
	}

	fHTTPStream = new(std::nothrow) HTTPNetworkStream(url, &fRequestHeaders,
		fRequestBody);

	if (fHTTPStream == NULL)
		return B_NO_MEMORY;

	return B_OK;
}


void
BHTTPRequest::_ErrorOccurred(status_t error)
{
	fListener->RequestFailed(this, error);
}


void
BHTTPRequest::_HeaderReceived(BString name, BString value)
{
	fResult.fHeaders.AddHeader(name, value);

	BString cookieName = name.Trim().CapitalizeEachWord();

	if (fOptSetCookies && fContext != NULL && cookieName == "Set-Cookie")
		fContext->GetCookieJar().AddCookie(value, fURL);
	else if (cookieName == "Content-Encoding")
		_SetupDecompressionStream();
}


void
BHTTPRequest::_HeadersComplete()
{
	// Set up BHTTPResult values
	fResult.fStatusCode = fHTTPStream.StatusCode();
	fResult.fStatusString = fHTTPStream.StatusString();
	fResult.fURL = fURL;

	fListener->HeadersReceived(this);
}


void
BHTTPRequest::_DataReceived(const void* data, size_t length)
{
	fListener->DataReceived(this, data, -1, length); // TODO
}


void
BHTTPRequest::_RequestComplete()
{
	fListener->RequestCompleted(this, true);

	fState = RESPONSE_RECEIVED;
	_OnCompletion(B_OK);
}


void
BHTTPRequest::_PrepareHeaders()
{
	// HTTP 1.1 additional headers
	if (fHTTPVersion == B_HTTP_11) {
		BString host = URL().Host();
		if (URL().HasPort() && !_IsDefaultPort(URL().Port()))
			host << ':' << URL().Port();

		fRequestHeaders.AddHeader("Host", host);
		fRequestHeaders.AddHeader("Accept", "*/*");
		fRequestHeaders.AddHeader("Accept-Encoding", "gzip");
			// Allows the server to compress data using the "gzip" format.
			// "deflate" is not supported, because there are two interpretations
			// of what it means (the RFC and Microsoft products), and we don't
			// want to handle this. Very few websites support only deflate,
			// and most of them will send gzip, or at worst, uncompressed data.

		BString connection = _ShouldKeepAlive() ? "keep-alive" : "close";
		fRequestHeaders.AddHeader("Connection", connection);
	}

	// Classic HTTP headers
	if (fOptUserAgent.CountChars() > 0)
		fRequestHeaders.AddHeader("User-Agent", fOptUserAgent.String());

	if (fOptReferer.CountChars() > 0)
		fRequestHeaders.AddHeader("Referer", fOptReferer.String());

	// Authentication
	if (fContext != NULL) {
		BHttpAuthentication& authentication = fContext->GetAuthentication(fUrl);
		if (authentication.Method() != B_HTTP_AUTHENTICATION_NONE) {
			if (fOptUsername.Length() > 0) {
				authentication.SetUserName(fOptUsername);
				authentication.SetPassword(fOptPassword);
			}

			BString request(fRequestMethod);
			fRequestHeaders.AddHeader("Authorization",
				authentication.Authorization(fURL, request));
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

		fRequestHeaders.AddHeader("Content-Type", contentType);
		fRequestHeaders.AddHeader("Content-Length",
			fOptPostFields->ContentLength());
	} else if (fOptInputData != NULL) {
		if (fOptInputDataSize >= 0)
			fRequestHeaders.AddHeader("Content-Length", fOptInputDataSize);
		else
			fRequestHeaders.AddHeader("Transfer-Encoding", "chunked");
	}

	// Optional headers specified by the user
	for (int32 headerIndex = 0; headerIndex < fUserHeaders->CountHeaders();
			headerIndex++) {
		BHttpHeader& optHeader = fUserHeaders[headerIndex];
		int32 replaceIndex = fRequestHeaders.HasHeader(optHeader.Name());

		// Add or replace the current option header to the
		// output header list
		if (replaceIndex == -1)
			fRequestHeaders.AddHeader(optHeader.Name(), optHeader.Value());
		else
			fRequestHeaders[replaceIndex].SetValue(optHeader.Value());
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

			fRequestHeaders.AddHeader("Cookie", cookieString);
		}
	}
}


bool
BHTTPRequest::_CertificateVerificationFailed(BCertificate& certificate,
	const char* message)
{
	if (fListener != NULL) {
		return fListener->CertificateVerificationFailed(this, certificate,
			message);
	}

	return false;
}


bool
BHTTPRequest::_IsDefaultPort()
{
	if (fSSL && Url().Port() == 443)
		return true;
	if (!fSSL && Url().Port() == 80)
		return true;
	return false;
}


bool
BHTTPRequest::_ShouldKeepAlive()
{
	if (fContext == NULL)
		return false;

	if (fContext->HTTPCache() == NULL)
		return false;

	// TODO: this should really be a method on the stream, but the problem
	// is we prepare the headers before the stream.
	return fContext->HTTPCache()->ShouldKeepAlive();
}


/*static*/ bool
BHTTPRequest::IsInformationalStatusCode(int16 code)
{
	return (code >= B_HTTP_STATUS__INFORMATIONAL_BASE)
		&& (code <  B_HTTP_STATUS__INFORMATIONAL_END);
}


/*static*/ bool
BHTTPRequest::IsSuccessStatusCode(int16 code)
{
	return (code >= B_HTTP_STATUS__SUCCESS_BASE)
		&& (code <  B_HTTP_STATUS__SUCCESS_END);
}


/*static*/ bool
BHTTPRequest::IsRedirectionStatusCode(int16 code)
{
	return (code >= B_HTTP_STATUS__REDIRECTION_BASE)
		&& (code <  B_HTTP_STATUS__REDIRECTION_END);
}


/*static*/ bool
BHTTPRequest::IsClientErrorStatusCode(int16 code)
{
	return (code >= B_HTTP_STATUS__CLIENT_ERROR_BASE)
		&& (code <  B_HTTP_STATUS__CLIENT_ERROR_END);
}


/*static*/ bool
BHTTPRequest::IsServerErrorStatusCode(int16 code)
{
	return (code >= B_HTTP_STATUS__SERVER_ERROR_BASE)
		&& (code <  B_HTTP_STATUS__SERVER_ERROR_END);
}


/*static*/ int16
BHTTPRequest::StatusCodeClass(int16 code)
{
	if (BHTTPRequest::IsInformationalStatusCode(code))
		return B_HTTP_STATUS_CLASS_INFORMATIONAL;
	else if (BHTTPRequest::IsSuccessStatusCode(code))
		return B_HTTP_STATUS_CLASS_SUCCESS;
	else if (BHTTPRequest::IsRedirectionStatusCode(code))
		return B_HTTP_STATUS_CLASS_REDIRECTION;
	else if (BHTTPRequest::IsClientErrorStatusCode(code))
		return B_HTTP_STATUS_CLASS_CLIENT_ERROR;
	else if (BHTTPRequest::IsServerErrorStatusCode(code))
		return B_HTTP_STATUS_CLASS_SERVER_ERROR;

	return B_HTTP_STATUS_CLASS_INVALID;
}
