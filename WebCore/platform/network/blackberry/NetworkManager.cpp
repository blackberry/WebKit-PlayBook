/*
 * Copyright (C) 2009, 2010, 2011 Research In Motion Limited. All rights reserved.
 */

#include "config.h"
#include "NetworkManager.h"

#include "AboutData.h"
#include "AuthenticationChallenge.h"
#include "Base64.h"
#include "Chrome.h"
#include "CookieManager.h"
#include "CredentialStorage.h"
#include "CString.h"
#include "Deque.h"
#include "Frame.h"
#include "FrameLoaderClient.h"
#include "FrameLoaderClientBlackBerry.h"
#include "FormData.h"
#include "HTTPParsers.h"
#include "Logging.h"
#include "MIMETypeRegistry.h"
#include "NetworkStreamFactory.h"
#include "NotImplemented.h"
#include "BlackBerryCookieCache.h"
#include "BlackBerryPlatformMisc.h"
#include "Page.h"
#include "PageClientBlackBerry.h"
#include "PlatformString.h"
#include "ResourceError.h"
#include "ResourceHandle.h"
#include "ResourceHandleClient.h"
#include "ResourceHandleInternal.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "Timer.h"
#include "streams/FilterStream.h"
#include "streams/NetworkRequest.h"

#include <BuildInformation.h>

using WTF::CString;
using WTF::Deque;
using WTF::HashMap;
using WTF::RefPtr;
using WebCore::AuthenticationChallenge;
using WebCore::CookieManager;
using WebCore::Credential;
using WebCore::CredentialStorage;
using WebCore::HTTPHeaderMap;
using WebCore::Frame;
using WebCore::FormData;
using WebCore::FormDataElement;
using WebCore::KURL;
using WebCore::LogNetwork;
using WebCore::MIMETypeRegistry;
using WebCore::BlackBerryCookieCache;
using WebCore::Page;
using WebCore::ProtectionSpace;
using WebCore::ProtectionSpaceServerType;
using WebCore::ProtectionSpaceAuthenticationScheme;
using WebCore::ResourceError;
using WebCore::ResourceHandle;
using WebCore::ResourceHandleClient;
using WebCore::ResourceRequest;
using WebCore::ResourceRequestCachePolicy;
using WebCore::ResourceResponse;
using WebCore::Timer;

namespace Olympia {

namespace WebKit {

static const int redirectMaximum = 10;

/***
 * Helper functions
 */

static inline int charToHex(char c)
{
    return c >= '0' && c <= '9' ? c - '0'
        : c >= 'a' && c <= 'f' ? c - 'a' + 10
        : c >= 'A' && c <= 'F' ? c - 'A' + 10
        : 0;
}

static void escapeDecode(const char* src, int length, Vector<char>& out)
{
    out.resize(length);

    const char* const srcEnd = src + length;
    char *dst = out.data();
    for (; src < srcEnd; ) {
        char inputChar = *src++;
        if (inputChar == '%' && src + 2 <= srcEnd) {
            int digit1 = charToHex(*src++);
            int digit2 = charToHex(*src++);
            *dst++ = (digit1 << 4) | digit2;
        } else {
            *dst++ = inputChar;
        }
    }

    out.resize(dst - out.data());
}

class RecursionGuard
{
public:
    RecursionGuard(bool &guard)
        : m_guard(guard)
    {
        ASSERT(!m_guard);
        m_guard = true;
    }

    ~RecursionGuard()
    {
        m_guard = false;
    }

private:
    bool& m_guard;
};

/***
 * DeferredData declaration
 */

class NetworkJob;

class DeferredData
{
public:
    DeferredData(NetworkJob& job);

    void deferOpen(int status, const WTF::String& message);
    void deferWMLOverride();
    void deferHeaderReceived(const WTF::String& key, const WTF::String& value);
    void deferDataReceived(const char* buf, size_t len);
    void deferDataSent(unsigned long long bytesSent, unsigned long long totalBytesToBeSent);
    void deferDone();

    bool hasDeferredData() const
    {
        return m_deferredOpen || m_deferredWMLOverride || !m_headerKeys.isEmpty() || !m_dataSegments.isEmpty() || m_deferredDone;
    }

    void processDeferredData();

    void scheduleProcessDeferredData()
    {
        if (!m_processDataTimer.isActive())
            m_processDataTimer.startOneShot(0);
    }

private:
    void fireProcessDataTimer(Timer<DeferredData>*);

    NetworkJob& m_job;
    Timer<DeferredData> m_processDataTimer;

    bool m_deferredOpen;
    int m_status;
    WTF::String m_message;
    bool m_deferredWMLOverride;
    Vector<WTF::String> m_headerKeys;
    Vector<WTF::String> m_headerValues;
    Deque< Vector<char> > m_dataSegments;
    unsigned long long m_bytesSent;
    unsigned long long m_totalBytesToBeSent;
    bool m_deferredDone;
    bool m_inProcessDeferredData;
};


/***
 * NetworkJob class
 */

class NetworkJob : public Platform::FilterStream
{
public:
    NetworkJob()
        : Platform::FilterStream()
        , m_playerId(0)
        , m_loadDataTimer(this, &NetworkJob::fireLoadDataTimer)
        , m_loadAboutTimer(this, &NetworkJob::fireLoadAboutTimer)
        , m_deleteJobTimer(this, &NetworkJob::fireDeleteJobTimer)
        , m_streamFactory(0)
        , m_isFile(false)
        , m_isData(false)
        , m_isAbout(false)
        , m_isFTP(false)
        , m_isFTPDir(true)
        , m_isRunning(true)  // always started immediately after creation
        , m_cancelled(false)
        , m_statusReceived(false)
        , m_dataReceived(false)
        , m_responseSent(false)
        , m_extendedStatusCode(0)
        , m_redirectCount(0)
        , m_deferredData(*this)
        , m_deferLoadingCount(0)
        , m_callingClient(false)
        , m_isXHR(false)
    {
    }

