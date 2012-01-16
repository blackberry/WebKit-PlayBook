/*
 * Copyright (C) 2008, 2009 Julien Chaffraix <julien.chaffraix@gmail.com>
 * Copyright (C) Research In Motion Limited 2010-2011. All rights reserved.
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
#include "CookieManager.h"

#include "ParsedCookie.h"
#include "CookieBackingStore.h"
#include "CookieParser.h"
#include "CString.h"
#include "CurrentTime.h"
#include "FileSystem.h"
#include "Logging.h"
#include <BlackBerryPlatformClient.h>
#include <string>

namespace WebCore {

CookieManager& cookieManager()
{
    static CookieManager *cookieManager = 0;
    if (!cookieManager) {
        // Open the cookieJar now and get the backing store cookies to fill the manager.
        cookieManager = new CookieManager;
        cookieBackingStore().open(cookieManager->cookieJar());
        cookieManager->getBackingStoreCookies();
    }
    return *cookieManager;
} 

CookieManager::CookieManager()
    : m_count(0)
    , m_privateMode(false)
    , m_cookieJarFileName(pathByAppendingComponent(Olympia::Platform::Client::get()->getApplicationDataDirectory().c_str(), "/cookieCollection.db"))
    , m_policy(CookieStorageAcceptPolicyAlways)
{
}

CookieManager::~CookieManager()
{
    removeAllCookies(DoNotRemoveFromBackingStore);
    cookieBackingStore().close();
}

void CookieManager::setCookies(const KURL& url, const String& value)
{
    CookieParser parser(url);
    Vector<ParsedCookie*> cookies = parser.parse(value);

    for (size_t i = 0; i < cookies.size(); ++i) {
        ParsedCookie* cookie = cookies[i];
        if (!shouldRejectForSecurityReason(cookie, url))
            checkAndTreatCookie(cookie);
        else
            delete cookie;
    }
}

bool CookieManager::shouldRejectForSecurityReason(const ParsedCookie* cookie, const KURL& url)
{
    // We have to disable the following check because sites like Facebook and
    // Gmail currently do not follow the spec.
#if 0
    // Check if path attribute is a prefix of the request URI.
    if (!url.path().startsWith(cookie->path())) {
        LOG_ERROR("Cookie %s is rejected because its path does not math the URL %s\n", cookie->toString().utf8().data(), url.string().utf8().data());
        return true;
    }
#endif

    if (!cookie->hasDefaultDomain()) {
        // Check if the domain starts with a dot.
        // FIXME: This check is in fact disabled because the CookieParser prefix the domain if needed and should be removed.
        if (cookie->domain()[0] != '.') {
            LOG_ERROR("Cookie %s is rejected because its domain does not start with a dot.\n", cookie->toString().utf8().data());
            return true;
        }

        // Check if the domain contains an embedded dot.
        int dotPosition = cookie->domain().find(".", 1);
        if (dotPosition == -1 || static_cast<unsigned int>(dotPosition) == cookie->domain().length()) {
            LOG_ERROR("Cookie %s is rejected because its domain does not contain an embedded dot.\n", cookie->toString().utf8().data());
            return true;
        }
    }

    // The request host should domain match the Domain attribute.
    int diffPos = url.host().endsWith(cookie->domain());
    if (diffPos == -1) {
        LOG_ERROR("Cookie %s is rejected because its domain does not domain match the URL %s\n", cookie->toString().utf8().data(), url.string().utf8().data());
        return true;
    }
    // We should check for an embedded dot in the portion of string in the host not in the domain
    // but to match firefox behaviour we do not.

    return false;
}

String CookieManager::getCookie(const KURL& url, HttpOnlyCookieFiltering filter)
{
    bool isConnectionSecure = false;
    static String httpsPrefix("https:");
    if (url.string().startsWith(httpsPrefix, false))
        isConnectionSecure = true;

    // The max size is the number of cookie per host multiplied by the maximum length of a cookie. We add 1 for the final '\0'.
    static const size_t cookiesMaxLength = 2 * (3 + s_maxCookieLength) * s_maxCookieCountPerHost + 1;
    Vector<UChar> cookiePairs;
    cookiePairs.reserveInitialCapacity(cookiesMaxLength);
    for (HashMap<String, CookieMap*>::iterator it = m_managerMap.begin(); it != m_managerMap.end(); ++it) {

        // Handle sub-domain by only looking at the end of the host.
        if (it->first.endsWith(url.host()) || (it->first.startsWith(".", false) && ("." + url.host()).endsWith(it->first, false))) {
            // Get CookieMap to check for expired cookies.
            Vector<ParsedCookie*> cookies = it->second->getCookies();

            if (cookiePairs.size() > 0 && !cookies.isEmpty())
                append(cookiePairs, "; ");
            for (size_t i = 0; i < cookies.size(); ++i) {
                ParsedCookie* cookie = cookies[i];
                // Get the cookies filtering out the secure cookies on an unsecure connection and HttpOnly cookies if requested.
                if (url.path().startsWith(cookie->path(), false) && (isConnectionSecure || !cookie->isSecure()) && (filter == WithHttpOnlyCookies || !cookie->isHttpOnly())) {
                    String nameValuePair = cookie->toNameValuePair();
                    append(cookiePairs, nameValuePair);
                    if (i != cookies.size() - 1)
                        append(cookiePairs, "; ");
                }
            }
        }
    }
    // Per construction of our cookies, we should not grow our vector.
    ASSERT(cookiePairs.capacity() == cookiesMaxLength);

    // Append the final '\0'.
    static const String nullTerminator("\0");
    append(cookiePairs, nullTerminator);
    return String::adopt(cookiePairs);
}

#if PLATFORM(BLACKBERRY) && OS(QNX)
void CookieManager::cookiesGet(Vector<ParsedCookie> &cookies, const KURL& url) const
{
    bool isConnectionSecure = false;
    static String httpsPrefix("https:");
    if (url.string().startsWith(httpsPrefix, false)) {
        isConnectionSecure = true;
    }

    for (HashMap<String, CookieMap*>::const_iterator it = m_managerMap.begin(); it != m_managerMap.end(); ++it) {

        // Handle sub-domain by only looking at the end of the host.
        if (it->first.endsWith(url.host()) || (it->first.startsWith(".", false) && ("." + url.host()).endsWith(it->first, false))) {
            // Get CookieMap to check for expired cookies.
            Vector<ParsedCookie*> allCookies = it->second->getCookies();

            for (size_t i = 0; i < allCookies.size(); ++i) {
                ParsedCookie* cookie = allCookies[i];
                // Get the cookies filtering out the secure cookies on an unsecure connection.
                if (url.path().startsWith(cookie->path(), false) && (isConnectionSecure || !cookie->isSecure())) {
                    cookies.append(*cookie);
                }
            }
        }
    }
}
#endif

void CookieManager::removeAllCookies(BackingStoreRemoval backingStoreRemoval)
{
    HashMap<String, CookieMap*>::iterator first = m_managerMap.begin();
    HashMap<String, CookieMap*>::iterator end = m_managerMap.end();
    for (HashMap<String, CookieMap*>::iterator it = first; it != end; ++it)
        delete it->second;

    m_managerMap.clear();

    if (backingStoreRemoval == RemoveFromBackingStore)
        cookieBackingStore().removeAll();
    m_count = 0;
}

void CookieManager::setCookieJar(const char* fileName)
{
    m_cookieJarFileName = String(fileName);
    cookieBackingStore().open(m_cookieJarFileName);
}

void CookieManager::checkAndTreatCookie(ParsedCookie* cookie)
{
    ASSERT(cookie->domain().length());

    CookieMap* curMap = m_managerMap.get(cookie->domain());

    // Check for cookie to remove in case it is not a session cookie and it has expired.
    if (cookie->hasExpired()) {
        // The cookie has expired so check we have a valid HashMap so try delete it.
        if (curMap) {
            // Check if we have a cookie to remove and update information accordingly.
            bool cookieAlreadyExists = curMap->exists(cookie);
            if (cookieAlreadyExists) {
                curMap->remove(cookie);
                if (!m_privateMode)
                    cookieBackingStore().remove(cookie);
                delete cookie;
            }
        }
    } else {
        if (!curMap) {
            curMap = new CookieMap();
            m_managerMap.add(cookie->domain(), curMap);
            addCookieToMap(curMap, cookie);
        } else {
            // Check if there is a previous cookie.
            bool cookieAlreadyExists = curMap->exists(cookie);

            if (cookieAlreadyExists)
                update(curMap, cookie);
            else
                addCookieToMap(curMap, cookie);
        }
    }
}

void CookieManager::addCookieToMap(CookieMap* map, ParsedCookie* cookie)
{
    // Check if we do not have reached the cookie's threshold.
    if (map->count() > s_maxCookieCountPerHost)
        map->removeOldestCookie();
    else if (m_count >= s_globalMaxCookieCount) {
        if (map->count())
            map->removeOldestCookie();
        else {
#ifndef NDEBUG
            bool hasRemovedCookie = false;
#endif
            // We are above the limit and our map is empty, let's find another cookie to remove.
            for (HashMap<String, CookieMap*>::iterator it = m_managerMap.begin(); it != m_managerMap.end(); ++it) {
                if (it->second->count()) {
                    it->second->removeOldestCookie();
#ifndef NDEBUG
                    hasRemovedCookie = true;
#endif
                    break;
                }
            }
            ASSERT(hasRemovedCookie);
        }
    }

    map->add(cookie);

    // Only add non session cookie to the backing store.
    if (!cookie->isSession() && !m_privateMode)
        cookieBackingStore().insert(cookie);
}

void CookieManager::update(CookieMap* map, ParsedCookie* newCookie)
{
    ASSERT(map->exists(newCookie));
    map->remove(newCookie);
    map->add(newCookie);
    if (!m_privateMode)
        cookieBackingStore().update(newCookie);
}

void CookieManager::getBackingStoreCookies()
{
    // This method should be called just after having created the cookieManager
    // NEVER afterwards!
    ASSERT(!m_count);

    Vector<ParsedCookie*> cookies = cookieBackingStore().getAllCookies();
    for (size_t i = 0; i < cookies.size(); ++i) {
        ParsedCookie* newCookie = cookies[i];

        if (newCookie->hasExpired())
            delete newCookie;
        else {
            CookieMap* curMap = m_managerMap.get(newCookie->domain());
            if (!curMap) {
                curMap = new CookieMap();
                m_managerMap.add(newCookie->domain(), curMap);
            }
            // Use the straightforward add
            curMap->add(newCookie);
        }
    }
}

void CookieManager::setPrivateMode(const bool mode)
{
    m_privateMode = mode;
    if (!mode){
        getBackingStoreCookies();
    }
}

} // namespace WebCore
