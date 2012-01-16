/*
 * Copyright (C) Research In Motion Limited 2009-2011. All rights reserved.
 */

#include "config.h"
#include "BlackBerryGlobal.h"

#include "ApplicationCacheStorage.h"
#include "ApplicationCacheStorageManager.h"
#include "Cache.h"
#include "CacheClientBlackBerry.h"
#include "CookieManager.h"
#include "Credential.h"
#include "CredentialStorage.h"
#include "CrossOriginPreflightResultCache.h"
#include "DatabaseTracker.h"
#include "DatabaseTrackerManager.h"
#include "GCController.h"
#include "InitializeThreading.h"
#include "ImageSource.h"
#include "KURL.h"
#include "Logging.h"
#include "MainThread.h"
#include "NetworkStateNotifier.h"
#include "ObjectAllocator.h"
#include "BlackBerryCookieCache.h"
#include "BlackBerryPlatformSettings.h"
#include "WebString.h"
#include "OutOfMemoryHandler.h"
#include "PageCache.h"
#include "PageGroup.h"
#include "ProtectionSpace.h"
#include "StorageNamespace.h"

namespace Olympia {
namespace WebKit {

static bool gIsGlobalInitialized = false;

// global initialize of various WebKit singletons and such
void globalInitialize()
{
    if (gIsGlobalInitialized)
        return;
    gIsGlobalInitialized = true;

#if ENABLE(OLYMPIA_DEBUG_MEMORY)
    olympiaDebugInitialize();
#endif

    Olympia::Platform::initializeObjectAllocators();

    Olympia::WebKit::initializeOutOfMemoryHandler();

    // Turn on logging
    WebCore::InitializeLoggingChannelsIfNecessary();

    // Initialize threading
    JSC::initializeThreading();

    // normally this is called from initializeThreading, but we're using ThreadingNone
    // we're grabbing callOnMainThread without using the rest of the threading support
    WTF::initializeMainThread();

    // Track visited links
    WebCore::PageGroup::setShouldTrackVisitedLinks(true);

    WebCore::CacheClientBlackBerry::get()->initialize();

    Olympia::Platform::Settings* settings = Olympia::Platform::Settings::get();

    WebCore::ImageSource::setMaxPixelsPerDecodedImage(settings->maxPixelsPerDecodedImage());
}

void collectJavascriptGarbageNow()
{
    if (gIsGlobalInitialized)
        WebCore::gcController().garbageCollectNow();
}

void clearCookieCache()
{
#if defined(__QNXNTO__)
    WebCore::cookieManager().removeAllCookies(WebCore::RemoveFromBackingStore);
#else
    WebCore::OlympiaCookieCache::instance().clearAllCookies();
#endif
}

void clearMemoryCaches()
{
    int capacity = WebCore::pageCache()->capacity();
    WebCore::pageCache()->setCapacity(0);
    WebCore::pageCache()->setCapacity(capacity);

    WebCore::CrossOriginPreflightResultCache::shared().empty();

#if !ENABLE(OLYMPIA_DEBUG_MEMORY)
    // setDisabled(true) will try to evict all cached resources
    WebCore::cache()->setDisabled(true);
    WebCore::cache()->setDisabled(false);
#endif
}

void clearAppCache(const WebString& pageGroupName)
{
    WTF::String name(pageGroupName.characters(), pageGroupName.length());

    WebCore::cacheStorage(name).empty();
}

void clearLocalStorage(const WebString& pageGroupName)
{
    WTF::String name(pageGroupName.characters(), pageGroupName.length());

    WebCore::PageGroup* group = WebCore::PageGroup::pageGroup(name);
    if (!group)
        return;

    group->removeLocalStorage();
}

void clearDatabase(const WebString& pageGroupName)
{
    WTF::String name(pageGroupName.characters(), pageGroupName.length());

    WebCore::DatabaseTracker::tracker(name).deleteAllDatabases();
}
#if OS(BLACKBERRY)
void closeAllAppCaches()
{
    WebCore::ApplicationCacheStorageManager::closeAll();
}
void reopenAllAppCaches()
{
    WebCore::ApplicationCacheStorageManager::reopenAll();
}

void closeAllLocalStorages()
{
    WebCore::StorageNamespace::closeAllLocalStorageNamespaces();
}
void reopenAllTrackerDatabases()
{
    WebCore::DatabaseTrackerManager::reopenAllTrackerDatabases();
}
void closeAllTrackerDatabases()
{
    WebCore::DatabaseTrackerManager::closeAllTrackerDatabases();
}
#endif

void updateOnlineStatus(bool online)
{
    WebCore::networkStateNotifier().networkStateChange(online);
}

WebString getCookie(const char* url)
{
     return WebCore::cookieManager().getCookie(WebCore::KURL(WebCore::KURL(), url), WebCore::WithHttpOnlyCookies);
}

WebCore::ProtectionSpace getProtectionSpace(const char* url, WebString& realm, WebString& scheme)
{
    WebCore::ProtectionSpaceAuthenticationScheme protectionSpaceScheme = WebCore::ProtectionSpaceAuthenticationSchemeDefault;
    if (equalIgnoringCase(scheme, "ntlm"))
        protectionSpaceScheme = WebCore::ProtectionSpaceAuthenticationSchemeNTLM;
    else if (equalIgnoringCase(scheme, "basic"))
        protectionSpaceScheme = WebCore::ProtectionSpaceAuthenticationSchemeHTTPBasic;
    else if (equalIgnoringCase(scheme, "digest"))
        protectionSpaceScheme = WebCore::ProtectionSpaceAuthenticationSchemeHTTPDigest;
    WebCore::KURL kurl(WebCore::KURL(), url);
    return WebCore::ProtectionSpace(kurl.host(), kurl.port(), realm.equalIgnoringCase("ftp") ?
            WebCore::ProtectionSpaceServerFTP : WebCore::ProtectionSpaceServerHTTP, realm, protectionSpaceScheme);
}

void getCredential(const char* url, WebString& realm, WebString& scheme, WebString& user, WebString& password)
{
    WebCore::Credential cres = WebCore::CredentialStorage::get(getProtectionSpace(url, realm, scheme));
    user = cres.user();
    password = cres.password();
}

void storeCredential(const char* url, WebString& realm, WebString& scheme, WebString& user, WebString& password)
{
    WebCore::Credential credential(user, password, WebCore::CredentialPersistenceForSession);
    WebCore::CredentialStorage::set(credential, getProtectionSpace(url, realm, scheme), WebCore::KURL(WebCore::KURL(), url));
}

void removeCredential(const char* url, WebString& realm, WebString& scheme)
{
    WebCore::CredentialStorage::remove(getProtectionSpace(url, realm, scheme));

}

}
}