    bool initialize(int playerId, const WTF::String& pageGroupName, const KURL& url, const Platform::NetworkRequest& request, PassRefPtr<ResourceHandle> handle, Platform::NetworkStreamFactory* streamFactory, const Frame& frame, int deferLoadingCount, int redirectCount)
    {
        m_playerId = playerId;
        m_pageGroupName = pageGroupName;

        m_response.setURL(url);
        m_isFile = url.protocolIs("file") || url.protocolIs("local");
        m_isData = url.protocolIs("data");
        m_isAbout = url.protocolIs("about");
        m_isFTP = url.protocolIs("ftp");

        m_handle = handle;

        m_streamFactory = streamFactory;
        m_frame = &frame;
        m_redirectCount = redirectCount;
        m_deferLoadingCount = deferLoadingCount;

        Platform::FilterStream* wrappedStream = m_streamFactory->createNetworkStream(request, m_playerId);
        if (!wrappedStream)
            return false;
        setWrappedStream(wrappedStream);

        m_isXHR = request.getTargetType() == Olympia::Platform::NetworkRequest::TargetIsXMLHTTPRequest;

        return true;
    }

    PassRefPtr<ResourceHandle> handle() const
    {
        return m_handle;
    }

    bool isRunning() const
    {
        return m_isRunning;
    }

    bool isCancelled() const
    {
        return m_cancelled;
    }

    bool clientIsOk() const
    {
        return !m_cancelled && m_handle && m_handle->client();
    }

    static bool isInfo(int statusCode)
    {
        return (100 <= statusCode && statusCode < 200);
    }

    static bool isRedirect(int statusCode)
    {
        return (300 <= statusCode && statusCode < 400 && statusCode != 304);
    }

    bool isError(int statusCode)
    {
        return (statusCode < 0 || (!m_isXHR && (400 <= statusCode && statusCode < 600)));
    }

    static bool isUnauthorized(int statusCode)
    {
        return (statusCode == 401);
    }

    void loadDataURL()
    {
        m_loadDataTimer.startOneShot(0);
    }

    bool loadAboutURL()
    {
        WTF::String aboutWhat(m_response.url().string().substring(6)); // first 6 chars are "about:"

        if (!aboutWhat.isEmpty()
                && !equalIgnoringCase(aboutWhat, "blank")
                && !equalIgnoringCase(aboutWhat, "credits")
#if !defined(PUBLIC_BUILD) || !PUBLIC_BUILD
                && !equalIgnoringCase(aboutWhat, "version")
                && (Platform::debugSetting() == 0
                    || (!equalIgnoringCase(aboutWhat, "config")
                        && !equalIgnoringCase(aboutWhat, "build")
                        && !equalIgnoringCase(aboutWhat, "memory")))
#endif
                )
            return false;

        m_loadAboutTimer.startOneShot(0);
        return true;
    }

    int cancelJob()
    {
        m_cancelled = true;

        // cancel jobs loading local data by killing the timer, and jobs
        // getting data from the network by calling the inherited
        // URLStream::cancel
        if (m_loadDataTimer.isActive()) {
            m_loadDataTimer.stop();
            notifyDone();
            return 0;
        }

        if (m_loadAboutTimer.isActive()) {
            m_loadAboutTimer.stop();
            notifyDone();
            return 0;
        }

        return cancel();
    }

    bool isDeferringLoading() const
    {
        return m_deferLoadingCount > 0;
    }

    void updateDeferLoadingCount(int delta)
    {
        m_deferLoadingCount += delta;
        ASSERT(m_deferLoadingCount >= 0);

        if (!isDeferringLoading()) {
            // there might already be a timer set to call this, but it's safe to schedule it again.
            m_deferredData.scheduleProcessDeferredData();
        }
    }

    /***
     * notifyOpen
     */

    virtual void notifyOpen(int status, const char* message)
    {
        if (shouldDeferLoading())
            m_deferredData.deferOpen(status, message);
        else
            handleNotifyOpen(status, message);
    }

    void handleNotifyOpen(int status, const WTF::String& message)
    {
        // check for messages out of order or after cancel
        if ((m_statusReceived && m_extendedStatusCode != 401) || m_responseSent || m_cancelled)
            return;

        if (isInfo(status))
            return;     // ignore

        m_statusReceived = true;

        // convert non-HTTP status codes to generic HTTP codes
        m_extendedStatusCode = status;
        if (status == 0) {
            m_response.setHTTPStatusCode(200);
        } else if (status < 0) {
            m_response.setHTTPStatusCode(404);
        } else {
            m_response.setHTTPStatusCode(status);
        }

        m_response.setHTTPStatusText(message);
    }

    /***
     * notifyWMLOverride
     */

    virtual void notifyWMLOverride()
    {
        if (shouldDeferLoading())
            m_deferredData.deferWMLOverride();
        else
            handleNotifyWMLOverride();
    }

    void handleNotifyWMLOverride()
    {
        m_response.setIsWML(true);
    }

    /***
     * notifyHeaderReceived
     */

