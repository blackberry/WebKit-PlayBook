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
#include "CookieMap.h"

#include "CString.h"
#include "CookieBackingStore.h"
#include "CookieManager.h"
#include "CurrentTime.h"
#include "Logging.h"
#include "ParsedCookie.h"

namespace WebCore {

CookieMap::CookieMap()
    : m_oldestCookie(0)
{
}

CookieMap::~CookieMap()
{
    HashMap<String, ParsedCookie*>::iterator first = m_cookieMap.begin();
    HashMap<String, ParsedCookie*>::iterator end = m_cookieMap.end();
    for (HashMap<String, ParsedCookie*>::iterator cookieIterator = first; cookieIterator != end; ++cookieIterator)
        delete cookieIterator->second;
}

bool CookieMap::exists(const ParsedCookie* cookie) const
{
    String key = cookie->name() + cookie->path();
    return m_cookieMap.get(key);
}

void CookieMap::add(ParsedCookie* cookie)
{
    String key = cookie->name() + cookie->path();
    ASSERT(!m_cookieMap.get(key));
    m_cookieMap.add(key, cookie);
    if (!m_oldestCookie || m_oldestCookie->lastAccessed() > cookie->lastAccessed())
        m_oldestCookie = cookie;
    cookieManager().addedCookie();
}

void CookieMap::remove(const ParsedCookie* cookie)
{
    String key = cookie->name() + cookie->path();

    // Find a previous entry for deletion
    ParsedCookie* prevCookie = m_cookieMap.take(key);

    if (prevCookie == m_oldestCookie)
        updateOldestCookie();

    cookieManager().removedCookie();
    ASSERT(prevCookie);
    delete prevCookie;
}

Vector<ParsedCookie*> CookieMap::getCookies()
{
    Vector<ParsedCookie*> cookies;
    Vector<String> expiredCookies;

    double now = currentTime();

    HashMap<String, ParsedCookie*>::iterator first = m_cookieMap.begin();
    HashMap<String, ParsedCookie*>::iterator end = m_cookieMap.end();
    for (HashMap<String, ParsedCookie*>::iterator cookieIterator = first; cookieIterator != end; ++cookieIterator) {
        ParsedCookie* cookie = cookieIterator->second;
        // Check for non session cookie expired.
        if (cookie->hasExpired()) {
            LOG(Network, "ParsedCookie name: %s value: %s path: %s  expired", cookie->name().ascii().data(), cookie->value().ascii().data(), cookie->path().ascii().data());
            expiredCookies.append(cookieIterator->first);
        } else {
            cookie->setLastAccessed(now);
            cookies.append(cookie);
        }
    }

    for (Vector<String>::iterator it = expiredCookies.begin(); it != expiredCookies.end(); ++it) {
        ParsedCookie* cookie = m_cookieMap.take(*it);
        cookieBackingStore().remove(cookie);
        cookieManager().removedCookie();
        delete cookie;
    }

    if (expiredCookies.size())
        updateOldestCookie();

    return cookies;
}

void CookieMap::removeOldestCookie()
{
    ASSERT(m_oldestCookie);
    if (!m_oldestCookie)
        return;

    remove(m_oldestCookie);

    ASSERT(!m_oldestCookie || count());
}

void CookieMap::updateOldestCookie()
{
    if (m_cookieMap.size() == 0)
        m_oldestCookie = 0;
    else {
        HashMap<String, ParsedCookie*>::iterator it = m_cookieMap.begin();
        m_oldestCookie = it->second;
        ++it;
        for (; it != m_cookieMap.end(); ++it)
            if (m_oldestCookie->lastAccessed() > it->second->lastAccessed())
                m_oldestCookie = it->second;
    }
}

} // namespace WebCore
