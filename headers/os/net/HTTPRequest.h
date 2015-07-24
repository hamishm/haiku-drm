/*
 * Copyright 2010-2015 Haiku Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _HTTP_REQUEST_H
#define _HTTP_REQUEST_H


#include <Certificate.h>
#include <HttpForm.h>
#include <HttpHeaders.h>
#include <HttpResult.h>
#include <NetworkAddress.h>
#include <NetworkRequest.h>


class BHTTPRequest : public BURLRequest {
public:
								BHTTPRequest(const BUrl& url,
									BUrlProtocolListener* listener = NULL,
									BUrlContext* context = NULL);
	virtual						~BHTTPRequest();

			void				SetMethod(const char* const method);
			void				SetFollowLocation(bool follow);

			void				SetMaxRedirections(int8 maxRedirections);
			void				SetReferrer(const BString& referrer);
			void				SetUserAgent(const BString& agent);
			void				SetAutoReferrer(bool enable);

			void				SetUserName(const BString& name);
			void				SetPassword(const BString& password);

			void				SetRangeStart(off_t position);
			void				SetRangeEnd(off_t position);

			void				SetPostFields(const BHttpForm& fields);

			void				AdoptPostFields(BHttpForm* const fields);
			void				AdoptInputData(BDataIO* const data,
									const ssize_t size = -1);

			BHttpHeaders&		RequestHeaders();
			const BUrl&			URL();

			status_t			Stop();
			const BUrlResult&	Result() const;

	static	bool				IsInformationalStatusCode(int16 code);
	static	bool				IsSuccessStatusCode(int16 code);
	static	bool				IsRedirectionStatusCode(int16 code);
	static	bool				IsClientErrorStatusCode(int16 code);
	static	bool				IsServerErrorStatusCode(int16 code);
	static	int16				StatusCodeClass(int16 code);

private:
			// Callbacks for the HTTPStream
	friend class HTTPStream;

			void				_ErrorOccurred(status_t error);
			void				_HeaderReceived(BString name, BString value);
			void				_HeadersComplete();
			void				_DataSent(size_t length);
			void				_DataReceived(const void* data, size_t length);
			void				_RequestComplete();

			// Internal methods
			void				_OnComplete(status_t result);
			status_t			_NextState(status_t result);
			status_t			_NextRequest();
			status_t			_MakeRequest();

			void				_PrepareHeaders();
			void				_PrepareRequestBody();

			status_t			_HandleRedirection(int statusCode);
			status_t			_HandleUnauthorized();

			// SSL failure management
	friend	class				CheckedSecureSocket;
			bool				_CertificateVerificationFailed(
									BCertificate& certificate,
									const char* message);

			// Utility methods
			bool				_IsDefaultPort();
			void				_ResetOptions();
			bool				_ShouldKeepAlive();

private:
			BString				fRequestMethod;
			int8				fHTTPVersion;

			BHttpHeaders		fRequestHeaders;
			BHttpHeaders		fUserHeaders;

			BHttpResult			fResult;

			// Request state
			enum {
				INITIAL,
				CREATING_STREAM,
				SENDING_REQUEST,
				RESPONSE_RECEIVED,
				REQUEST_COMPLETED
			}					fState;

			int64				fDataSent;

			int					fRedirectCount;

			// Protocol options
			BString				fOptReferer;
			BString				fOptUserAgent;
			BString				fOptUsername;
			BString				fOptPassword;

			BHttpForm*			fOptPostFields;
			BDataIO*			fOptInputData;
			int64				fOptInputDataSize;
			off_t				fOptRangeStart;
			off_t				fOptRangeEnd;

			uint32				fOptAuthMethods;
			uint8				fOptMaxRedirects;

			bool				fOptSetCookies : 1;
			bool				fOptFollowLocation : 1;
			bool				fOptAutoReferer : 1;
};


inline BHttpHeaders&
BHTTPRequest::RequestHeaders()
{
	return fUserHeaders;
}


inline const BUrl&
BHTTPRequest::URL()
{
	return fURL;
}


inline void
BHTTPRequest::SetMethod(const char* const method)
{
	fRequestMethod = method;
}


inline void
BHTTPRequest::SetFollowLocation(bool follow)
{
	fOptFollowLocation = follow;
}


inline void
BHTTPRequest::SetMaxRedirections(int8 redirections)
{
	fOptMaxRedirects = redirections;
}


inline void
BHTTPRequest::SetReferrer(const BString& referrer)
{
	fOptReferer = referrer;
}


inline void
BHTTPRequest::SetUserAgent(const BString& agent)
{
	fOptUserAgent = agent;
}


inline void
BHTTPRequest::SetAutoReferrer(bool enable)
{
	fOptAutoReferer = enable;
}


void
BHTTPRequest::SetPostFields(const BHttpForm& fields)
{
	AdoptPostFields(new(std::nothrow) BHttpForm(fields));
}


void
BHTTPRequest::SetUserName(const BString& name)
{
	fOptUsername = name;
}


void
BHTTPRequest::SetPassword(const BString& password)
{
	fOptPassword = password;
}




// Request method
const char* const B_HTTP_GET = "GET";
const char* const B_HTTP_POST = "POST";
const char* const B_HTTP_PUT = "PUT";
const char* const B_HTTP_HEAD = "HEAD";
const char* const B_HTTP_DELETE = "DELETE";
const char* const B_HTTP_OPTIONS = "OPTIONS";
const char* const B_HTTP_TRACE = "TRACE";
const char* const B_HTTP_CONNECT = "CONNECT";


// HTTP Version
enum {
	B_HTTP_10 = 1,
	B_HTTP_11
};


// HTTP status classes
enum http_status_code_class {
	B_HTTP_STATUS_CLASS_INVALID			= 000,
	B_HTTP_STATUS_CLASS_INFORMATIONAL 	= 100,
	B_HTTP_STATUS_CLASS_SUCCESS			= 200,
	B_HTTP_STATUS_CLASS_REDIRECTION		= 300,
	B_HTTP_STATUS_CLASS_CLIENT_ERROR	= 400,
	B_HTTP_STATUS_CLASS_SERVER_ERROR	= 500
};


// Known HTTP status codes
enum http_status_code {
	// Informational status codes
	B_HTTP_STATUS__INFORMATIONAL_BASE	= 100,
	B_HTTP_STATUS_CONTINUE = B_HTTP_STATUS__INFORMATIONAL_BASE,
	B_HTTP_STATUS_SWITCHING_PROTOCOLS,
	B_HTTP_STATUS__INFORMATIONAL_END,

	// Success status codes
	B_HTTP_STATUS__SUCCESS_BASE			= 200,
	B_HTTP_STATUS_OK = B_HTTP_STATUS__SUCCESS_BASE,
	B_HTTP_STATUS_CREATED,
	B_HTTP_STATUS_ACCEPTED,
	B_HTTP_STATUS_NON_AUTHORITATIVE_INFORMATION,
	B_HTTP_STATUS_NO_CONTENT,
	B_HTTP_STATUS_RESET_CONTENT,
	B_HTTP_STATUS_PARTIAL_CONTENT,
	B_HTTP_STATUS__SUCCESS_END,

	// Redirection status codes
	B_HTTP_STATUS__REDIRECTION_BASE		= 300,
	B_HTTP_STATUS_MULTIPLE_CHOICE = B_HTTP_STATUS__REDIRECTION_BASE,
	B_HTTP_STATUS_MOVED_PERMANENTLY,
	B_HTTP_STATUS_FOUND,
	B_HTTP_STATUS_SEE_OTHER,
	B_HTTP_STATUS_NOT_MODIFIED,
	B_HTTP_STATUS_USE_PROXY,
	B_HTTP_STATUS_TEMPORARY_REDIRECT,
	B_HTTP_STATUS__REDIRECTION_END,

	// Client error status codes
	B_HTTP_STATUS__CLIENT_ERROR_BASE	= 400,
	B_HTTP_STATUS_BAD_REQUEST = B_HTTP_STATUS__CLIENT_ERROR_BASE,
	B_HTTP_STATUS_UNAUTHORIZED,
	B_HTTP_STATUS_PAYMENT_REQUIRED,
	B_HTTP_STATUS_FORBIDDEN,
	B_HTTP_STATUS_NOT_FOUND,
	B_HTTP_STATUS_METHOD_NOT_ALLOWED,
	B_HTTP_STATUS_NOT_ACCEPTABLE,
	B_HTTP_STATUS_PROXY_AUTHENTICATION_REQUIRED,
	B_HTTP_STATUS_REQUEST_TIMEOUT,
	B_HTTP_STATUS_CONFLICT,
	B_HTTP_STATUS_GONE,
	B_HTTP_STATUS_LENGTH_REQUIRED,
	B_HTTP_STATUS_PRECONDITION_FAILED,
	B_HTTP_STATUS_REQUEST_ENTITY_TOO_LARGE,
	B_HTTP_STATUS_REQUEST_URI_TOO_LARGE,
	B_HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE,
	B_HTTP_STATUS_REQUESTED_RANGE_NOT_SATISFIABLE,
	B_HTTP_STATUS_EXPECTATION_FAILED,
	B_HTTP_STATUS__CLIENT_ERROR_END,

	// Server error status codes
	B_HTTP_STATUS__SERVER_ERROR_BASE 	= 500,
	B_HTTP_STATUS_INTERNAL_SERVER_ERROR = B_HTTP_STATUS__SERVER_ERROR_BASE,
	B_HTTP_STATUS_NOT_IMPLEMENTED,
	B_HTTP_STATUS_BAD_GATEWAY,
	B_HTTP_STATUS_SERVICE_UNAVAILABLE,
	B_HTTP_STATUS_GATEWAY_TIMEOUT,
	B_HTTP_STATUS__SERVER_ERROR_END
};


#endif // _HTTP_REQUEST_H