    virtual void notifyHeaderReceived(const char* key, const char* value)
    {
        if (shouldDeferLoading())
            m_deferredData.deferHeaderReceived(key, value);
        else {
            WTF::String keyString(key);
            WTF::String valueString;
            if (equalIgnoringCase(keyString, "Location")) {
                // Location, like all headers, is supposed to be Latin-1.  But some sites (wikipedia) send it in UTF-8.
                // All byte strings that are valid UTF-8 are also valid Latin-1 (although outside ASCII, the meaning will
                // differ), but the reverse isn't true.  So try UTF-8 first and fall back to Latin-1 if it's invalid.
                // (High Latin-1 should be url-encoded anyway.)
                //
                // FIXME: maybe we should do this with other headers?
                // Skip it for now - we don't want to rewrite random bytes unless we're sure. (Definitely don't want to
                // rewrite cookies, for instance.) Needs more investigation.
                valueString = WTF::String::fromUTF8(value);
                if (valueString.isNull())
                    valueString = value;
            } else
                valueString = value;

            handleNotifyHeaderReceived(keyString, valueString);
        }
    }

    // exists only to resolve ambiguity between char* and String parameters
    void notifyStringHeaderReceived(const WTF::String& key, const WTF::String& value)
    {
        if (shouldDeferLoading())
            m_deferredData.deferHeaderReceived(key, value);
        else
            handleNotifyHeaderReceived(key, value);
    }

    void handleNotifyHeaderReceived(const WTF::String& key, const WTF::String& value)
    {
        // check for messages out of order or after cancel
        if (!m_statusReceived || m_responseSent || m_cancelled)
            return;

        WTF::String lowerKey = key.lower();
        if (lowerKey == "content-type")
            m_contentType = value;

        if (lowerKey == "content-disposition")
            m_contentDisposition = value;

        if (lowerKey == "set-cookie") {
            if (m_frame && m_frame->loader() && m_frame->loader()->client()
                && static_cast<WebCore::FrameLoaderClientBlackBerry*>(m_frame->loader()->client())->cookiesEnabled())
#if OS(QNX)
                handleSetCookieHeader(value);
#endif
                BlackBerryCookieCache::instance().clearAllCookiesForHost(m_pageGroupName, m_response.url());

        }

        if (lowerKey == "www-authenticate")
            handleAuthHeader(value);

        if (lowerKey == Platform::NetworkRequest::HEADER_OLYMPIA_FTP)
            handleFTPHeader(value);

        m_response.setHTTPHeaderField(key, value);
    }

    void handleSetCookieHeader(const WTF::String& value)
    {
        KURL url = m_response.url();
        CookieManager& cookieManager = WebCore::cookieManager();
        if ((cookieManager.cookiePolicy() == WebCore::CookieStorageAcceptPolicyOnlyFromMainDocumentDomain)
          && (m_handle->firstRequest().firstPartyForCookies() != url)
          && cookieManager.getCookie(url, WebCore::WithHttpOnlyCookies).isEmpty())
            return;
        cookieManager.setCookies(url, value);
    }

    /***
     * notifyDataReceived
     */

    virtual void notifyDataReceived(const char* buf, size_t len)
    {
        if (shouldDeferLoading())
            m_deferredData.deferDataReceived(buf, len);
        else
            handleNotifyDataReceived(buf, len);
    }

    void handleNotifyDataReceived(const char* buf, size_t len)
    {
        // check for messages out of order or after cancel
        if ((!m_isFile && !m_statusReceived) || m_cancelled)
            return;

        if (!buf || !len)
            return;

        m_dataReceived = true;

        // protect against reentrancy
        updateDeferLoadingCount(1);

        if (!isRedirect(m_extendedStatusCode)) {
            sendResponseIfNeeded();
            if (clientIsOk()) {
                RecursionGuard guard(m_callingClient);
                m_handle->client()->didReceiveData(m_handle.get(), buf, len, len);
            }
        }

        updateDeferLoadingCount(-1);
    }

    /***
     * notifyDataSent
     */

