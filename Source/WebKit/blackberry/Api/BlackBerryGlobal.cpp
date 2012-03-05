/*
 * Copyright (C) 2009, 2010, 2011 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "BlackBerryGlobal.h"

#include "ApplicationCacheStorage.h"
#include "CacheClientBlackBerry.h"
#include "CookieManager.h"
#include "Credential.h"
#include "CredentialStorage.h"
#include "CrossOriginPreflightResultCache.h"
#include "ExecutableAllocator.h"
#include "FontCache.h"
#include "GCController.h"
#include "ImageSource.h"
#include "InitializeThreading.h"
#include "JSDOMWindow.h"
#include "JSGlobalData.h"
#include "Logging.h"
#include "MainThread.h"
#include "MemoryCache.h"
#include "NetworkStateNotifier.h"
#include "OutOfMemoryHandler.h"
#include "PageCache.h"
#include "PageGroup.h"
#include "TextureCacheCompositingThread.h"
#include "WebString.h"
#include "bindings/js/GCController.h"
#include "runtime/JSLock.h"
#include <BlackBerryPlatformExecutableMessage.h>
#include <BlackBerryPlatformMessageClient.h>
#include <BlackBerryPlatformSettings.h>

namespace BlackBerry {
namespace WebKit {

static bool gIsGlobalInitialized = false;

// global initialize of various WebKit singletons and such
void globalInitialize()
{
    if (gIsGlobalInitialized)
        return;
    gIsGlobalInitialized = true;

#if ENABLE(BLACKBERRY_DEBUG_MEMORY)
    blackberryDebugInitialize();
#endif

    BlackBerry::WebKit::initializeOutOfMemoryHandler();

    // Turn on logging
    WebCore::initializeLoggingChannelsIfNecessary();

    // Initialize threading
    JSC::initializeThreading();

    // normally this is called from initializeThreading, but we're using ThreadingNone
    // we're grabbing callOnMainThread without using the rest of the threading support
    WTF::initializeMainThread();

    // Track visited links
    WebCore::PageGroup::setShouldTrackVisitedLinks(true);

    WebCore::CacheClientBlackBerry::get()->initialize();

    BlackBerry::Platform::Settings* settings = BlackBerry::Platform::Settings::get();

    WebCore::ImageSource::setMaxPixelsPerDecodedImage(settings->maxPixelsPerDecodedImage());
}

void collectJavascriptGarbageNow()
{
    if (gIsGlobalInitialized)
        WebCore::gcController().garbageCollectNow();
}

void clearCookieCache()
{
    WebCore::cookieManager().removeAllCookies(WebCore::RemoveFromBackingStore);
}

#if USE(ACCELERATED_COMPOSITING)
static void clearMemoryCachesInCompositingThread()
{
    WebCore::textureCacheCompositingThread()->prune(0);
}
#endif

void clearMemoryCaches()
{
#if USE(ACCELERATED_COMPOSITING)
    // Call textureCacheCompositingThread()->prune(0) in UI thread.
    BlackBerry::Platform::userInterfaceThreadMessageClient()->dispatchMessage(BlackBerry::Platform::createFunctionCallMessage(clearMemoryCachesInCompositingThread));
#endif

    {
        JSC::JSLock lock(JSC::SilenceAssertionsOnly);
        // This function also performs a GC.
        JSC::releaseExecutableMemory(*WebCore::JSDOMWindow::commonJSGlobalData());
    }

    // Clean caches after JS garbage collection because JS GC can
    // generate more dead resources.

    int capacity = WebCore::pageCache()->capacity();
    WebCore::pageCache()->setCapacity(0);
    WebCore::pageCache()->setCapacity(capacity);

    WebCore::CrossOriginPreflightResultCache::shared().empty();

    if (!WebCore::memoryCache()->disabled()) {
        // Evict all dead resources and prune live resources.
        WebCore::memoryCache()->setCapacities(0, 0, 0);

        // Update cache capacity based on current memory status.
        WebCore::CacheClientBlackBerry::get()->updateCacheCapacity();
    }

    WebCore::fontCache()->invalidate();
}

void clearAppCache(const WebString& pageGroupName)
{
    WebCore::cacheStorage().empty();
}

void clearLocalStorage(const WebString& pageGroupName)
{
}

void clearDatabase(const WebString& pageGroupName)
{
}

void updateOnlineStatus(bool online)
{
    WebCore::networkStateNotifier().networkStateChange(online);
}

}
}

