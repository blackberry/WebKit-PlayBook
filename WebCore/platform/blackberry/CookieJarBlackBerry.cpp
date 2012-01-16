/*
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/
 * Copyright (C) Research In Motion Limited 2010-2011. All rights reserved.
 */

#include "config.h"
#include "CookieJar.h"

#include "BlackBerryCookieCache.h"
#if OS(QNX)
#include "Cookie.h"
#include "CookieManager.h"
#endif
#include "CString.h"
#include "Document.h"
#include "Frame.h"
#include "FrameLoaderClientBlackBerry.h"
#include "KURL.h"
#include "NotImplemented.h"
#if OS(BLACKBERRY)
#include "OlympiaPlatformCookieJar.h"
#endif
#include "Page.h"
#include "PageGroupLoadDeferrer.h"
#include "Settings.h"

namespace WebCore {

#if !OS(QNX)
static int getPlayerId(Frame* frame)
{
    return static_cast<FrameLoaderClientBlackBerry*>(frame->loader()->client())->playerId();
}
#endif

static WTF::String cookiesFromCache(const Page* page, const KURL& url)
{
    ASSERT(page);
    if (page->settings()->isCookieCacheEnabled())
        return BlackBerryCookieCache::instance().cookies(page->groupName(), url);
    return WTF::String();
}

static void cacheCookies(const Page* page, const KURL& url, const WTF::String& cookies)
{
    ASSERT(page);
    if (page->settings()->isCookieCacheEnabled())
        BlackBerryCookieCache::instance().setCookies(page->groupName(), url, cookies);
}

static void clearCookieCacheForHost(const Page* page, const KURL& url)
{
    ASSERT(page);
    if (page->settings()->isCookieCacheEnabled())
        BlackBerryCookieCache::instance().clearAllCookiesForHost(page->groupName(), url);
}

WTF::String cookies(Document const* document, KURL const& url)
{
    Frame* frame = document->frame();
    Page* page = frame ? frame->page() : 0;

    if (!page)
        return WTF::String();

    if (!(frame && frame->loader() && frame->loader()->client()))
        return WTF::String();

    if (!static_cast<WebCore::FrameLoaderClientBlackBerry*>(frame->loader()->client())->cookiesEnabled())
        return WTF::String();

    WTF::String cachedCookies = cookiesFromCache(page, url);
    if (!cachedCookies.isEmpty())
        return cachedCookies;

#if OS(QNX)
    ASSERT(document && url == document->cookieURL());
    // 'HttpOnly' cookies should no be accessible from scripts, so we filter them out here
    WTF::String result = cookieManager().getCookie(url, NoHttpOnlyCookie);
#else

    PageGroupLoadDeferrer deferrer(page, true);

    UnsharedArray<char> cookies;
    if (!Olympia::Platform::getCookieString(getPlayerId(frame), url.string().latin1().data(), cookies))
        return WTF::String();

    WTF::String result = WTF::String(cookies.get());
#endif
    if (page)
        cacheCookies(page, url, result);
    return result;
}

void setCookies(Document* document, KURL const& url, String const& value)
{
    Frame* frame = document->frame();
    Page* page = frame ? frame->page() : 0;

    if (!page)
        return;

    if (!(frame && frame->loader() && frame->loader()->client()))
        return;

    if (!static_cast<WebCore::FrameLoaderClientBlackBerry*>(frame->loader()->client())->cookiesEnabled())
        return;

#if OS(QNX)
    ASSERT(document && url == document->cookieURL());
    cookieManager().setCookies(url, value);
    if (page) {
        clearCookieCacheForHost(page, url);
        cacheCookies(page, url, cookieManager().getCookie(url, WithHttpOnlyCookies));
    }
#else

    if (Olympia::Platform::setCookieString(getPlayerId(frame), url.string().latin1().data(), value.latin1().data())) {
        clearCookieCacheForHost(page, url);

        UnsharedArray<char> cookies;
        if (!Olympia::Platform::getCookieString(getPlayerId(frame), url.string().latin1().data(), cookies)) {
            ASSERT_NOT_REACHED();
            return;
        }

        WTF::String result = WTF::String(cookies.get());

        cacheCookies(page, url, result);
    }
#endif
}

bool cookiesEnabled(Document const*)
{
    // FIXME. Currently cookie is enabled by default, no setting on property page.
    return true;
}

bool getRawCookies(const Document* document, const KURL& url, Vector<Cookie>& rawCookies)
{
    notImplemented();
    return false;
}

void deleteCookie(const Document* document, const KURL& url, const WTF::String& cookieName)
{
    notImplemented();
}

WTF::String cookieRequestHeaderFieldValue(const Document* document, const KURL &url)
{
#if OS(QNX)
    ASSERT(document);

    if (!(document->frame() && document->frame()->loader() && document->frame()->loader()->client()))
        return WTF::String();

    if (!static_cast<WebCore::FrameLoaderClientBlackBerry*>(document->frame()->loader()->client())->cookiesEnabled())
        return WTF::String();

    return cookieManager().getCookie(url, WithHttpOnlyCookies);
#else
    notImplemented();
    return WTF::String();
#endif
}

} // namespace WebCore
