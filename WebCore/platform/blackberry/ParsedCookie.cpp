/*
 * Copyright (C) 2008, 2009 Julien Chaffraix <julien.chaffraix@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ParsedCookie.h"

#include "CookieManager.h"
#include "CString.h"
#include "CurrentTime.h"
#include "Logging.h"
#include "KURL.h"
#include <curl/curl.h>

namespace WebCore {

ParsedCookie::ParsedCookie(double currentTime)
    : m_hasDefaultDomain(false)
    , m_expiry(0)
    , m_isSecure(false)
    , m_isHttpOnly(false)
    , m_isSession(true)
    , m_lastAccessed(currentTime)
{
}

ParsedCookie::ParsedCookie(const String& name, const String& value, const String& domain, const String& path, double expiry, double lastAccessed, bool isSecure, bool isHttpOnly)
    : m_name(name)
    , m_value(value)
    , m_domain(domain)
    , m_hasDefaultDomain(false)
    , m_path(path)
    , m_expiry(expiry)
    , m_isSecure(isSecure)
    , m_isHttpOnly(isHttpOnly)
    , m_isSession(false)
    , m_lastAccessed(lastAccessed)
{
}

ParsedCookie::~ParsedCookie()
{
}

void ParsedCookie::setExpiry(const String& expiry)
{
    if (expiry.isEmpty())
        return;

    m_isSession = false;

    m_expiry = (double)curl_getdate(expiry.utf8().data(), NULL);

    if (m_expiry == -1) {
        LOG_ERROR("Could not parse date");
        // In this case, consider that the cookie is session only
        m_isSession = true;
    }
}

void ParsedCookie::setMaxAge(const String& maxAge)
{
    bool ok;
    m_expiry = maxAge.toDouble(&ok);
    if (!ok) {
        LOG_ERROR("Could not parse Max-Age : %s", maxAge.ascii().data());
        return;
    }

    m_isSession = false;

    // If maxAge is null, keep the value so that it is discarded later.
    // FIXME: is this necessary?
    if (m_expiry)
        m_expiry += currentTime();
}

void ParsedCookie::setDefaultDomain(const KURL& requestURL)
{
    m_domain = requestURL.host();
    m_hasDefaultDomain = true;
}

bool ParsedCookie::hasExpired() const
{
    // Session cookies do not expires, they will just not be saved to the backing store.
    return !m_isSession && m_expiry < currentTime();
}

bool ParsedCookie::isUnderSizeLimit() const
{
    return m_value.length() <= CookieManager::maxCookieLength() && m_name.length() <= CookieManager::maxCookieLength();
}

String ParsedCookie::toString() const
{
    String cookie = name();
    cookie += " = ";
    cookie += value();
    cookie += "; Domain = ";
    cookie += domain();
    cookie += "; Path = ";
    cookie += path();
    return cookie;
}

String ParsedCookie::toNameValuePair() const
{
    static const String equal("=");

    size_t cookieLength = m_name.length() + m_value.length() + 2;
    Vector<UChar> result;
    result.reserveInitialCapacity(cookieLength);
    append(result, m_name);
    append(result, equal);
    append(result, m_value);

    return String::adopt(result);
}

} // namespace WebCore