    virtual void notifyDataSent(unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
    {
        if (shouldDeferLoading())
            m_deferredData.deferDataSent(bytesSent, totalBytesToBeSent);
        else
            handleNotifyDataSent(bytesSent, totalBytesToBeSent);
    }

    void handleNotifyDataSent(unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
    {
        if (m_cancelled)
            return;

        // protect against reentrancy
        updateDeferLoadingCount(1);

        if (clientIsOk()) {
            RecursionGuard guard(m_callingClient);
            m_handle->client()->didSendData(m_handle.get(), bytesSent, totalBytesToBeSent);
        }

        updateDeferLoadingCount(-1);
    }

    /***
     * notifyDone
     */

    virtual void notifyDone()
    {
        if (shouldDeferLoading())
            m_deferredData.deferDone();
        else
            handleNotifyDone();
    }

    void handleNotifyDone()
    {
        m_isRunning = false;

        if (!m_cancelled) {
            if (!m_statusReceived) {
                // connection failed before sending notifyOpen: use generic NetworkError
                notifyOpen(Platform::FilterStream::StatusNetworkError, 0);
            }

            // If an HTTP authentication-enabled request is successful, save
            // the credentials for later reuse. If the request fails, delete
            // the saved credentials.
            if (!isError(m_extendedStatusCode)) {
                storeCredentials();
            } else if (isUnauthorized(m_extendedStatusCode)) {
                purgeCredentials();
            }

            if (!isRedirect(m_extendedStatusCode) || (m_redirectCount >= redirectMaximum) || !handleRedirect()) {
                if (isRedirect(m_extendedStatusCode) && (m_redirectCount >= redirectMaximum))
                    m_extendedStatusCode = Platform::FilterStream::StatusTooManyRedirects;

                sendResponseIfNeeded();
                if (clientIsOk()) {
                    RecursionGuard guard(m_callingClient);
                    if (isError(m_extendedStatusCode) && !m_dataReceived) {
                        WTF::String domain = m_extendedStatusCode < 0 ? ResourceError::platformErrorDomain : ResourceError::httpErrorDomain;
                        ResourceError error(domain, m_extendedStatusCode, m_response.url().string(), m_response.httpStatusText());
                        m_handle->client()->didFail(m_handle.get(), error);
                    } else {
                        m_handle->client()->didFinishLoading(m_handle.get(), 0);
                    }
                }
            }
        }

        // whoever called notifyDone still have a reference to the job, so
        // schedule the deletion with a timer
        m_deleteJobTimer.startOneShot(0);

        // Detach from the ResourceHandle in any case.
        m_handle = 0;
    }

private:

    bool shouldDeferLoading() const
    {
        return isDeferringLoading() || m_deferredData.hasDeferredData();
    }

    bool handleRedirect()
    {
        ASSERT(m_handle);
        if (!m_handle)
            return false;

        WTF::String location = m_response.httpHeaderField("Location");
        if (location.isNull())
            return false;

        KURL newURL(m_response.url(), location);
        if (!newURL.isValid())
            return false;

        ResourceRequest newRequest = m_handle->firstRequest();
        newRequest.setURL(newURL);
        newRequest.setMustHandleInternally(true);

        WTF::String method = newRequest.httpMethod().upper();
        if ((method != "GET") && (method != "HEAD")) {
            newRequest.setHTTPMethod("GET");
            newRequest.setHTTPBody(0);
            newRequest.setHTTPHeaderField("Content-Length", WTF::String());
            newRequest.setHTTPHeaderField("Content-Type", WTF::String());
        }

        // Do not send existing credentials with the new request
        m_handle->getInternal()->m_currentWebChallenge.nullify();

        if (clientIsOk()) {
            RecursionGuard guard(m_callingClient);
            m_handle->client()->willSendRequest(m_handle.get(), newRequest, m_response);

            // cancelled can become true if the redirected url fails the policy check
            if (m_cancelled)
                return false;
        }

        // Pass the ownership of the ResourceHandle to the new NetworkJob.
        RefPtr<ResourceHandle> handle = m_handle;
        m_handle = 0;
        NetworkManager::instance()->startJob(m_playerId, m_pageGroupName, handle, newRequest, m_streamFactory, *m_frame, m_deferLoadingCount, m_redirectCount + 1);

        return true;
    }

    // this can cause m_cancelled to be set to true, if it passes up an error to m_handle->client() which causes the job to be cancelled
    void sendResponseIfNeeded()
    {
        if (m_responseSent)
            return;

        m_responseSent = true;

        // check for errors
        if (isError(m_extendedStatusCode) && !m_dataReceived)
            return;

        WTF::String urlFilename = m_response.url().lastPathComponent();

        // get the MIME type that was set by the content sniffer
        // if there's no custom sniffer header, try to set it from the Content-Type header
        // if this fails, guess it from extension
        WTF::String mimeType;
        if (m_isFTP && m_isFTPDir)
            mimeType = "application/x-ftp-directory";
        else
            mimeType = m_response.httpHeaderField(Platform::NetworkRequest::HEADER_RIM_SNIFFED_MIME_TYPE);
        if (mimeType.isNull())
            mimeType = WebCore::extractMIMETypeFromMediaType(m_contentType);
        if (mimeType.isNull())
            mimeType = MIMETypeRegistry::getMIMETypeForPath(urlFilename);
        m_response.setMimeType(mimeType);

        // set encoding from Content-Type header
        m_response.setTextEncodingName(WebCore::extractCharsetFromMediaType(m_contentType));

        // set content length from header
        WTF::String contentLength = m_response.httpHeaderField("Content-Length");
        if (!contentLength.isNull())
            m_response.setExpectedContentLength(contentLength.toInt64());

        // set suggested filename for downloads from the Content-Disposition header; if this fails, fill it in from the url
        // skip this for data url's, because they have no Content-Disposition header and the format is wrong to be a filename
        if (!m_isData && !m_isAbout) {
            WTF::String suggestedFilename = WebCore::filenameFromHTTPContentDisposition(m_contentDisposition);
            if (suggestedFilename.isNull())
                suggestedFilename = urlFilename;
            m_response.setSuggestedFilename(suggestedFilename);
        }

        // make sure local files aren't cached, since this just duplicates them
        if (m_isFile || m_isData || m_isAbout)
            m_response.setHTTPHeaderField("Cache-Control", "no-cache");

        if (clientIsOk()) {
            RecursionGuard guard(m_callingClient);
            m_handle->client()->didReceiveResponse(m_handle.get(), m_response);
        }
    }

    void fireLoadDataTimer(Timer<NetworkJob>*)
    {
        parseData();
    }

    void fireLoadAboutTimer(Timer<NetworkJob>*)
    {
        handleAbout();
    }

    void fireDeleteJobTimer(Timer<NetworkJob>*)
    {
        NetworkManager::instance()->deleteJob(this);
    }

    void parseData()
    {
        Vector<char> result;

        WTF::String contentType("text/plain;charset=US-ASCII");

        WTF::String data(m_response.url().string().substring(5));
        Vector<WTF::String> hparts;
        bool isBase64 = false;

        size_t index = data.find(',');
        if (index != WTF::notFound && index > 0) {
            contentType = data.left(index).lower();
            data = data.substring(index + 1);

            contentType.split(';', hparts);
            Vector<WTF::String>::iterator i;
            for (i = hparts.begin(); i != hparts.end(); ++i) {
                if (i->stripWhiteSpace().lower() == "base64") {
                    isBase64 = true;
                    WTF::String value = *i;
                    do {
                        if (*i == value) {
                            int position = i - hparts.begin();
                            hparts.remove(position);
                            i = hparts.begin() + position;
                        } else {
                            ++i;
                        }
                    } while (i != hparts.end());
                    break;
                }
            }
            contentType = WTF::String();
            for (i = hparts.begin(); i != hparts.end(); ++i) {
                if (i > hparts.begin())
                    contentType += ",";

                contentType += *i;
            }
        } else if (index == 0) {
            data = data.substring(1);  // broken header
        }

        {
            CString latin = data.latin1();
            escapeDecode(latin.data(), latin.length(), result);
        }

        if (isBase64) {
            WTF::String s(result.data(), result.size());
            CString latin = s.removeCharacters(WTF::isSpaceOrNewline).latin1();
            result.clear();
            result.append(latin.data(), latin.length());
            Vector<char> bytesOut;
            if (WebCore::base64Decode(result, bytesOut))
                result.swap(bytesOut);
            else
                result.clear();
        }

        notifyOpen(result.isEmpty() ? 404 : 200, 0);
        notifyStringHeaderReceived("Content-Type", contentType);
        notifyStringHeaderReceived("Content-Length", WTF::String::number(result.size()));
        notifyDataReceived(result.data(), result.size());
        notifyDone();
    }

    void handleAbout()
    {
        WTF::String aboutWhat(m_response.url().string().substring(6)); // first 6 chars are "about:"

        WTF::String result;

        bool handled = false;
        if (aboutWhat.isEmpty() || equalIgnoringCase(aboutWhat, "blank")) {
            handled = true;
        } else if (equalIgnoringCase(aboutWhat, "credits")) {
            result.append(WTF::String("<html><head><title>Open Source Credits</title> <style> .about {padding:14px;} </style> <meta name=\"viewport\" content=\"width=device-width, user-scalable=no\"></head><body>"));
            result.append(WTF::String(WebCore::WEBKITCREDITS));
            result.append(WTF::String("</body></html>"));
            handled = true;
#if !defined(PUBLIC_BUILD) || !PUBLIC_BUILD
        } else if (equalIgnoringCase(aboutWhat, "version")) {
            result.append(WTF::String("<html><meta name=\"viewport\" content=\"width=device-width, user-scalable=no\"></head><body>"));
            result.append(WTF::String(WebCore::BUILDTIME));
            result.append(WTF::String("</body></html>"));
            handled = true;
        } else if (Platform::debugSetting() > 0 && equalIgnoringCase(aboutWhat, "config")) {
            result = WebCore::configPage();
            handled = true;
        } else if (Platform::debugSetting() > 0 && equalIgnoringCase(aboutWhat, "build")) {
            result.append(WTF::String("<html><head><title>BlackBerry Browser Build Information</title></head><body><pre>"));
            result.append(WTF::String(WebCore::BUILDINFO));
            result.append(WTF::String(BUILDINFO_PLATFORM));
            result.append(WTF::String(BUILDINFO_LIBWEBVIEW));
            result.append(WTF::String("</pre></body></html>"));
            handled = true;
        } else if (equalIgnoringCase(aboutWhat, "memory")) {
            result = WebCore::memoryPage();
            handled = true;
#endif
        }

        CString resultString = result.utf8();

        notifyOpen(handled ? 404 : 200, 0);
        notifyStringHeaderReceived("Content-Length", WTF::String::number(resultString.length()));
        notifyStringHeaderReceived("Content-Type", "text/html");
        notifyDataReceived(resultString.data(), resultString.length());
        notifyDone();
    }

    // The server needs authentication credentials. Search in the
    // CredentialStorage or prompt the user via dialog.
    bool handleAuthHeader(const WTF::String& header)
    {
        if (!m_handle)
            return false;

        if (!m_handle->getInternal()->m_currentWebChallenge.isNull())
            return false;

        if (header.isEmpty())
            return false;

        if (equalIgnoringCase(header, "ntlm")) {
            sendRequestWithCredentials(WebCore::ProtectionSpaceServerHTTP, WebCore::ProtectionSpaceAuthenticationSchemeNTLM, "NTLM");
        }

        // Extract the auth scheme and realm from the header.
        size_t spacePos = header.find(' ');
        if (spacePos == WTF::notFound) {
            LOG(Network, "WWW-Authenticate field '%s' badly formatted: missing scheme.", header.utf8().data());
            return false;
        }

        WTF::String scheme = header.left(spacePos);

        ProtectionSpaceAuthenticationScheme protectionSpaceScheme = WebCore::ProtectionSpaceAuthenticationSchemeDefault;
        if (equalIgnoringCase(scheme, "basic")) {
            protectionSpaceScheme = WebCore::ProtectionSpaceAuthenticationSchemeHTTPBasic;
        } else if (equalIgnoringCase(scheme, "digest")) {
            protectionSpaceScheme = WebCore::ProtectionSpaceAuthenticationSchemeHTTPDigest;
        } else {
            notImplemented();
            return false;
        }

        size_t realmPos = header.find("realm=", spacePos);
        if (realmPos == WTF::notFound) {
            LOG(Network, "WWW-Authenticate field '%s' badly formatted: missing realm.", header.utf8().data());
            return false;
        }
        size_t beginPos = realmPos + 6;
        WTF::String realm  = header.right(header.length() - beginPos);
        if (realm.startsWith("\"")) {
            beginPos += 1;
            size_t endPos = header.find("\"", beginPos);
            if (endPos == WTF::notFound) {
                LOG(Network, "WWW-Authenticate field '%s' badly formatted: invalid realm.", header.utf8().data());
                return false;
            }
            realm = header.substring(beginPos, endPos - beginPos);
        }

        // Get the user's credentials and resend the request
        sendRequestWithCredentials(WebCore::ProtectionSpaceServerHTTP, protectionSpaceScheme, realm);

        return true;
    }

    bool handleFTPHeader(const WTF::String& header)
    {
        size_t spacePos = header.find(' ');
        if (spacePos == WTF::notFound)
            return false;
        WTF::String statusCode = header.left(spacePos);
        switch (statusCode.toInt()) {
        case 213:
            m_isFTPDir = false;
            break;
        case 530:
            purgeCredentials();
            sendRequestWithCredentials(WebCore::ProtectionSpaceServerFTP, WebCore::ProtectionSpaceAuthenticationSchemeDefault, "ftp");
            break;
        case 230:
            storeCredentials();
            break;
        }

        return true;
    }

    bool sendRequestWithCredentials(ProtectionSpaceServerType type, ProtectionSpaceAuthenticationScheme scheme, WTF::String realm)
    {
        ASSERT(m_handle);
        if (!m_handle)
            return false;

        KURL newURL = m_response.url();
        if (!newURL.isValid())
            return false;

        ProtectionSpace protectionSpace(m_response.url().host(), m_response.url().port(), type, realm, scheme);

        // We've got the scheme and realm. Now we need a username and password.
        // First search the CredentialStorage.
        WebCore::Credential credential = CredentialStorage::get(protectionSpace);
        if (!credential.isEmpty()) {
            m_handle->getInternal()->m_currentWebChallenge = AuthenticationChallenge(protectionSpace, credential, 0, m_response, ResourceError());
            m_handle->getInternal()->m_currentWebChallenge.setStored(true);
        } else {
            // CredentialStore is empty. Ask the user via dialog.
            WTF::String username;
            WTF::String password;

            if (!m_frame || !m_frame->loader() || !m_frame->loader()->client())
                return false;
            m_frame->loader()->client()->authenticationChallenge(realm, username, password);

            if (username.isEmpty() && password.isEmpty())
                return false;

            credential = Credential(username, password, WebCore::CredentialPersistenceForSession);

            m_handle->getInternal()->m_currentWebChallenge = AuthenticationChallenge(protectionSpace, credential, 0, m_response, ResourceError());
        }

        // Resend the resource request. Cloned from handleRedirect(). Not sure
        // if we need everything that follows...
        ResourceRequest newRequest = m_handle->firstRequest();
        newRequest.setURL(newURL);
        newRequest.setMustHandleInternally(true);

        if (clientIsOk()) {
            RecursionGuard guard(m_callingClient);
            m_handle->client()->willSendRequest(m_handle.get(), newRequest, m_response);

            // cancelled can become true if the redirected url fails the policy check
            if (m_cancelled)
                return false;
        }

        // Pass the ownership of the ResourceHandle to the new NetworkJob.
        RefPtr<ResourceHandle> handle = m_handle;
        m_handle = 0;
        NetworkManager::instance()->startJob(m_playerId, m_pageGroupName, handle, newRequest, m_streamFactory, *m_frame, m_deferLoadingCount, m_redirectCount);

        return true;
    }

    void storeCredentials()
    {
        if (!m_handle)
            return;

        AuthenticationChallenge& challenge = m_handle->getInternal()->m_currentWebChallenge;
        if (challenge.isNull())
            return;

        if (challenge.isStored())
            return;

        CredentialStorage::set(challenge.proposedCredential(), challenge.protectionSpace(), m_response.url());
        challenge.setStored(true);
    }

    void purgeCredentials()
    {
        if (!m_handle)
            return;

        AuthenticationChallenge& challenge = m_handle->getInternal()->m_currentWebChallenge;
        if (challenge.isNull())
            return;

        CredentialStorage::remove(challenge.protectionSpace());
        challenge.setStored(false);
    }

private:
    int m_playerId;
    WTF::String m_pageGroupName;
    RefPtr<ResourceHandle> m_handle;
    ResourceResponse m_response;
    Timer<NetworkJob> m_loadDataTimer;
    Timer<NetworkJob> m_loadAboutTimer;
    Timer<NetworkJob> m_deleteJobTimer;
    WTF::String m_contentType;
    WTF::String m_contentDisposition;
    Platform::NetworkStreamFactory* m_streamFactory;
    bool m_isFile;
    bool m_isData;
    bool m_isAbout;
    bool m_isFTP;
    bool m_isFTPDir;
    bool m_isRunning;
    bool m_cancelled;
    bool m_statusReceived;
    bool m_dataReceived;
    bool m_responseSent;

    // if an HTTP status code is received, m_extendedStatusCode and m_response.httpStatusCode will both be set to it
    // if a platform error code is received, m_extendedStatusCode will be set to it and m_response.httpStatusCode will be set to 404
    int m_extendedStatusCode;

    int m_redirectCount;

    DeferredData m_deferredData;
    int m_deferLoadingCount;
    bool m_callingClient;
    const Frame* m_frame;
    bool m_isXHR; // TODO - After 7.0, remove this. Only the Qt port reports HTTP error statuses as didFails, so we probably shouldn't
};


/***
 * DeferredData implementation
 */

DeferredData::DeferredData(NetworkJob& job)
    : m_job(job)
    , m_processDataTimer(this, &DeferredData::fireProcessDataTimer)
    , m_deferredOpen(false)
    , m_status(0)
    , m_deferredWMLOverride(false)
    , m_bytesSent(0)
    , m_totalBytesToBeSent(0)
    , m_deferredDone(false)
    , m_inProcessDeferredData(false)
{
}

void DeferredData::deferOpen(int status, const WTF::String& message)
{
    m_deferredOpen = true;
    m_status = status;
    m_message = message;
}

void DeferredData::deferWMLOverride()
{
    m_deferredWMLOverride = true;
}

void DeferredData::deferHeaderReceived(const WTF::String& key, const WTF::String& value)
{
    m_headerKeys.append(key);
    m_headerValues.append(value);
}

void DeferredData::deferDataSent(unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    m_bytesSent = bytesSent;
    m_totalBytesToBeSent = totalBytesToBeSent;
}

void DeferredData::deferDataReceived(const char* buf, size_t len)
{
    m_dataSegments.append(Vector<char>());
    m_dataSegments.rbegin()->append(buf, len);
}

void DeferredData::deferDone()
{
    m_deferredDone = true;
}

void DeferredData::processDeferredData()
{
    if (m_inProcessDeferredData)
        return;

    RecursionGuard guard(m_inProcessDeferredData);

    // sending data to webkit causes it to be parsed and javascript executed,
    // which might cause it to cancel the job or set defersLoading again.  So
    // to be safe, check this after every step.
    if (m_job.isDeferringLoading() || m_job.isCancelled())
        return;

    if (m_deferredOpen) {
        m_job.handleNotifyOpen(m_status, m_message);
        m_deferredOpen = false;
        if (m_job.isDeferringLoading() || m_job.isCancelled())
            return;
    }

    if (m_deferredWMLOverride) {
        m_job.handleNotifyWMLOverride();
        m_deferredWMLOverride = false;
        if (m_job.isDeferringLoading() || m_job.isCancelled())
            return;
    }

    size_t numHeaders = m_headerKeys.size();
    ASSERT(m_headerValues.size() == numHeaders);
    for (unsigned i = 0; i < numHeaders; ++i) {
        m_job.handleNotifyHeaderReceived(m_headerKeys[i], m_headerValues[i]);

        if (m_job.isDeferringLoading()) {
            // remove all the headers that have already been processed
            m_headerKeys.remove(0, i + 1);
            m_headerValues.remove(0, i + 1);
            return;
        }

        if (m_job.isCancelled()) {
            // don't bother removing headers; job will be deleted
            return;
        }
    }
    m_headerKeys.clear();
    m_headerValues.clear();

    // only process 32k of data at a time to avoid blocking the event loop for too long
    static const unsigned maxData = 32 * 1024;

    unsigned totalData = 0;
    while (!m_dataSegments.isEmpty()) {
        const Vector<char>& buffer = m_dataSegments.first();
        m_job.handleNotifyDataReceived(buffer.data(), buffer.size());
        totalData += buffer.size();

        if (m_job.isCancelled()) {
            // don't bother removing segment; job will be deleted
            return;
        }

        m_dataSegments.removeFirst();

        if (m_job.isDeferringLoading()) {
            // stop processing until deferred loading is turned off again
            return;
        }

        if (totalData >= maxData && !m_dataSegments.isEmpty()) {
            // pause for now, and schedule a timer to continue after running the event loop
            scheduleProcessDeferredData();
            return;
        }
    }

    if (m_totalBytesToBeSent) {
        m_job.handleNotifyDataSent(m_bytesSent, m_totalBytesToBeSent);
        m_bytesSent = 0;
        m_totalBytesToBeSent = 0;
    }

    if (m_deferredDone) {
        m_job.handleNotifyDone();
        m_deferredDone = false;
        if (m_job.isDeferringLoading() || m_job.isCancelled())
            return;
    }
}

void DeferredData::fireProcessDataTimer(Timer<DeferredData>*)
{
    processDeferredData();
}


/***
 * NetworkManager implementation
 */

NetworkManager *NetworkManager::instance()
{
    static NetworkManager *sInstance = NULL;
    if (!sInstance) {
        sInstance = new NetworkManager;
        ASSERT(sInstance);
    }
    return sInstance;
}

bool NetworkManager::startJob(int playerId, PassRefPtr<ResourceHandle> job, const Frame& frame, bool defersLoading)
{
    ASSERT(job.get());
    RefPtr<ResourceHandle> refJob(job); // We shouldn't call methods on PassRefPtr so make a new RefPt
    return startJob(playerId, refJob, refJob->firstRequest(), frame, defersLoading);
}

bool NetworkManager::startJob(int playerId, PassRefPtr<ResourceHandle> job, const ResourceRequest& request, const Frame& frame, bool defersLoading)
{
    Page* page = frame.page();
    if (!page)
        return false;
    Platform::NetworkStreamFactory* streamFactory = page->chrome()->platformPageClient()->networkStreamFactory();
    return startJob(playerId, page->groupName(), job, request, streamFactory, frame, defersLoading ? 1 : 0);
}

bool NetworkManager::startJob(int playerId, const WTF::String& pageGroupName, PassRefPtr<ResourceHandle> job, const ResourceRequest& request, Platform::NetworkStreamFactory* streamFactory, const Frame& frame, int deferLoadingCount, int redirectCount)
{
    // make sure the ResourceHandle doesn't go out of scope while calling callbacks
    RefPtr<ResourceHandle> guardJob(job);

    KURL url = request.url();

    // only load the initial url once
    bool isInitial = (url == m_initialURL);
    if (isInitial)
        m_initialURL = KURL();

    Platform::NetworkRequest platformRequest;
    request.initializePlatformRequest(platformRequest, isInitial);

    // Attach any applicable auth credentials to the NetworkRequest.
    AuthenticationChallenge& challenge = guardJob->getInternal()->m_currentWebChallenge;
    if (!challenge.isNull()) {
        Credential credential = challenge.proposedCredential();
        ProtectionSpace protectionSpace = challenge.protectionSpace();
        ProtectionSpaceServerType type = protectionSpace.serverType();

        WTF::String username = credential.user();
        WTF::String password = credential.password();

        Platform::NetworkRequest::AuthType authType = Platform::NetworkRequest::AuthNone;
        if (type == WebCore::ProtectionSpaceServerHTTP) {
            switch (protectionSpace.authenticationScheme()) {
            case WebCore::ProtectionSpaceAuthenticationSchemeHTTPBasic:
                authType = Platform::NetworkRequest::AuthHTTPBasic;
                break;
            case WebCore::ProtectionSpaceAuthenticationSchemeHTTPDigest:
                authType = Platform::NetworkRequest::AuthHTTPDigest;
                break;
            case WebCore::ProtectionSpaceAuthenticationSchemeNTLM:
                authType = Platform::NetworkRequest::AuthHTTPNTLM;
                break;
            // Lots more cases to handle
            default:
                // defaults to AuthNone as per above.
                break;
            }
        } else if (type == WebCore::ProtectionSpaceServerFTP) {
            authType = Platform::NetworkRequest::AuthFTP;
        }

        if (authType != Platform::NetworkRequest::AuthNone)
            platformRequest.setCredentials(username.utf8().data(), password.utf8().data(), authType);
    }

#if OS(QNX)
    if ((&frame) && (&frame)->loader() && (&frame)->loader()->client()
        && static_cast<WebCore::FrameLoaderClientBlackBerry*>((&frame)->loader()->client())->cookiesEnabled()) {
        // Prepare a cookie header if there are cookies related to this url.
        WTF::String cookiePairs = WebCore::cookieManager().getCookie(url, WebCore::WithHttpOnlyCookies);
        if (!cookiePairs.isEmpty())
            platformRequest.setCookieData(cookiePairs.utf8().data());
    }
#endif

    NetworkJob* networkJob = new NetworkJob;
    if (!networkJob)
        return false;
    if (!networkJob->initialize(playerId, pageGroupName, url, platformRequest, guardJob, streamFactory, frame, deferLoadingCount, redirectCount)) {
        delete networkJob;
        return false;
    }

    // Make sure we have only one NetworkJob for one ResourceHandle
    ASSERT(!findJobForHandle(guardJob));

    m_jobs.append(networkJob);

    // handle data: URL's
    if (url.protocolIs("data")) {
        networkJob->loadDataURL();
        return true;
    }

    // handle about: URL's
    if (url.protocolIs("about")) {
        // try to handle the url internally; if it isn't recognized, continue and pass it to the client
        if (networkJob->loadAboutURL())
            return true;
    }

    // handle all other URL's
    int result = networkJob->open();
    if (result != 0)
        return false;

    return true;
}

bool NetworkManager::stopJob(PassRefPtr<ResourceHandle> job)
{
    if (NetworkJob* networkJob = findJobForHandle(job))
        return networkJob->cancelJob() == 0;

    return false;
}

NetworkJob* NetworkManager::findJobForHandle(PassRefPtr<ResourceHandle> job)
{
    for (unsigned i = 0; i < m_jobs.size(); ++i) {
        NetworkJob* networkJob = m_jobs[i];
        if (networkJob->handle() == job) {
            // We have only one job for one handle.
            return networkJob;
        }
    }
    return 0;
}

void NetworkManager::deleteJob(NetworkJob* job)
{
    ASSERT(!job->isRunning());
    size_t position = m_jobs.find(job);
    if (position != WTF::notFound)
        m_jobs.remove(position);
    delete job;
}

void NetworkManager::setDefersLoading(PassRefPtr<ResourceHandle> job, bool defersLoading)
{
    if (NetworkJob* networkJob = findJobForHandle(job))
        networkJob->updateDeferLoadingCount(defersLoading ? 1 : -1);
}

void NetworkManager::pauseLoad(PassRefPtr<ResourceHandle> job, bool pause)
{
    if (NetworkJob* networkJob = findJobForHandle(job))
        networkJob->pauseLoad(pause);
}

Platform::FilterStream* NetworkManager::streamForHandle(WTF::PassRefPtr<WebCore::ResourceHandle> job)
{
    NetworkJob* networkJob = findJobForHandle(job);

    return networkJob ? networkJob->wrappedStream() : 0;
}

} // System

} // Olympia
