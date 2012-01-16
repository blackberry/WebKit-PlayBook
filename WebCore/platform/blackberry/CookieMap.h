/*
 * Copyright (C) Julien Chaffraix <julien.chaffraix@gmail.com>
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

#ifndef CookieMap_h
#define CookieMap_h

#include "config.h"

#include "HashMap.h"
#include "StringHash.h"
#include "PlatformString.h"

namespace WebCore {

    class ParsedCookie;

    /**
     * A cookie map is a container for cookies that matches a common host.
     * The number of cookie per host is limited by CookieManager::s_maxCookieCountPerHost
     */
    class CookieMap {

    public:
        CookieMap();
        ~CookieMap();

        unsigned int count() const { return m_cookieMap.size(); }

        void add(ParsedCookie* cookie);

        /**
         * Remove a cookie from the map.
         * Note the deletion is based on a key search not using the provided pointer.
         */
        void remove(const ParsedCookie*);

        /**
         * Check if a cookie matching this cookie exists (used when parsing to detect duplicate entries).
         * Returns whether a cookie matching this one exists in the map.
         */
        bool exists(const ParsedCookie*) const;
 
        Vector<ParsedCookie*> getCookies();

        void removeOldestCookie();

    private:
        void updateOldestCookie();

        // The key is the tuple (name, path)
        // The spec asks to have also domain, which is implied by choosing the CookieMap relevant to the domain
        HashMap<String, ParsedCookie*> m_cookieMap;

        // Store the oldest cookie to speed up LRU checks
        ParsedCookie* m_oldestCookie;

        // FIXME : should have a m_shouldUpdate flag to update the network layer only when the map has changed
    };

} // namespace WebCore

#endif // CookieMap_h
