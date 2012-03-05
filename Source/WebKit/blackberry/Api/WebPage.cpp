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
#include "WebPage.h"

#include "ApplicationCacheStorage.h"
#include "BackForwardController.h"
#include "BackForwardListImpl.h"
#include "BackingStore.h"
#include "BackingStoreClient.h"
#include "BackingStoreCompositingSurface.h"
#include "BackingStoreTile.h"
#include "BackingStore_p.h"
#include "BlackBerryGlobal.h"
#include "CString.h"
#include "CachedImage.h"
#include "Chrome.h"
#include "ChromeClientBlackBerry.h"
#include "ContextMenuClientBlackBerry.h"
#include "CookieManager.h"
#include "Cursor.h"
#include "DOMSupport.h"
#include "Database.h"
#include "DatabaseSync.h"
#include "DatabaseTracker.h"
#include "DeviceMotionClientBlackBerry.h"
#include "DeviceOrientationClientBlackBerry.h"
#include "DragClientBlackBerry.h"
// FIXME: We should be using DumpRenderTreeClient, but I'm not sure where we should
// create the DRT_BB object. See PR #120355.
#if ENABLE_DRT
#include "DumpRenderTreeBlackBerry.h"
#endif
#include "EditorClientBlackBerry.h"
#include "FatFingers.h"
#include "FloatPoint.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameLoaderClientBlackBerry.h"
#include "FrameView.h"
#if ENABLE(CLIENT_BASED_GEOLOCATION)
#if ENABLE_DRT
#include "GeolocationClientMock.h"
#endif
#include "GeolocationControllerClientBlackBerry.h"
#endif
#include "GroupSettings.h"
#include "HTMLAreaElement.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "HTTPParsers.h"
#include "HistoryItem.h"
#include "HitTestResult.h"
#include "IconDatabaseClientBlackBerry.h"
#include "InRegionScroller.h"
#include "InputHandler.h"
#include "InspectorBackendDispatcher.h"
#include "InspectorClientBlackBerry.h"
#include "InspectorController.h"
#include "IntPoint.h"
#include "IntRect.h"
#include "IntSize.h"
#include "JavaScriptCore/APICast.h"
#include "JavaScriptCore/JSContextRef.h"
#include "JavaScriptDebuggerBlackBerry.h"
#include "NetworkManager.h"
#include "Node.h"
#include "NodeRenderStyle.h"
#include "Page.h"
#include "PageCache.h"
#include "PageGroup.h"
#include "PlatformString.h"
#include "PlatformTouchEvent.h"
#include "PlatformWheelEvent.h"
#include "PluginDatabase.h"
#include "PluginView.h"
#include "RenderLayer.h"
#include "RenderObject.h"
#include "RenderText.h"
#include "RenderThemeBlackBerry.h"
#include "RenderTreeAsText.h"
#include "RenderView.h"
#include "RenderWidget.h"
#include "ResourceHolderImpl.h"
#include "ScriptSourceCode.h"
#include "ScriptValue.h"
#include "ScrollTypes.h"
#include "SelectionHandler.h"
#include "Settings.h"
#include "Storage.h"
#include "StorageNamespace.h"
#include "SurfacePool.h"
#include "Text.h"
#include "ThreadCheck.h"
#include "TouchEventHandler.h"
#include "TransformationMatrix.h"
#include "VisiblePosition.h"
#if ENABLE(WEBDOM)
#include "WebDOMDocument.h"
#endif
#include "WebPageClient.h"
#include "WebPage_p.h"
#include "WebSocket.h"
#include "WebStringImpl.h"
#include "npapi.h"
#include "runtime_root.h"

#if ENABLE(VIDEO)
#include "HTMLMediaElement.h"
#include "MediaPlayer.h"
#include "MediaPlayerPrivateBlackBerry.h"
#endif

#if USE(OPENVG)
#include "EGLDisplayOpenVG.h"
#include "PainterOpenVG.h"
#include "SurfaceOpenVG.h"
#elif USE(SKIA)
#include "PlatformContextSkia.h"
#endif

#if USE(ACCELERATED_COMPOSITING)
#include "FrameLayers.h"
#include "WebPageCompositor.h"
#endif

#include <BlackBerryPlatformExecutableMessage.h>
#include <BlackBerryPlatformITPolicy.h>
#include <BlackBerryPlatformInputEvents.h>
#include <BlackBerryPlatformKeyboardEvent.h>
#include <BlackBerryPlatformMessage.h>
#include <BlackBerryPlatformMessageClient.h>
#include <BlackBerryPlatformMouseEvent.h>
#include <BlackBerryPlatformPrimitives.h>
#include <BlackBerryPlatformScreen.h>
#include <BlackBerryPlatformSettings.h>
#include <BlackBerryPlatformTouchEvent.h>
#include <SharedPointer.h>
#include <sys/keycodes.h>
#include <vector>
#include <wtf/MathExtras.h>
#include <unicode/ustring.h> // platform ICU

#ifndef USER_PROCESSES
#include <memalloc.h>
#else
#if USE(OPENVG) && !defined(_MSC_VER)
#include <i_egl.h>
#endif
#endif

#if ENABLE(SKIA_GPU_CANVAS)
#include "BlackBerryPlatformGraphics.h"
#include "GrContext.h"
#endif

#define DEBUG_BLOCK_ZOOM 0
#define DEBUG_TOUCH_EVENTS 0
#define DEBUG_WEBPAGE_LOAD 0

using namespace std;
using namespace WebCore;

typedef const unsigned short* CUShortPtr;

namespace BlackBerry {
namespace WebKit {

Vector<WebPage*>* WebPagePrivate::s_visibleWebPages = 0; // Initially, no web page is visible.

static Vector<WebPage*>* visibleWebPages()
{
    if (!WebPagePrivate::s_visibleWebPages)
        WebPagePrivate::s_visibleWebPages = new Vector<WebPage*>;
    return WebPagePrivate::s_visibleWebPages;
}

const unsigned blockZoomMargin = 3; // Add 3 pixel margin on each side.
static int blockClickRadius = 0;
static double maximumBlockZoomScale = 3.0; // This scale can be clamped by the maximumScale set for the page.

const double manualScrollInterval = 0.1; // The time interval during which we associate user action with scrolling.

const double delayedZoomInterval = 0.0;

const IntSize minimumLayoutSize(10, 10); // Needs to be a small size, greater than 0, that we can grow the layout from.
const IntSize maximumLayoutSize(10000, 10000); // Used with viewport meta tag, but we can still grow from this of course.

// Helper function to parse a URL and fill in missing parts.
static KURL parseUrl(const WTF::String& url)
{
    WTF::String urlString(url);
    KURL kurl = KURL(KURL(), urlString);
    if (kurl.protocol().isEmpty()) {
        urlString.insert("http://", 0);
        kurl = KURL(KURL(), urlString);
    }

    return kurl;
}

// Helper functions to convert to and from WebCore types.
static inline WebCore::MouseEventType toWebCoreMouseEventType(const BlackBerry::Platform::MouseEvent::Type type)
{
    switch (type) {
    case BlackBerry::Platform::MouseEvent::MouseButtonDown:
        return WebCore::MouseEventPressed;
    case BlackBerry::Platform::MouseEvent::MouseButtonUp:
        return WebCore::MouseEventReleased;
    case BlackBerry::Platform::MouseEvent::MouseMove:
    default:
        return WebCore::MouseEventMoved;
    }
}

static inline WebCore::ResourceRequestCachePolicy toWebCoreCachePolicy(Platform::NetworkRequest::CachePolicy policy)
{
    switch (policy) {
    case Platform::NetworkRequest::UseProtocolCachePolicy:
        return WebCore::UseProtocolCachePolicy;
    case Platform::NetworkRequest::ReloadIgnoringCacheData:
        return WebCore::ReloadIgnoringCacheData;
    case Platform::NetworkRequest::ReturnCacheDataElseLoad:
        return WebCore::ReturnCacheDataElseLoad;
    case Platform::NetworkRequest::ReturnCacheDataDontLoad:
        return WebCore::ReturnCacheDataDontLoad;
    default:
        ASSERT_NOT_REACHED();
        return WebCore::UseProtocolCachePolicy;
    }
}

#if ENABLE(EVENT_MODE_METATAGS)
static inline BlackBerry::Platform::CursorEventMode toPlatformCursorEventMode(WebCore::CursorEventMode mode)
{
    switch (mode) {
    case WebCore::ProcessedCursorEvents:
        return BlackBerry::Platform::ProcessedCursorEvents;
    case WebCore::NativeCursorEvents:
        return BlackBerry::Platform::NativeCursorEvents;
    default:
        ASSERT_NOT_REACHED();
        return BlackBerry::Platform::ProcessedCursorEvents;
    }
}

static inline BlackBerry::Platform::TouchEventMode toPlatformTouchEventMode(WebCore::TouchEventMode mode)
{
    switch (mode) {
    case WebCore::ProcessedTouchEvents:
        return BlackBerry::Platform::ProcessedTouchEvents;
    case WebCore::NativeTouchEvents:
        return BlackBerry::Platform::NativeTouchEvents;
    case WebCore::PureTouchEventsWithMouseConversion:
        return BlackBerry::Platform::PureTouchEventsWithMouseConversion;
    default:
        ASSERT_NOT_REACHED();
        return BlackBerry::Platform::ProcessedTouchEvents;
    }
}
#endif

static inline HistoryItem* historyItemFromBackForwardId(WebPage::BackForwardId id)
{
    return reinterpret_cast<HistoryItem*>(id);
}

static inline WebPage::BackForwardId backForwardIdFromHistoryItem(HistoryItem* item)
{
    return reinterpret_cast<WebPage::BackForwardId>(item);
}

WebPagePrivate::WebPagePrivate(WebPage* webPage, WebPageClient* client, const Platform::IntRect& rect)
    : m_webPage(webPage)
    , m_client(client)
    , m_page(0) // Initialized by init.
    , m_mainFrame(0) // Initialized by init.
    , m_currentContextNode(0)
    , m_webSettings(0) // Initialized by init.
    , m_visible(false)
    , m_shouldResetTilesWhenShown(false)
    , m_userScalable(true)
    , m_userPerformedManualZoom(false)
    , m_userPerformedManualScroll(false)
    , m_contentsSizeChanged(false)
    , m_overflowExceedsContentsSize(false)
    , m_resetVirtualViewportOnCommitted(true)
    , m_shouldUseFixedDesktopMode(false)
    , m_needTouchEvents(false)
    , m_preventIdleDimmingCount(0)
#if ENABLE(TOUCH_EVENTS)
    , m_preventDefaultOnTouchStart(false)
#endif
    , m_nestedLayoutFinishedCount(0)
    , m_actualVisibleWidth(rect.width())
    , m_actualVisibleHeight(rect.height())
    , m_virtualViewportWidth(0)
    , m_virtualViewportHeight(0)
    , m_defaultLayoutSize(minimumLayoutSize)
    , m_didRestoreFromPageCache(false)
    , m_viewMode(WebPagePrivate::Desktop) // Default to Desktop mode for PB.
    , m_loadState(WebPagePrivate::None)
    , m_transformationMatrix(new WebCore::TransformationMatrix())
    , m_backingStore(0) // Initialized by init.
    , m_backingStoreClient(0) // Initialized by init.
    , m_inputHandler(new InputHandler(this))
    , m_selectionHandler(new SelectionHandler(this))
    , m_touchEventHandler(new TouchEventHandler(this))
#if ENABLE(EVENT_MODE_METATAGS)
    , m_cursorEventMode(WebCore::ProcessedCursorEvents)
    , m_touchEventMode(WebCore::ProcessedTouchEvents)
#endif
    , m_currentCursor(BlackBerry::Platform::CursorNone)
    , m_dumpRenderTree(0) // Lazy initialization.
    , m_initialScale(-1.0)
    , m_minimumScale(-1.0)
    , m_maximumScale(-1.0)
    , m_blockZoomFinalScale(1.0)
    , m_anchorInNodeRectRatio(-1, -1)
    , m_currentBlockZoomNode(0)
    , m_currentBlockZoomAdjustedNode(0)
    , m_shouldReflowBlock(false)
    , m_delayedZoomTimer(adoptPtr(new Timer<WebPagePrivate>(this, &WebPagePrivate::zoomAboutPointTimerFired)))
    , m_lastUserEventTimestamp(0.0)
    , m_pluginMouseButtonPressed(false)
    , m_pluginMayOpenNewTab(false)
    , m_geolocationClient(0)
    , m_inRegionScrollStartingNode(0)
#if USE(ACCELERATED_COMPOSITING)
    , m_isAcceleratedCompositingActive(false)
    , m_rootLayerCommitTimer(adoptPtr(new Timer<WebPagePrivate>(this, &WebPagePrivate::rootLayerCommitTimerFired)))
    , m_needsOneShotDrawingSynchronization(false)
    , m_needsCommit(false)
    , m_suspendRootLayerCommit(false)
#endif
    , m_pendingOrientation(-1)
    , m_fullscreenVideoNode(0)
    , m_hasInRegionScrollableAreas(false)
    , m_updateDelegatedOverlaysDispatched(false)
{
}

WebPagePrivate::~WebPagePrivate()
{
    // Hand the backingstore back to another owner if necessary.
    m_webPage->setVisible(false);
    if (BackingStorePrivate::currentBackingStoreOwner() == m_webPage)
        BackingStorePrivate::setCurrentBackingStoreOwner(0);

    delete m_webSettings;
    m_webSettings = 0;

    delete m_backingStoreClient;
    m_backingStoreClient = 0;
    m_backingStore = 0;

    delete m_page;
    m_page = 0;

    delete m_transformationMatrix;
    m_transformationMatrix = 0;

    delete m_selectionHandler;
    m_selectionHandler = 0;

    delete m_inputHandler;
    m_inputHandler = 0;

    delete m_touchEventHandler;
    m_touchEventHandler = 0;

#if ENABLE_DRT
    delete m_dumpRenderTree;
    m_dumpRenderTree = 0;
#endif

}

void WebPagePrivate::init(const WebString& pageGroupName)
{
    ChromeClientBlackBerry* chromeClient = new ChromeClientBlackBerry(m_webPage);
    ContextMenuClientBlackBerry* contextMenuClient = 0;
#if ENABLE(CONTEXT_MENUS)
    contextMenuClient = new ContextMenuClientBlackBerry();
#endif
    EditorClientBlackBerry* editorClient = new EditorClientBlackBerry(m_webPage);
    DragClientBlackBerry* dragClient = 0;
#if ENABLE(DRAG_SUPPORT)
    dragClient = new DragClientBlackBerry();
#endif
    InspectorClientBlackBerry* inspectorClient = 0;
#if ENABLE(INSPECTOR)
    inspectorClient = new InspectorClientBlackBerry(m_webPage);
#endif

    FrameLoaderClientBlackBerry* frameLoaderClient = new FrameLoaderClientBlackBerry();

    Page::PageClients pageClients;
    pageClients.chromeClient = chromeClient;
    pageClients.contextMenuClient = contextMenuClient;
    pageClients.editorClient = editorClient;
    pageClients.dragClient = dragClient;
    pageClients.inspectorClient = inspectorClient;

#if ENABLE(CLIENT_BASED_GEOLOCATION)
    // Note the object will be destroyed when the page is destroyed.
#if ENABLE_DRT
    if (getenv("drtRun"))
        pageClients.geolocationClient = new GeolocationClientMock();
    else
#endif
        pageClients.geolocationClient = m_geolocationClient = new GeolocationControllerClientBlackBerry(m_webPage);
#else
    pageClients.geolocationClient = m_geolocationClient;
#endif

    pageClients.deviceMotionClient = new DeviceMotionClientBlackBerry(m_webPage);
    pageClients.deviceOrientationClient = new DeviceOrientationClientBlackBerry(m_webPage);
    m_page = new Page(pageClients);

#if ENABLE(CLIENT_BASED_GEOLOCATION) && ENABLE_DRT
    // In case running in DumpRenderTree mode set the controller to mock provider.
    if (getenv("drtRun"))
        static_cast<GeolocationClientMock*>(pageClients.geolocationClient)->setController(m_page->geolocationController());
#endif

    m_page->setCustomHTMLTokenizerChunkSize(256);
    m_page->setCustomHTMLTokenizerTimeDelay(0.300);

    m_webSettings = WebSettings::createFromStandardSettings();

    // FIXME: We explicitly call setDelegate() instead of passing ourself in createFromStandardSettings()
    // so that we only get one didChangeSettings() callback when we set the page group name. This causes us
    // to make a copy of the WebSettings since some WebSettings method make use of the page group name.
    // Instead, we shouldn't be storing the page group name in WebSettings.
    m_webSettings->setDelegate(this);
    m_webSettings->setPageGroupName(pageGroupName);

    RefPtr<Frame> newFrame = Frame::create(m_page, /*HTMLFrameOwnerElement**/0, frameLoaderClient);

    m_mainFrame = newFrame.get();
    frameLoaderClient->setFrame(m_mainFrame, m_webPage);
    m_mainFrame->init();

#if ENABLE(WEBGL)
    BlackBerry::Platform::Settings* settings = BlackBerry::Platform::Settings::get();
    m_page->settings()->setWebGLEnabled(settings && settings->isWebGLSupported());
#endif
#if ENABLE(SKIA_GPU_CANVAS)
    m_page->settings()->setCanvasUsesAcceleratedDrawing(true);
    m_page->settings()->setAccelerated2dCanvasEnabled(true);
#endif
#if ENABLE(VIEWPORT_REFLOW)
    m_page->settings()->setTextReflowEnabled(m_webSettings->textReflowMode() == WebSettings::TextReflowEnabled);
#endif

    m_page->settings()->setUseHixie76WebSocketProtocol(false);
    m_page->settings()->setInteractiveFormValidationEnabled(true);
    m_page->settings()->setAllowUniversalAccessFromFileURLs(false);

    m_backingStoreClient = BackingStoreClient::create(m_mainFrame, /*parent frame*/0, m_webPage);
    // The direct access to BackingStore is left here for convenience since it
    // is owned by BackingStoreClient and then deleted by its destructor.
    m_backingStore = m_backingStoreClient->backingStore();

    m_page->settings()->setSpatialNavigationEnabled(m_webSettings->isSpatialNavigationEnabled());
    blockClickRadius = int(roundf(0.35 * BlackBerry::Platform::Graphics::Screen::pixelsPerInch(0).width())); // The clicked rectangle area should be a fixed unit of measurement.

    m_page->settings()->setDelegateSelectionPaint(true);

}

void WebPagePrivate::load(const char* url, const char* networkToken, const char* method, Platform::NetworkRequest::CachePolicy cachePolicy, const char* data, size_t dataLength, const char* const* headers, size_t headersLength, bool isInitial, bool mustHandleInternally, bool forceDownload, const char* overrideContentType)
{
    stopCurrentLoad();

    WTF::String urlString(url);
    if (urlString.startsWith("vs:", false)) {
        urlString = urlString.substring(3);
        m_mainFrame->setInViewSourceMode(true);
    } else
        m_mainFrame->setInViewSourceMode(false);

    KURL kurl = parseUrl(urlString);
    if (protocolIs(kurl, "javascript")) {
        // Never run javascript while loading is deferred.
        if (m_page->defersLoading()) {
            FrameLoaderClientBlackBerry* frameLoaderClient = static_cast<FrameLoaderClientBlackBerry*>(m_mainFrame->loader()->client());
            frameLoaderClient->setDeferredManualScript(kurl);
        } else
            m_mainFrame->script()->executeIfJavaScriptURL(kurl, DoNotReplaceDocumentIfJavaScriptURL);
        return;
    }

    if (isInitial)
        NetworkManager::instance()->setInitialURL(kurl);

    WebCore::ResourceRequest request(kurl, ""/*referrer*/);
    request.setToken(networkToken);
    if (isInitial || mustHandleInternally)
        request.setMustHandleInternally(true);
    request.setHTTPMethod(method);
    request.setCachePolicy(toWebCoreCachePolicy(cachePolicy));
    if (overrideContentType)
        request.setOverrideContentType(overrideContentType);

    if (data)
        request.setHTTPBody(FormData::create(data, dataLength));

    for (unsigned i = 0; i + 1 < headersLength; i += 2)
        request.addHTTPHeaderField(headers[i], headers[i + 1]);

    if (forceDownload)
        request.setForceDownload(true);

    m_mainFrame->loader()->load(request, ""/*name*/, false);
}

void WebPagePrivate::loadString(const char* string, const char* baseURL, const char* contentType)
{
    KURL kurl = parseUrl(baseURL);
    WebCore::ResourceRequest request(kurl);
    WTF::RefPtr<WebCore::SharedBuffer> buffer
        = WebCore::SharedBuffer::create(string, strlen(string));
    WebCore::SubstituteData substituteData(buffer,
                                           extractMIMETypeFromMediaType(contentType),
                                           extractCharsetFromMediaType(contentType),
                                           KURL());
    m_mainFrame->loader()->load(request, substituteData, false);
}

bool WebPagePrivate::executeJavaScript(const char* script, JavaScriptDataType& returnType, WebString& returnValue)
{
    WebCore::ScriptValue result = m_mainFrame->script()->executeScript(WTF::String::fromUTF8(script), false);
    JSC::JSValue value = result.jsValue();
    if (!value) {
        returnType = JSException;
        return false;
    }

    JSC::ExecState* exec = m_mainFrame->script()->globalObject(mainThreadNormalWorld())->globalExec();
    JSGlobalContextRef context = toGlobalRef(exec);

    JSType type = JSValueGetType(context, toRef(exec, value));

    switch (type) {
    case kJSTypeNull:
        returnType = JSNull;
        break;
    case kJSTypeBoolean:
        returnType = JSBoolean;
        break;
    case kJSTypeNumber:
        returnType = JSNumber;
        break;
    case kJSTypeString:
        returnType = JSString;
        break;
    case kJSTypeObject:
        returnType = JSObject;
        break;
    case kJSTypeUndefined:
    default:
        returnType = JSUndefined;
        break;
    }

    if (returnType == JSBoolean || returnType == JSNumber || returnType == JSString || returnType == JSObject) {
        WTF::String str = result.toString(exec);
        returnValue = WebString(reinterpret_cast<WebStringImpl*>(str.impl()));
    }

    return true;
}

bool WebPagePrivate::executeJavaScriptInIsolatedWorld(const WebCore::ScriptSourceCode& sourceCode, JavaScriptDataType& returnType, WebString& returnValue)
{
    if (!m_isolatedWorld)
        m_isolatedWorld = m_mainFrame->script()->createWorld();

    // Use evaluateInWorld to avoid canExecuteScripts check.
    WebCore::ScriptValue result = m_mainFrame->script()->evaluateInWorld(sourceCode, m_isolatedWorld.get());
    JSC::JSValue value = result.jsValue();
    if (!value) {
        returnType = JSException;
        return false;
    }

    JSC::ExecState* exec = m_mainFrame->script()->globalObject(m_isolatedWorld.get())->globalExec();
    JSGlobalContextRef context = toGlobalRef(exec);

    JSType type = JSValueGetType(context, toRef(exec, value));

    switch (type) {
    case kJSTypeNull:
        returnType = JSNull;
        break;
    case kJSTypeBoolean:
        returnType = JSBoolean;
        break;
    case kJSTypeNumber:
        returnType = JSNumber;
        break;
    case kJSTypeString:
        returnType = JSString;
        break;
    case kJSTypeObject:
        returnType = JSObject;
        break;
    case kJSTypeUndefined:
    default:
        returnType = JSUndefined;
        break;
    }

    if (returnType == JSBoolean || returnType == JSNumber || returnType == JSString || returnType == JSObject) {
        WTF::String str = result.toString(exec);
        returnValue = WebString(reinterpret_cast<WebStringImpl*>(str.impl()));
    }

    return true;
}

void WebPagePrivate::stopCurrentLoad()
{
    // This function should contain all common code triggered by WebPage::load
    // (which stops any load in progress before starting the new load) and
    // WebPage::stoploading (the entry point for the client to stop the load
    // explicitly). If it should only be done while stopping the load
    // explicitly, it goes in WebPage::stopLoading, not here.
    m_mainFrame->loader()->stopAllLoaders();

    // Cancel any deferred script that hasn't been processed yet.
    FrameLoaderClientBlackBerry* frameLoaderClient = static_cast<FrameLoaderClientBlackBerry*>(m_mainFrame->loader()->client());
    frameLoaderClient->setDeferredManualScript(KURL());
}

static void closeURLRecursively(Frame* frame)
{
    // Do not create more frame please.
    FrameLoaderClientBlackBerry* frameLoaderClient = static_cast<FrameLoaderClientBlackBerry*>(frame->loader()->client());
    frameLoaderClient->suppressChildFrameCreation();

    frame->loader()->closeURL();

    Vector<RefPtr<Frame>, 10> childFrames;

    for (RefPtr<Frame> childFrame = frame->tree()->firstChild(); childFrame; childFrame = childFrame->tree()->nextSibling())
        childFrames.append(childFrame);

    unsigned size = childFrames.size();
    for (unsigned i = 0; i < size; i++)
        closeURLRecursively(childFrames[i].get());
}

void WebPagePrivate::prepareToDestroy()
{
    // Before the client starts tearing itself down, dispatch the unload event
    // so it can take effect while all the client's state (e.g. scroll position)
    // is still present.
    closeURLRecursively(m_mainFrame);
}

void WebPagePrivate::setLoadState(LoadState state)
{
    if (m_loadState == state)
        return;

    bool isFirstLoad = m_loadState == None;

    // See RIM Bug #1068.
    if (state == Finished && m_mainFrame && m_mainFrame->document())
        m_mainFrame->document()->updateStyleIfNeeded();

    m_loadState = state;

#if DEBUG_WEBPAGE_LOAD
    BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "WebPagePrivate::setLoadState %d", state);
#endif

    switch (m_loadState) {
    case Provisional:
        if (isFirstLoad) {
            // Paints the visible backingstore as white to prevent initial checkerboard on
            // the first blit.
            if (m_backingStore->d->renderVisibleContents() && !m_backingStore->d->isSuspended() && !m_backingStore->d->shouldDirectRenderingToWindow())
                m_backingStore->d->blitVisibleContents();
        }
        break;
    case Committed:
        {
            unscheduleZoomAboutPoint();

#if ENABLE(SKIA_GPU_CANVAS)
            if (m_page->settings()->canvasUsesAcceleratedDrawing()) {
                // Free GPU resources as we're on a new page.
                // This will help us to free memory pressure.
                BlackBerry::Platform::Graphics::makeSharedResourceContextCurrent(BlackBerry::Platform::Graphics::GLES2);
                GrContext* grContext = BlackBerry::Platform::Graphics::getGrContext();
                grContext->freeGpuResources();
            }
#endif

#if USE(ACCELERATED_COMPOSITING)
            // FIXME: compositor may only be touched on the compositing thread.
            // However, it's created/destroyed by a sync command so this is harmless.
            if (m_compositor) {
                m_compositor->setLayoutRectForCompositing(IntRect());
                m_compositor->setContentsSizeForCompositing(IntSize());
            }
#endif
            m_previousContentsSize = IntSize();
            m_backingStore->d->resetRenderQueue();
            m_backingStore->d->resetTiles(true /*resetBackground*/);
            m_backingStore->d->setScrollingOrZooming(false, false /*shouldBlit*/);
            m_userPerformedManualZoom = false;
            m_userPerformedManualScroll = false;
            m_shouldUseFixedDesktopMode = false;
            if (m_resetVirtualViewportOnCommitted) { // For DRT.
                m_virtualViewportWidth = 0;
                m_virtualViewportHeight = 0;
            }
            if (m_webSettings->viewportWidth() > 0) {
                m_virtualViewportWidth = m_webSettings->viewportWidth();
                m_virtualViewportHeight = m_defaultLayoutSize.height();
            }
            // Check if we have already process the meta viewport tag, this only happens on history navigation
            if (!m_didRestoreFromPageCache) {
                m_viewportArguments = ViewportArguments();
                m_userScalable = m_webSettings->isUserScalable();
                resetScales();
            } else {
                IntSize virtualViewport = recomputeVirtualViewportFromViewportArguments();
                m_webPage->setVirtualViewportSize(virtualViewport.width(), virtualViewport.height());
            }

#if ENABLE(EVENT_MODE_METATAGS)
            didReceiveCursorEventMode(WebCore::ProcessedCursorEvents);
            didReceiveTouchEventMode(WebCore::ProcessedTouchEvents);
#endif

            // If it's a outmost SVG document, we use FixedDesktop mode, otherwise
            // we default to Mobile mode. For example, using FixedDesktop mode to
            // render http://www.croczilla.com/bits_and_pieces/svg/samples/tiger/tiger.svg
            // is user-experience friendly.
            if (m_page->mainFrame()->document()->isSVGDocument()) {
                setShouldUseFixedDesktopMode(true);
                setViewMode(FixedDesktop);
            } else
                setViewMode(Mobile);

            // Reset block zoom and reflow.
            resetBlockZoom();
#if ENABLE(VIEWPORT_REFLOW)
            toggleTextReflowIfTextReflowEnabledOnlyForBlockZoom();
#endif

            // Set the scroll to origin here and notify the client since we'll be
            // zooming below without any real contents yet thus the contents size
            // we report to the client could make our current scroll position invalid.
            setScrollPosition(IntPoint(0, 0));
            notifyTransformedScrollChanged();

            // Paints the visible backingstore as white. Note it is important we do
            // this strictly after re-setting the scroll position to origin and resetting
            // the scales otherwise the visible contents calculation is wrong and we
            // can end up blitting artifacts instead. See: RIM Bug #401.
            if (m_backingStore->d->renderVisibleContents() && !m_backingStore->d->isSuspended() && !m_backingStore->d->shouldDirectRenderingToWindow())
                m_backingStore->d->blitVisibleContents();

            zoomToInitialScaleOnLoad();

            // Update cursor status.
            updateCursor();

#if USE(ACCELERATED_COMPOSITING)
            // Don't render compositing contents from previous page.
            resetCompositingSurface();
#endif
            break;
        }
    case Finished:
    case Failed:
        // Notify client of the initial zoom change.
        m_client->zoomChanged(m_webPage->isMinZoomed(), m_webPage->isMaxZoomed(), !shouldZoomOnEscape(), currentScale());
        m_backingStore->d->updateTiles(true /*updateVisible*/, false /*immediate*/);
        break;
    default:
        break;
    }
}

double WebPagePrivate::clampedScale(double scale) const
{
    if (scale < minimumScale())
        return minimumScale();
    if (scale > maximumScale())
        return maximumScale();
    return scale;
}

bool WebPagePrivate::shouldZoomAboutPoint(double scale, const WebCore::FloatPoint&, bool enforceScaleClamping, double* clampedScale)
{
    if (!m_mainFrame->view())
        return false;

    if (enforceScaleClamping)
        scale = this->clampedScale(scale);

    ASSERT(clampedScale);
    *clampedScale = scale;

    if (currentScale() == scale) {
        // Make sure backingstore updates resume from pinch zoom in the case where the final zoom level doesn't change.
        m_backingStore->d->resumeScreenAndBackingStoreUpdates(BackingStore::None);
        m_client->zoomChanged(m_webPage->isMinZoomed(), m_webPage->isMaxZoomed(), !shouldZoomOnEscape(), currentScale());
        return false;
    }

    return true;
}

bool WebPagePrivate::zoomAboutPoint(double unclampedScale, const WebCore::FloatPoint& anchor, bool enforceScaleClamping, bool forceRendering, bool isRestoringZoomLevel)
{
    if (!isRestoringZoomLevel) {
        // Clear any existing block zoom.  (If we are restoring a saved zoom level on page load,
        // there is guaranteed to be no existing block zoom and we don't want to clear m_shouldReflowBlock.)
        resetBlockZoom();
    }

    // The reflow and block zoom stuff here needs to happen regardless of
    // whether we shouldZoomAboutPoint.
#if ENABLE(VIEWPORT_REFLOW)
    toggleTextReflowIfTextReflowEnabledOnlyForBlockZoom(m_shouldReflowBlock);
    if (m_page->settings()->isTextReflowEnabled() && m_mainFrame->view())
        setNeedsLayout();
#endif

    double scale;
    if (!shouldZoomAboutPoint(unclampedScale, anchor, enforceScaleClamping, &scale)) {
        if (m_webPage->settings()->textReflowMode() == WebSettings::TextReflowEnabled) {
            m_currentPinchZoomNode = 0;
            m_anchorInNodeRectRatio = FloatPoint(-1, -1);
        }
        return false;
    }
    TransformationMatrix zoom;
    zoom.scale(scale);

#if DEBUG_WEBPAGE_LOAD
    if (loadState() < Finished)
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "WebPagePrivate::zoomAboutPoint scale %f anchor (%f, %f)", scale, anchor.x(), anchor.y());
#endif

    // Our current scroll position in float.
    FloatPoint scrollPosition = this->scrollPosition();

    // Anchor offset from scroll position in float.
    FloatPoint anchorOffset(anchor.x() - scrollPosition.x(), anchor.y() - scrollPosition.y());

    // The horizontal scaling factor and vertical scaling factor should be equal
    // to preserve aspect ratio of content.
    ASSERT(m_transformationMatrix->m11() == m_transformationMatrix->m22());

    // Need to invert the previous transform to anchor the viewport.
    double inverseScale = scale / m_transformationMatrix->m11();

    // Actual zoom.
    *m_transformationMatrix = zoom;

    // Suspend all screen updates to the backingstore.
    m_backingStore->d->suspendScreenAndBackingStoreUpdates();

    updateViewportSize();

    IntPoint newScrollPosition(IntPoint(max(0, static_cast<int>(roundf(anchor.x() - anchorOffset.x() / inverseScale))),
                                        max(0, static_cast<int>(roundf(anchor.y() - anchorOffset.y() / inverseScale)))));

    if (m_webPage->settings()->textReflowMode() == WebSettings::TextReflowEnabled) {
        // This is a hack for email which has reflow always turned on.
        m_mainFrame->view()->setNeedsLayout();
        requestLayoutIfNeeded();
        if (m_currentPinchZoomNode)
            newScrollPosition = calculateReflowedScrollPosition(anchorOffset, scale == minimumScale() ? 1 : inverseScale);
         m_currentPinchZoomNode = 0;
         m_anchorInNodeRectRatio = FloatPoint(-1, -1);
    }

    setScrollPosition(newScrollPosition);

    notifyTransformChanged();

    bool isLoading = this->isLoading();

    // We need to invalidate all tiles both visible and non-visible if we're loading.
    m_backingStore->d->updateTiles(isLoading /*updateVisible*/, false /*immediate*/);

    m_client->resetBitmapZoomScale(m_transformationMatrix->m11());

    bool shouldRender = !isLoading || m_userPerformedManualZoom || forceRendering;
    bool shouldClearVisibleZoom = isLoading && shouldRender;

    if (shouldClearVisibleZoom) {
        // If we are loading and rendering then we need to clear the render queue's
        // visible zoom jobs as they will be irrelevant with the render below.
        m_backingStore->d->clearVisibleZoom();
    }

    // Clear window to make sure there are no artifacts.
    if (shouldRender) {
        m_backingStore->d->clearWindow();
        // Resume all screen updates to the backingstore and render+blit visible contents to screen.
        m_backingStore->d->resumeScreenAndBackingStoreUpdates(BackingStore::RenderAndBlit);
    } else {
        // Resume all screen updates to the backingstore but do not blit to the screen because we not rendering.
        m_backingStore->d->resumeScreenAndBackingStoreUpdates(BackingStore::None);
    }

    m_client->zoomChanged(m_webPage->isMinZoomed(), m_webPage->isMaxZoomed(), !shouldZoomOnEscape(), currentScale());

    return true;
}

WebCore::IntPoint WebPagePrivate::calculateReflowedScrollPosition(const WebCore::FloatPoint& anchorOffset, double inverseScale)
{
    // Should only be invoked when text reflow is enabled
    ASSERT(m_webPage->settings()->textReflowMode() == WebSettings::TextReflowEnabled);

    int offsetY = 0;
    int offsetX = 0;

    IntRect nodeRect = rectForNode(m_currentPinchZoomNode.get());

    if (m_currentPinchZoomNode->renderer() && m_anchorInNodeRectRatio.y() >= 0) {
        offsetY = nodeRect.height() * m_anchorInNodeRectRatio.y();
        if (m_currentPinchZoomNode->renderer()->isImage() && m_anchorInNodeRectRatio.x() > 0)
            offsetX = nodeRect.width() * m_anchorInNodeRectRatio.x() - anchorOffset.x() / inverseScale;
    }

    IntRect reflowedRect = adjustRectOffsetForFrameOffset(nodeRect, m_currentPinchZoomNode.get());

    return IntPoint(max(0, static_cast<int>(roundf(reflowedRect.x() + offsetX))),
                    max(0, static_cast<int>(roundf(reflowedRect.y() + offsetY - anchorOffset.y() / inverseScale))));
}

bool WebPagePrivate::scheduleZoomAboutPoint(double unclampedScale, const FloatPoint& anchor, bool enforceScaleClamping, bool forceRendering)
{
    double scale;
    if (!shouldZoomAboutPoint(unclampedScale, anchor, enforceScaleClamping, &scale)) {
        // We could be back to the right zoom level before the timer has
        // timed out, because of wiggling back and forth. Stop the timer.
        unscheduleZoomAboutPoint();
        return false;
    }

    // For some reason, the bitmap zoom wants an anchor in backingstore coordinates!
    // this is different from zoomAboutPoint, which wants content coordinates.
    // See RIM Bug #641.

    FloatPoint transformedAnchor = mapToTransformedFloatPoint(anchor);
    FloatPoint transformedScrollPosition = mapToTransformedFloatPoint(scrollPosition());

    // Prohibit backingstore from updating the window overtop of the bitmap.
    m_backingStore->d->suspendScreenAndBackingStoreUpdates();

    // Need to invert the previous transform to anchor the viewport.
    double zoomFraction = scale / transformationMatrix()->m11();

    // Anchor offset from scroll position in float.
    FloatPoint anchorOffset(transformedAnchor.x() - transformedScrollPosition.x(),
                            transformedAnchor.y() - transformedScrollPosition.y());

    IntPoint srcPoint(
        static_cast<int>(roundf(transformedAnchor.x() - anchorOffset.x() / zoomFraction)),
        static_cast<int>(roundf(transformedAnchor.y() - anchorOffset.y() / zoomFraction)));

    const IntRect viewportRect = IntRect(IntPoint(0, 0), transformedViewportSize());
    const IntRect dstRect = viewportRect;

    // This is the rect to pass as the actual source rect in the backingstore
    // for the transform given by zoom.
    IntRect srcRect(srcPoint.x(),
                    srcPoint.y(),
                    viewportRect.width() / zoomFraction,
                    viewportRect.height() / zoomFraction);
    m_backingStore->d->blitContents(dstRect, srcRect);

    m_delayedZoomArguments.scale = scale;
    m_delayedZoomArguments.anchor = anchor;
    m_delayedZoomArguments.enforceScaleClamping = enforceScaleClamping;
    m_delayedZoomArguments.forceRendering = forceRendering;
    m_delayedZoomTimer->startOneShot(delayedZoomInterval);

    return true;
}

void WebPagePrivate::unscheduleZoomAboutPoint()
{
    if (m_delayedZoomTimer->isActive())
        m_backingStore->d->resumeScreenAndBackingStoreUpdates(BackingStore::None);

    m_delayedZoomTimer->stop();
}

void WebPagePrivate::zoomAboutPointTimerFired(WebCore::Timer<WebPagePrivate>*)
{
    zoomAboutPoint(m_delayedZoomArguments.scale, m_delayedZoomArguments.anchor, m_delayedZoomArguments.enforceScaleClamping, m_delayedZoomArguments.forceRendering);
}

void WebPagePrivate::setNeedsLayout()
{
    FrameView* view = m_mainFrame->view();
    ASSERT(view);
    view->setNeedsLayout();
}

void WebPagePrivate::requestLayoutIfNeeded() const
{
    FrameView* view = m_mainFrame->view();
    ASSERT(view);
    view->updateLayoutAndStyleIfNeededRecursive();
    ASSERT(!view->needsLayout());
}

WebCore::IntPoint WebPagePrivate::scrollPosition() const
{
    return m_backingStoreClient->scrollPosition();
}

WebCore::IntPoint WebPagePrivate::maximumScrollPosition() const
{
    return m_backingStoreClient->maximumScrollPosition();
}

void WebPagePrivate::setScrollPosition(const WebCore::IntPoint& pos)
{
    m_backingStoreClient->setScrollPosition(pos);
}

bool WebPagePrivate::shouldSendResizeEvent()
{
    if (!m_mainFrame->document())
        return false;

    // PR#96865 : Provide an option to always send resize events, regardless of the loading
    //            status. The scenario for this are Sapphire applications which tend to
    //            maintain an open GET request to the server. This open GET results in
    //            webkit thinking that content is still arriving when at the application
    //            level it is considered fully loaded.
    //
    //            NOTE: Care must be exercised in the use of this option, as it bypasses
    //                  the sanity provided in 'isLoadingInAPISense()' below.
    //
    static const bool unrestrictedResizeEvents = BlackBerry::Platform::Settings::get()->unrestrictedResizeEvents();
    if (unrestrictedResizeEvents)
        return true;

    // Don't send the resize event if the document is loading. Some pages automatically reload
    // when the window is resized; Safari on iPhone often resizes the window while setting up its
    // viewport. This obviously can cause problems.
    DocumentLoader* documentLoader = m_mainFrame->loader()->documentLoader();
    if (documentLoader && documentLoader->isLoadingInAPISense())
        return false;

    return true;
}

void WebPagePrivate::willDeferLoading()
{
    m_client->willDeferLoading();
}

void WebPagePrivate::didResumeLoading()
{
    m_client->didResumeLoading();
}

bool WebPage::scrollBy(const Platform::IntSize& delta, bool scrollMainFrame)
{
    d->m_backingStoreClient->setIsClientGeneratedScroll(true);
    bool b = d->scrollBy(delta.width(), delta.height(), scrollMainFrame);
    d->m_backingStoreClient->setIsClientGeneratedScroll(false);
    return b;
}

bool WebPagePrivate::scrollBy(int deltaX, int deltaY, bool scrollMainFrame)
{
    IntSize delta(deltaX, deltaY);
    if (!scrollMainFrame) {
        // We need to work around the fact that ::map{To,From}Transformed do not
        // work well with negative values, like a negative width or height of an IntSize.
        WebCore::IntSize copiedDelta(WebCore::IntSize(abs(delta.width()), abs(delta.height())));
        WebCore::IntSize untransformedCopiedDelta = mapFromTransformed(copiedDelta);
        delta = WebCore::IntSize(
            delta.width() < 0 ? -untransformedCopiedDelta.width() : untransformedCopiedDelta.width(),
            delta.height() < 0 ? -untransformedCopiedDelta.height(): untransformedCopiedDelta.height());

        if (m_inRegionScrollStartingNode) {
            if (scrollNodeRecursively(m_inRegionScrollStartingNode.get(), delta)) {
                m_selectionHandler->selectionPositionChanged();
                // FIXME: We have code in place to handle scrolling and clipping tap highlight
                // on in-region scrolling. As soon as it is fast enough (i.e. we have it backed by
                // a backing store), we can reliably make use of it in the real world.
                // m_touchEventHandler->drawTapHighlight();
                return true;
            }
        }

        return false;
    }

    setScrollPosition(scrollPosition() + delta);
    return true;
}

void WebPage::notifyInRegionScrollStatusChanged(bool status)
{
    d->notifyInRegionScrollStatusChanged(status);
}

void WebPagePrivate::notifyInRegionScrollStatusChanged(bool status)
{
    if (!status && m_inRegionScrollStartingNode) {
        enqueueRenderingOfClippedContentOfScrollableNodeAfterInRegionScrolling(m_inRegionScrollStartingNode.get());
        m_inRegionScrollStartingNode = 0;
    }
}

void WebPagePrivate::enqueueRenderingOfClippedContentOfScrollableNodeAfterInRegionScrolling(WebCore::Node* scrolledNode)
{
    ASSERT(scrolledNode);
    if (scrolledNode->isDocumentNode()) {
        Frame* frame = static_cast<const Document*>(scrolledNode)->frame();
        ASSERT(frame);
        if (!frame)
            return;
        ASSERT(frame != m_mainFrame);
        FrameView* view = frame->view();
        if (!view)
            return;

        // Steps:
        // #1 - Get frame rect in contents coords.
        // #2 - Get the clipped scrollview rect in contents coords.
        // #3 - Take transform into account for 1 and 2.
        // #4 - Subtract 2 from 1, so we know exactly which areas of the frame
        //      are offscreen, and need async repainting.
        FrameView* mainFrameView = m_mainFrame->view();
        ASSERT(mainFrameView);
        WebCore::IntRect frameRect = view->frameRect();
        frameRect = frame->tree()->parent()->view()->contentsToWindow(frameRect);
        frameRect = mainFrameView->windowToContents(frameRect);

        WebCore::IntRect visibleWindowRect = getRecursiveVisibleWindowRect(view);
        WebCore::IntRect visibleContentsRect = mainFrameView->windowToContents(visibleWindowRect);

        WebCore::IntRect transformedFrameRect = mapToTransformed(frameRect);
        WebCore::IntRect transformedVisibleContentsRect = mapToTransformed(visibleContentsRect);

        Platform::IntRectRegion offscreenRegionOfIframe
            = Platform::IntRectRegion::subtractRegions(Platform::IntRect(transformedFrameRect), Platform::IntRect(transformedVisibleContentsRect));

        if (!offscreenRegionOfIframe.isEmpty())
            m_backingStore->d->m_renderQueue->addToQueue(RenderQueue::RegularRender, offscreenRegionOfIframe.rects());
    }
}

void WebPagePrivate::setHasInRegionScrollableAreas(bool b)
{
    if (b != m_hasInRegionScrollableAreas)
        m_hasInRegionScrollableAreas = b;
}

WebCore::IntSize WebPagePrivate::viewportSize() const
{
    return mapFromTransformed(transformedViewportSize());
}

WebCore::IntSize WebPagePrivate::actualVisibleSize() const
{
    return mapFromTransformed(transformedActualVisibleSize());
}

bool WebPagePrivate::hasVirtualViewport() const
{
    return m_virtualViewportWidth && m_virtualViewportHeight;
}

void WebPagePrivate::updateViewportSize(bool setFixedReportedSize, bool sendResizeEvent)
{
    ASSERT(m_mainFrame->view());
    if (setFixedReportedSize)
        m_mainFrame->view()->setFixedReportedSize(actualVisibleSize());

    IntRect frameRect = IntRect(scrollPosition(), viewportSize());
    if (frameRect != m_mainFrame->view()->frameRect()) {
        m_mainFrame->view()->setFrameRect(frameRect);
        m_mainFrame->view()->adjustViewSize();
    }

    // We're going to need to send a resize event to JavaScript because
    // innerWidth and innerHeight depend on fixed reported size.
    // This is how we support mobile pages where JavaScript resizes
    // the page in order to get around the fixed layout size, e.g.
    // google maps when it detects a mobile user agent.
    if (sendResizeEvent && shouldSendResizeEvent())
        m_mainFrame->eventHandler()->sendResizeEvent();

    // When the actual visible size changes, we also
    // need to reposition fixed elements.
    m_mainFrame->view()->repaintFixedElementsAfterScrolling();
}

WebCore::FloatPoint WebPagePrivate::centerOfVisibleContentsRect() const
{
    // The visible contents rect in float.
    FloatRect visibleContentsRect = this->visibleContentsRect();

    // The center of the visible contents rect in float.
    return FloatPoint(visibleContentsRect.x() + visibleContentsRect.width() / 2.0,
                      visibleContentsRect.y() + visibleContentsRect.height() / 2.0);
}

WebCore::IntRect WebPagePrivate::visibleContentsRect() const
{
    return m_backingStoreClient->visibleContentsRect();
}

WebCore::IntSize WebPagePrivate::contentsSize() const
{
    if (!m_mainFrame->view())
        return IntSize();

    return m_backingStoreClient->contentsSize();
}

WebCore::IntSize WebPagePrivate::absoluteVisibleOverflowSize() const
{
    if (!m_mainFrame->contentRenderer())
        return IntSize();
    return IntSize(m_mainFrame->contentRenderer()->rightAbsoluteVisibleOverflow(), m_mainFrame->contentRenderer()->bottomAbsoluteVisibleOverflow());
}

void WebPagePrivate::contentsSizeChanged(const WebCore::IntSize& contentsSize)
{
    if (m_previousContentsSize == contentsSize)
        return;

    // This should only occur in the middle of layout so we set a flag here and
    // handle it at the end of the layout.
    m_contentsSizeChanged = true;

#if DEBUG_WEBPAGE_LOAD
    BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "WebPagePrivate::contentsSizeChanged %dx%d", contentsSize.width(), contentsSize.height());
#endif
}

void WebPagePrivate::overflowExceedsContentsSize()
{
    m_overflowExceedsContentsSize = true;
}

void WebPagePrivate::layoutFinished()
{
    if (!m_contentsSizeChanged && !m_overflowExceedsContentsSize)
        return;

    m_contentsSizeChanged = false; // Toggle to turn off notification again.
    m_overflowExceedsContentsSize = false;

    if (contentsSize().isEmpty())
        return;

    // The call to zoomToInitialScaleOnLoad can cause recursive layout when called from
    // the middle of a layout, but the recursion is limited by detection code in
    // setViewMode() and mitigation code in fixedLayoutSize().
    if (didLayoutExceedMaximumIterations()) {
        notifyTransformedContentsSizeChanged();
        return;
    }

    // Temporarily save the m_previousContentsSize here before updating it (in
    // notifyTransformedContentsSizeChanged()) so we can compare if our contents
    // shrunk afterwards.
    WebCore::IntSize previousContentsSize = m_previousContentsSize;

    m_nestedLayoutFinishedCount++;

    if (loadState() == Committed)
        zoomToInitialScaleOnLoad();
    else if (loadState() != None)
        notifyTransformedContentsSizeChanged();

    m_nestedLayoutFinishedCount--;

    if (!m_nestedLayoutFinishedCount) {
        // When the contents shrinks, there is a risk that we
        // will be left at a scroll position that lies outside of the
        // contents rect. Since we allow overscrolling and neglect
        // to clamp overscroll in order to retain input focus (RIM Bug #414)
        // we need to clamp somewhere, and this is where we know the
        // contents size has changed.

        if (contentsSize() != previousContentsSize) {

            IntPoint newScrollPosition = scrollPosition();

            if (contentsSize().height() < previousContentsSize.height()) {
                WebCore::IntPoint scrollPositionWithHeightShrunk =
                    WebCore::IntPoint(newScrollPosition.x(), maximumScrollPosition().y());
                newScrollPosition = newScrollPosition.shrunkTo(scrollPositionWithHeightShrunk);
            }

            if (contentsSize().width() < previousContentsSize.width()) {
                WebCore::IntPoint scrollPositionWithWidthShrunk =
                    WebCore::IntPoint(maximumScrollPosition().x(), newScrollPosition.y());
                newScrollPosition = newScrollPosition.shrunkTo(scrollPositionWithWidthShrunk);
            }

            if (newScrollPosition != scrollPosition()) {
                setScrollPosition(newScrollPosition);
                notifyTransformedScrollChanged();
            }
        }
    }
}

void WebPagePrivate::zoomToInitialScaleOnLoad()
{
#if DEBUG_WEBPAGE_LOAD
    BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "WebPagePrivate::zoomToInitialScaleOnLoad");
#endif

    bool needsLayout = false;

    // If the contents width exceeds the viewport width set to desktop mode.
    if (m_shouldUseFixedDesktopMode)
        needsLayout = setViewMode(FixedDesktop);
#if !OS(QNX)
    else {
        // On handset, if a web page would fit in landscape mode use mobile.
        int maxMobileWidth = BlackBerry::Platform::Graphics::Screen::landscapeWidth();

        if (contentsSize().width() > maxMobileWidth
        || absoluteVisibleOverflowSize().width() > maxMobileWidth)
            needsLayout = setViewMode(Desktop);
        else
            needsLayout = setViewMode(Mobile);
    }
#else
    else
        needsLayout = setViewMode(Desktop);
#endif

    if (needsLayout) {
        // This can cause recursive layout...
        setNeedsLayout();
    }

    if (contentsSize().isEmpty()) {
#if DEBUG_WEBPAGE_LOAD
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "WebPagePrivate::zoomToInitialScaleOnLoad content is empty!");
#endif
        requestLayoutIfNeeded();
        m_client->resetBitmapZoomScale(currentScale());
        notifyTransformedContentsSizeChanged();
        return;
    }

    bool performedZoom = false;
    bool shouldZoom = !m_userPerformedManualZoom;

    // If this load should restore view state, don't zoom to initial scale
    // but instead let the HistoryItem's saved viewport reign supreme.
    if (m_mainFrame && m_mainFrame->loader() && m_mainFrame->loader()->shouldRestoreScrollPositionAndViewState())
        shouldZoom = false;

    if (shouldZoom && loadState() == Committed) {
        // Preserve at top and at left position, to avoid scrolling
        // to a non top-left position for web page with viewport meta tag
        // that specifies an initial-scale that is zoomed in.
        FloatPoint anchor = centerOfVisibleContentsRect();
        if (!scrollPosition().x())
            anchor.setX(0);
        if (!scrollPosition().y())
            anchor.setY(0);
        performedZoom = zoomAboutPoint(initialScale(), anchor);
    }

    // zoomAboutPoint above can also toggle setNeedsLayout and cause recursive layout...
    requestLayoutIfNeeded();

    if (!performedZoom) {
        // We only notify if we didn't perform zoom, because zoom will notify on
        // its own...
        m_client->resetBitmapZoomScale(currentScale());
        notifyTransformedContentsSizeChanged();
    }
}

double WebPagePrivate::currentScale() const
{
    return m_transformationMatrix->m11();
}

double WebPagePrivate::zoomToFitScale() const
{
    // We must clamp the contents for this calculation so that we do not allow an
    // arbitrarily small zoomToFitScale much like we clamp the fixedLayoutSize()
    // so that we do not have arbitrarily large layout size.
    // If we have a specified viewport, we may need to be able to zoom out more.
    int contentWidth = std::min(contentsSize().width(), std::max(m_virtualViewportWidth, static_cast<int>(defaultMaxLayoutSize().width())));

    // defaultMaxLayoutSize().width() is a safeguard for excessively large page layouts that
    // is too restrictive for image documents. In this case, the document width is sufficient.
    Document* doc = m_page->mainFrame()->document();
    if (doc && doc->isImageDocument())
       contentWidth = contentsSize().width();

    // If we have a virtual viewport and its aspect ratio caused content to layout
    // wider than the default layout aspect ratio we need to zoom to fit the content height
    // in order to avoid showing a grey area below the web page.
    // Without virtual viewport we can never get into this situation.
    if (hasVirtualViewport()) {
        int contentHeight = std::min(contentsSize().height(), std::max(m_virtualViewportHeight, static_cast<int>(defaultMaxLayoutSize().height())));

        // Aspect ratio check without division.
        if (contentWidth * m_defaultLayoutSize.height() > contentHeight * m_defaultLayoutSize.width())
            return contentHeight > 0 ? static_cast<double>(m_defaultLayoutSize.height()) / contentHeight : 1.0;
    }

    return contentWidth > 0.0 ? static_cast<double>(m_actualVisibleWidth) / contentWidth : 1.0;
}

double WebPagePrivate::initialScale() const
{
    if (m_initialScale > 0.0)
        return m_initialScale;

    if (m_webSettings->isZoomToFitOnLoad())
        return zoomToFitScale();

    return 1.0;
}

void WebPage::initializeIconDataBase()
{
    IconDatabaseClientBlackBerry::getInstance()->initIconDatabase(d->m_webSettings);
}

bool WebPage::isUserScalable() const
{
    return d->isUserScalable();
}

double WebPage::currentScale() const
{
    return d->currentScale();
}

double WebPage::initialScale() const
{
    return d->initialScale();
}

double WebPage::zoomToFitScale() const
{
    return d->zoomToFitScale();
}

void WebPage::setInitialScale(double initialScale)
{
    d->setInitialScale(initialScale);
}

double WebPage::minimumScale() const
{
    return d->minimumScale();
}

void WebPage::setMinimumScale(double minimumScale)
{
    d->setMinimumScale(minimumScale);
}

double WebPage::maximumScale() const
{
    return d->maximumScale();
}

void WebPage::setMaximumScale(double maximumScale)
{
    d->setMaximumScale(maximumScale);
}

double WebPagePrivate::minimumScale() const
{
    return (m_minimumScale > zoomToFitScale() && m_minimumScale <= maximumScale()) ? m_minimumScale : zoomToFitScale();
}

double WebPagePrivate::maximumScale() const
{
    if (m_maximumScale >= zoomToFitScale() && m_maximumScale >= m_minimumScale)
        return m_maximumScale;

    return hasVirtualViewport() ? std::max<double>(zoomToFitScale(), 4.0) : 4.0;
}

void WebPagePrivate::resetScales()
{
    TransformationMatrix identity;
    *m_transformationMatrix = identity;
    m_initialScale = m_webSettings->initialScale() > 0 ? m_webSettings->initialScale() : -1.0;
    m_minimumScale = -1.0;
    m_maximumScale = -1.0;

    // We have to let WebCore know about updated framerect now that we've
    // reset our scales. See: RIM Bug #401.
    updateViewportSize();
}

WebCore::IntPoint WebPagePrivate::transformedScrollPosition() const
{
    return m_backingStoreClient->transformedScrollPosition();
}

WebCore::IntPoint WebPagePrivate::transformedMaximumScrollPosition() const
{
    return m_backingStoreClient->transformedMaximumScrollPosition();
}

WebCore::IntSize WebPagePrivate::transformedActualVisibleSize() const
{
    return IntSize(m_actualVisibleWidth, m_actualVisibleHeight);
}

WebCore::IntSize WebPagePrivate::transformedViewportSize() const
{
    return BlackBerry::Platform::Graphics::Screen::size();
}

WebCore::IntRect WebPagePrivate::transformedVisibleContentsRect() const
{
    // Usually this would be mapToTransformed(visibleContentsRect()), but
    // that results in rounding errors because we already set the WebCore
    // viewport size from our original transformedViewportSize().
    // Instead, we only transform the scroll position and take the
    // viewport size as it is, which ensures that e.g. blitting operations
    // always cover the whole widget/screen.
    return IntRect(transformedScrollPosition(), transformedViewportSize());
}

WebCore::IntSize WebPagePrivate::transformedContentsSize() const
{
    // mapToTransformed() functions use this method to crop their results,
    // so we can't make use of them here. While we want rounding inside page
    // boundaries to extend rectangles and round points, we need to crop the
    // contents size to the floored values so that we don't try to display
    // or report points that are not fully covered by the actual float-point
    // contents rectangle.
    const WebCore::IntSize untransformedContentsSize = contentsSize();
    const WebCore::FloatPoint transformedBottomRight = m_transformationMatrix->mapPoint(
        WebCore::FloatPoint(untransformedContentsSize.width(), untransformedContentsSize.height()));
    return WebCore::IntSize(floorf(transformedBottomRight.x()), floorf(transformedBottomRight.y()));
}

WebCore::IntPoint WebPagePrivate::mapFromContentsToViewport(const WebCore::IntPoint& point) const
{
    return m_backingStoreClient->mapFromContentsToViewport(point);
}

WebCore::IntPoint WebPagePrivate::mapFromViewportToContents(const WebCore::IntPoint& point) const
{
    return m_backingStoreClient->mapFromViewportToContents(point);
}

WebCore::IntRect WebPagePrivate::mapFromContentsToViewport(const WebCore::IntRect& rect) const
{
    return m_backingStoreClient->mapFromContentsToViewport(rect);
}

WebCore::IntRect WebPagePrivate::mapFromViewportToContents(const WebCore::IntRect& rect) const
{
    return m_backingStoreClient->mapFromViewportToContents(rect);
}

WebCore::IntPoint WebPagePrivate::mapFromTransformedContentsToTransformedViewport(const WebCore::IntPoint& point) const
{
    return m_backingStoreClient->mapFromTransformedContentsToTransformedViewport(point);
}

WebCore::IntPoint WebPagePrivate::mapFromTransformedViewportToTransformedContents(const WebCore::IntPoint& point) const
{
    return m_backingStoreClient->mapFromTransformedViewportToTransformedContents(point);
}

WebCore::IntRect WebPagePrivate::mapFromTransformedContentsToTransformedViewport(const WebCore::IntRect& rect) const
{
    return m_backingStoreClient->mapFromTransformedContentsToTransformedViewport(rect);
}

WebCore::IntRect WebPagePrivate::mapFromTransformedViewportToTransformedContents(const WebCore::IntRect& rect) const
{
    return m_backingStoreClient->mapFromTransformedViewportToTransformedContents(rect);
}

/*
 * NOTE: PIXEL ROUNDING!
 * Accurate back-and-forth rounding is not possible with information loss
 * by integer points and sizes, so we always expand the resulting mapped
 * float rectangles to the nearest integer. For points, we always use
 * floor-rounding in mapToTransformed() so that we don't have to crop to
 * the (floor'd) transformed contents size.
 */
static inline WebCore::IntPoint roundTransformedPoint(const WebCore::FloatPoint &point)
{
    // Maps by rounding half towards zero.
    return IntPoint(static_cast<int>(floorf(point.x())), static_cast<int>(floorf(point.y())));
}

static inline WebCore::IntPoint roundUntransformedPoint(const WebCore::FloatPoint &point)
{
    // Maps by rounding half away from zero.
    return IntPoint(static_cast<int>(ceilf(point.x())), static_cast<int>(ceilf(point.y())));
}

WebCore::IntPoint WebPagePrivate::mapToTransformed(const WebCore::IntPoint& point) const
{
    return roundTransformedPoint(m_transformationMatrix->mapPoint(FloatPoint(point)));
}

WebCore::FloatPoint WebPagePrivate::mapToTransformedFloatPoint(const WebCore::FloatPoint& point) const
{
    return m_transformationMatrix->mapPoint(point);
}

WebCore::IntPoint WebPagePrivate::mapFromTransformed(const WebCore::IntPoint& point) const
{
    return roundUntransformedPoint(m_transformationMatrix->inverse().mapPoint(FloatPoint(point)));
}

WebCore::FloatPoint WebPagePrivate::mapFromTransformedFloatPoint(const WebCore::FloatPoint& point) const
{
    return m_transformationMatrix->inverse().mapPoint(point);
}

WebCore::FloatRect WebPagePrivate::mapFromTransformedFloatRect(const WebCore::FloatRect& rect) const
{
    return m_transformationMatrix->inverse().mapRect(rect);
}

WebCore::IntSize WebPagePrivate::mapToTransformed(const WebCore::IntSize& size) const
{
    return mapToTransformed(IntRect(IntPoint(0, 0), size)).size();
}

WebCore::IntSize WebPagePrivate::mapFromTransformed(const WebCore::IntSize& size) const
{
    return mapFromTransformed(IntRect(IntPoint(0, 0), size)).size();
}

WebCore::IntRect WebPagePrivate::mapToTransformed(const WebCore::IntRect& rect) const
{
    return enclosingIntRect(m_transformationMatrix->mapRect(FloatRect(rect)));
}

// Use this in conjunction with mapToTransformed(IntRect), in most cases.
void WebPagePrivate::clipToTransformedContentsRect(WebCore::IntRect& rect) const
{
    rect.intersect(IntRect(IntPoint(0, 0), transformedContentsSize()));
}

WebCore::IntRect WebPagePrivate::mapFromTransformed(const WebCore::IntRect& rect) const
{
    return enclosingIntRect(m_transformationMatrix->inverse().mapRect(FloatRect(rect)));
}

bool WebPagePrivate::transformedPointEqualsUntransformedPoint(const WebCore::IntPoint& transformedPoint, const WebCore::IntPoint& untransformedPoint)
{
    // Scaling down is always more accurate than scaling up.
    if (m_transformationMatrix->a() > 1.0)
        return transformedPoint == mapToTransformed(untransformedPoint);

    return mapFromTransformed(transformedPoint) == untransformedPoint;
}

void WebPagePrivate::notifyTransformChanged()
{
    notifyTransformedContentsSizeChanged();
    notifyTransformedScrollChanged();

    m_backingStore->d->transformChanged();
}

void WebPagePrivate::notifyTransformedContentsSizeChanged()
{
    // We mark here as the last reported content size we sent to the client.
    m_previousContentsSize = contentsSize();

    const IntSize size = transformedContentsSize();
    m_backingStore->d->contentsSizeChanged(size);
    m_client->contentsSizeChanged(size);
    m_selectionHandler->selectionPositionChanged();
}

void WebPagePrivate::notifyTransformedScrollChanged()
{
    const IntPoint pos = transformedScrollPosition();
    m_backingStore->d->scrollChanged(pos);
    m_client->scrollChanged(pos);
}

bool WebPagePrivate::setViewMode(ViewMode mode)
{
    if (!m_mainFrame->view())
        return false;

    m_viewMode = mode;

    // If we're in the middle of a nested layout with a recursion count above
    // some maximum threshold, then our algorithm for finding the minimum content
    // width of a given page has become dependent on the visible width.
    //
    // We need to find some method to ensure that we don't experience excessive
    // and even infinite recursion. This can even happen with valid html. The
    // former can happen when we run into inline text with few candidates for line
    // break. The latter can happen for instance if the page has a negative margin
    // set against the right border. Note: this is valid by spec and can lead to
    // a situation where there is no value for which the content width will ensure
    // no horizontal scrollbar.
    // Example: LayoutTests/css1/box_properties/margin.html
    //
    // In order to address such situations when we detect a recursion above some
    // maximum threshold we snap our fixed layout size to a defined quantum increment.
    // Eventually, either the content width will be satisfied to ensure no horizontal
    // scrollbar or this increment will run into the maximum layout size and the
    // recursion will necessarily end.
    bool snapToIncrement = didLayoutExceedMaximumIterations();

    IntSize currentSize = m_mainFrame->view()->fixedLayoutSize();
    IntSize newSize = fixedLayoutSize(snapToIncrement);
    if (currentSize == newSize)
        return false;

    // FIXME: Temp solution. We'll get back to this.
    if (m_nestedLayoutFinishedCount) {
        double widthChange = fabs(double(newSize.width() - currentSize.width()) / currentSize.width());
        double heightChange = fabs(double(newSize.height() - currentSize.height()) / currentSize.height());
        if (widthChange < 0.05 && heightChange < 0.05)
            return false;
    }

    m_mainFrame->view()->setUseFixedLayout(useFixedLayout());
    m_mainFrame->view()->setFixedLayoutSize(newSize);
    return true; // Needs re-layout!
}

void WebPagePrivate::setCursor(WebCore::PlatformCursorHandle handle)
{
    if (m_currentCursor.type() != handle.type()) {
        m_currentCursor = handle;
        m_client->cursorChanged(handle.type(), handle.url().c_str(), handle.hotspot().x(), handle.hotspot().y());
    }
}

BlackBerry::Platform::NetworkStreamFactory* WebPagePrivate::networkStreamFactory()
{
    return m_client->networkStreamFactory();
}


BlackBerry::Platform::Graphics::Window* WebPagePrivate::platformWindow() const
{
    return m_client->window();
}

void WebPagePrivate::setPreventsScreenDimming(bool keepAwake)
{
    if (keepAwake) {
        if (!m_preventIdleDimmingCount)
            m_client->setPreventsScreenIdleDimming(true);
        m_preventIdleDimmingCount++;
    } else if (m_preventIdleDimmingCount > 0) {
        m_preventIdleDimmingCount--;
        if (!m_preventIdleDimmingCount)
            m_client->setPreventsScreenIdleDimming(false);
    } else
        ASSERT_NOT_REACHED(); // SetPreventsScreenIdleDimming(false) called too many times.
}

void WebPagePrivate::showVirtualKeyboard(bool showKeyboard)
{
    m_client->inputSetNavigationMode(showKeyboard);
}

void WebPagePrivate::ensureContentVisible(bool centerInView)
{
    m_inputHandler->ensureFocusElementVisible(centerInView);
}

void WebPagePrivate::zoomToContentRect(const WebCore::IntRect& rect)
{
    // Don't scale if the user is not supposed to scale.
    if (!isUserScalable())
        return;

    FloatPoint anchor = FloatPoint(rect.width() / 2.0 + rect.x(), rect.height() / 2.0 + rect.y());
    IntSize viewSize = viewportSize();

    // Calculate the scale required to scale that dimension to fit.
    double scaleH = (double)viewSize.width() / (double)rect.width();
    double scaleV = (double)viewSize.height() / (double)rect.height();

    // Choose the smaller scale factor so that all of the content is visible.
    zoomAboutPoint(min(scaleH, scaleV), anchor);
}

void WebPagePrivate::registerPlugin(WebCore::PluginView* plugin, bool shouldRegister)
{
    if (shouldRegister)
        m_pluginViews.add(plugin);
    else
        m_pluginViews.remove(plugin);
}

void WebPagePrivate::notifyPageOnLoad()
{
    HashSet<PluginView*>::const_iterator it = m_pluginViews.begin();
    HashSet<PluginView*>::const_iterator last = m_pluginViews.end();
    for (; it != last; ++it)
        (*it)->handleOnLoadEvent();
}

bool WebPagePrivate::shouldPluginEnterFullScreen(WebCore::PluginView* plugin, const char* windowUniquePrefix)
{
    return m_client->shouldPluginEnterFullScreen();
}

void WebPagePrivate::didPluginEnterFullScreen(WebCore::PluginView* plugin, const char* windowUniquePrefix)
{
    m_fullScreenPluginView = plugin;
    m_client->didPluginEnterFullScreen();
    BlackBerry::Platform::Graphics::Window::setTransparencyDiscardFilter(windowUniquePrefix);
    m_client->window()->setSensitivityFullscreenOverride(true);
}

void WebPagePrivate::didPluginExitFullScreen(WebCore::PluginView* plugin, const char* windowUniquePrefix)
{
    m_fullScreenPluginView = 0;
    m_client->didPluginExitFullScreen();
    BlackBerry::Platform::Graphics::Window::setTransparencyDiscardFilter(0);
    m_client->window()->setSensitivityFullscreenOverride(false);
}

void WebPagePrivate::onPluginStartBackgroundPlay(WebCore::PluginView* plugin, const char* windowUniquePrefix)
{
    m_client->onPluginStartBackgroundPlay();
}

void WebPagePrivate::onPluginStopBackgroundPlay(WebCore::PluginView* plugin, const char* windowUniquePrefix)
{
    m_client->onPluginStopBackgroundPlay();
}

bool WebPagePrivate::lockOrientation(bool landscape)
{
    return m_client->lockOrientation(landscape);
}

void WebPagePrivate::unlockOrientation()
{
    return m_client->unlockOrientation();
}

int WebPagePrivate::orientation() const
{
#if ENABLE(ORIENTATION_EVENTS)
    return m_mainFrame->orientation();
#else
#error ORIENTATION_EVENTS must be defined.
// Or a copy of the orientation value will have to be stored in these objects.
#endif
}

double WebPagePrivate::currentZoomFactor() const
{
    return currentScale();
}

int WebPagePrivate::showAlertDialog(WebPageClient::AlertType atype)
{
    return m_client->showAlertDialog(atype);
}

bool WebPagePrivate::isActive() const
{
    return m_client->isActive();
}

bool WebPagePrivate::useFixedLayout() const
{
    return true;
}

ActiveNodeContext WebPagePrivate::activeNodeContext(TargetDetectionStrategy strategy)
{
    ActiveNodeContext context;

    RefPtr<Node> node = contextNode(strategy);
    m_currentContextNode = node;
    if (!m_currentContextNode)
        return context;

    bool nodeAllowSelectionOverride = false;
    if (Node* linkNode = node->enclosingLinkEventParentOrSelf()) {
        KURL href;
        if (linkNode->isLink() && linkNode->hasAttributes()) {
            if (Attribute* attribute = linkNode->attributes()->getAttributeItem(HTMLNames::hrefAttr))
                href = linkNode->document()->completeURL(stripLeadingAndTrailingHTMLSpaces(attribute->value()));
        }

        WTF::String pattern = findPatternStringForUrl(href);
        if (!pattern.isEmpty())
            context.setPattern(pattern);

        if (!href.string().isEmpty()) {
            context.setUrl(href.string());

            // Links are non-selectable by default, but selection should be allowed
            // providing the page is selectable, use the parent to determine it.
            if (linkNode->parentNode() && linkNode->parentNode()->canStartSelection())
                nodeAllowSelectionOverride = true;
        }
    }

    if (!nodeAllowSelectionOverride && !node->canStartSelection())
        context.resetFlag(ActiveNodeContext::IsSelectable);

    if (node->isHTMLElement()) {
        HTMLImageElement* imageElement = 0;
        if (node->hasTagName(HTMLNames::imgTag))
            imageElement = static_cast<HTMLImageElement*>(node.get());
        else if (node->hasTagName(HTMLNames::areaTag))
            imageElement = static_cast<HTMLAreaElement*>(node.get())->imageElement();
        if (imageElement && imageElement->renderer()) {
            // FIXME: At the mean time, we only show "Save Image" when the image data is available.
            if (CachedResource* cachedResource = imageElement->cachedImage()) {
                if (cachedResource->isLoaded() && cachedResource->data()) {
                    WTF::String url = stripLeadingAndTrailingHTMLSpaces(imageElement->getAttribute(HTMLNames::srcAttr).string());
                    context.setImageSrc(node->document()->completeURL(url).string());
                }
            }
            WTF::String alt = imageElement->altText();
            if (!alt.isNull())
                context.setImageAlt(alt);
        }
    }

    if (node->isTextNode()) {
        WebCore::Text* curText = static_cast<WebCore::Text*>(node.get());
        if (!curText->wholeText().isEmpty())
            context.setText(curText->wholeText());
    }

    if (node->isElementNode()) {
        Element* element = static_cast<Element*>(node->shadowAncestorNode());
        if (DOMSupport::isTextBasedContentEditableElement(element)) {
            context.setFlag(ActiveNodeContext::IsInput);
            if (element->hasTagName(HTMLNames::inputTag))
                context.setFlag(ActiveNodeContext::IsSingleLine);
            if (DOMSupport::isPasswordElement(element))
                context.setFlag(ActiveNodeContext::IsPassword);

            WTF::String elementText(DOMSupport::inputElementText(element));
            if (!elementText.stripWhiteSpace().isEmpty())
                context.setText(elementText);
        }
    }

    if (node->isFocusable())
        context.setFlag(ActiveNodeContext::IsFocusable);

    return context;
}

void WebPagePrivate::updateCursor()
{
    int buttonMask = 0;
    if (m_lastMouseEvent.button() == LeftButton)
        buttonMask = BlackBerry::Platform::MouseEvent::ScreenLeftMouseButton;
    else if (m_lastMouseEvent.button() == MiddleButton)
        buttonMask = BlackBerry::Platform::MouseEvent::ScreenMiddleMouseButton;
    else if (m_lastMouseEvent.button() == RightButton)
        buttonMask = BlackBerry::Platform::MouseEvent::ScreenRightMouseButton;

    BlackBerry::Platform::MouseEvent event(buttonMask, buttonMask, mapToTransformed(m_lastMouseEvent.pos()), mapToTransformed(m_lastMouseEvent.globalPos()), 0, 0);
    m_webPage->mouseEvent(event);
}

WebCore::IntSize WebPagePrivate::fixedLayoutSize(bool snapToIncrement) const
{
    if (hasVirtualViewport())
        return IntSize(m_virtualViewportWidth, m_virtualViewportHeight);

    const int defaultLayoutWidth = m_defaultLayoutSize.width();
    const int defaultLayoutHeight = m_defaultLayoutSize.height();

    int minWidth = defaultLayoutWidth;
    int maxWidth = defaultMaxLayoutSize().width();
    int maxHeight = defaultMaxLayoutSize().height();

    // If the load state is none then we haven't actually got anything yet, but we need to layout
    // the entire page so that the user sees the entire page (unrendered) instead of just part of it.
    if (m_loadState == None)
        return IntSize(defaultLayoutWidth, defaultLayoutHeight);

    if (m_viewMode == FixedDesktop) {
        int width  = maxWidth;
        // if the defaultLayoutHeight is at minimum, it probably was set as 0
        // and clamped, meaning it's effectively not set.  (Even if it happened
        // to be set exactly to the minimum, it's too small to be useful.)  So
        // ignore it.
        int height;
        if (defaultLayoutHeight <= minimumLayoutSize.height())
            height = maxHeight;
        else
            height = ceilf(static_cast<float>(width) / static_cast<float>(defaultLayoutWidth) * static_cast<float>(defaultLayoutHeight));
        return IntSize(width, height);
    }

    if (m_viewMode == Desktop) {
        // If we detect an overflow larger than the contents size then use that instead since
        // it'll still be clamped by the maxWidth below...
        int width = std::max(absoluteVisibleOverflowSize().width(), contentsSize().width());

        if (snapToIncrement) {
            // Snap to increments of defaultLayoutWidth / 2.0.
            float factor = static_cast<float>(width) / (defaultLayoutWidth / 2.0);
            factor = ceilf(factor);
            width = (defaultLayoutWidth / 2.0) * factor;
        }

        if (width < minWidth)
            width = minWidth;
        if (width > maxWidth)
            width = maxWidth;
        int height = ceilf(static_cast<float>(width) / static_cast<float>(defaultLayoutWidth) * static_cast<float>(defaultLayoutHeight));
        return IntSize(width, height);
    }

    if (m_webSettings->isZoomToFitOnLoad()) {
        // We need to clamp the layout width to the minimum of the layout
        // width or the content width. This is important under rotation for mobile
        // websites. We want the page to remain layouted at the same width which
        // it was loaded with, and instead change the zoom level to fit to screen.
        // The height is welcome to adapt to the height used in the new orientation,
        // otherwise we will get a grey bar below the web page.
        if (m_mainFrame->view() && !contentsSize().isEmpty())
            minWidth = contentsSize().width();
        else {
            // If there is no contents width, use the minimum of screen width
            // and layout width to shape the first layout to a contents width
            // that we could reasonably zoom to fit, in a manner that takes
            // orientation into account and still respects a small default
            // layout width.
#if ENABLE(ORIENTATION_EVENTS)
            minWidth = m_mainFrame->orientation() % 180
                ? BlackBerry::Platform::Graphics::Screen::height()
                : BlackBerry::Platform::Graphics::Screen::width();
#else
            minWidth = BlackBerry::Platform::Graphics::Screen::width();
#endif
        }
    }

    return IntSize(std::min(minWidth, defaultLayoutWidth), defaultLayoutHeight);
}

BackingStoreClient* WebPagePrivate::backingStoreClientForFrame(const WebCore::Frame* frame) const
{
    ASSERT(frame);
    BackingStoreClient* backingStoreClient = 0;
    if (m_backingStoreClientForFrameMap.contains(frame))
        backingStoreClient = m_backingStoreClientForFrameMap.get(frame);
    return backingStoreClient;
}

void WebPagePrivate::addBackingStoreClientForFrame(const WebCore::Frame* frame, BackingStoreClient* client)
{
    ASSERT(frame);
    ASSERT(client);
    m_backingStoreClientForFrameMap.add(frame, client);
}

void WebPagePrivate::removeBackingStoreClientForFrame(const WebCore::Frame* frame)
{
    ASSERT(frame);
    if (m_backingStoreClientForFrameMap.contains(frame))
        m_backingStoreClientForFrameMap.remove(frame);
}


void WebPagePrivate::clearDocumentData(const WebCore::Document* documentGoingAway)
{
    ASSERT(documentGoingAway);
    if (m_currentContextNode && m_currentContextNode->document() == documentGoingAway)
        m_currentContextNode = 0;

    if (m_currentPinchZoomNode && m_currentPinchZoomNode->document() == documentGoingAway)
        m_currentPinchZoomNode = 0;

    if (m_currentBlockZoomAdjustedNode && m_currentBlockZoomAdjustedNode->document() == documentGoingAway)
        m_currentBlockZoomAdjustedNode = 0;

    if (m_inRegionScrollStartingNode && m_inRegionScrollStartingNode->document() == documentGoingAway)
        m_inRegionScrollStartingNode = 0;

    Node* nodeUnderFatFinger = m_touchEventHandler->lastFatFingersResult().validNode();
    if (nodeUnderFatFinger && nodeUnderFatFinger->document() == documentGoingAway)
        m_touchEventHandler->resetLastFatFingersResult();

    // NOTE: m_fullscreenVideoNode, m_fullScreenPluginView and m_pluginViews
    // are cleared in other methods already.
}

typedef bool (*PredicateFunction)(RenderLayer*);
static bool isPositionedContainer(RenderLayer* layer)
{
    RenderObject* o = layer->renderer();
    return o->isRenderView() || o->isPositioned() || o->isRelPositioned() || layer->hasTransform();
}

static bool isNonRenderViewFixedPositionedContainer(RenderLayer* layer)
{
    RenderObject* o = layer->renderer();
    if (o->isRenderView())
        return false;

    return o->isPositioned() && o->style()->position() == FixedPosition;
}

static bool isFixedPositionedContainer(RenderLayer* layer)
{
    RenderObject* o = layer->renderer();
    return o->isRenderView() || (o->isPositioned() && o->style()->position() == FixedPosition);
}

static RenderLayer* findAncestorOrSelfNotMatching(PredicateFunction predicate, RenderLayer* layer)
{
    RenderLayer* curr = layer;
    while (curr && !predicate(curr))
        curr = curr->parent();

    return curr;
}

RenderLayer* WebPagePrivate::enclosingFixedPositionedAncestorOrSelfIfFixedPositioned(RenderLayer* layer)
{
    return findAncestorOrSelfNotMatching(&isFixedPositionedContainer, layer);
}

RenderLayer* WebPagePrivate::enclosingPositionedAncestorOrSelfIfPositioned(RenderLayer* layer)
{
    return findAncestorOrSelfNotMatching(&isPositionedContainer, layer);
}

static inline Frame* frameForNode(Node* node)
{
    Node* origNode = node;
    for (; node; node = node->parentNode()) {
        if (WebCore::RenderObject* renderer = node->renderer()) {
            if (renderer->isRenderView()) {
                if (FrameView* view = toRenderView(renderer)->frameView()) {
                    if (Frame* frame = view->frame())
                        return frame;
                }
            }
            if (renderer->isWidget()) {
                WebCore::Widget* widget = toRenderWidget(renderer)->widget();
                if (widget && widget->isFrameView()) {
                    if (Frame* frame = static_cast<FrameView*>(widget)->frame())
                        return frame;
                }
            }
        }
    }

    for (node = origNode; node; node = node->parentNode()) {
        if (Document* doc = node->document()) {
            if (Frame* frame = doc->frame())
                return frame;
        }
    }

    return 0;
}

static WebCore::IntRect getNodeWindowRect(Node* node)
{
    if (Frame* frame = frameForNode(node)) {
        if (FrameView* view = frame->view())
            return view->contentsToWindow(node->getRect());
    }
    ASSERT_NOT_REACHED();
    return WebCore::IntRect();
}

WebCore::IntRect WebPagePrivate::getRecursiveVisibleWindowRect(WebCore::ScrollView* view, bool noClipOfMainFrame)
{
    ASSERT(m_mainFrame);

    // Don't call this function asking to not clip the main frame providing only
    // the main frame. All that can be returned is the content rect which
    // isn't what this function is for.
    if (noClipOfMainFrame && view == m_mainFrame->view()) {
        ASSERT_NOT_REACHED();
        return IntRect(IntPoint(0, 0), view->contentsSize());
    }

    IntRect visibleWindowRect(view->contentsToWindow(view->visibleContentRect(false)));
    if (view->parent() && !(noClipOfMainFrame && view->parent() == m_mainFrame->view())) {
        // Intersect with parent visible rect.
        visibleWindowRect.intersect(getRecursiveVisibleWindowRect(view->parent(), noClipOfMainFrame));
    }
    return visibleWindowRect;
}

class ComplicatedDistance {
public:

    enum DistanceType {
        DistanceNone,
        DistanceApart,
        DistanceIntersected,
        DistanceOverlapped,
    };

    DistanceType m_type;
    int m_dist;

    ComplicatedDistance() : m_type(DistanceNone), m_dist(0) { }

    void calcDistance(int baseStart, int baseEnd, int targetStart, int targetEnd)
    {
        m_dist = targetEnd <= baseStart ? targetEnd - baseStart - 1
            : targetStart >= baseEnd ? targetStart - baseEnd + 1
            : 0;
        if (m_dist) {
            m_type = DistanceApart;
            return;
        }

        m_dist = targetStart == baseStart
                ? 0
                : targetStart < baseStart
                ? targetEnd >= baseEnd ? 0 : baseStart - targetEnd
                : targetEnd <= baseEnd ? 0 : baseEnd - targetStart;
        if (m_dist) {
            m_type = DistanceIntersected;
            return;
        }

        m_dist = targetStart - baseStart + targetEnd - baseEnd;
        m_type = DistanceOverlapped;
    }

    bool isPositive() const { return m_dist >= 0; }
    bool isNegative() const { return m_dist <= 0; }

    bool operator<(const ComplicatedDistance& o) const
    {
        return m_type != o.m_type
            ? m_type > o.m_type
            : m_type == DistanceIntersected
            ? abs(m_dist) > abs(o.m_dist)
            : abs(m_dist) < abs(o.m_dist);
    }
    bool operator==(const ComplicatedDistance& o) const
    {
        return m_type == o.m_type && m_dist == o.m_dist;
    }
};

static inline bool checkDirection(BlackBerry::Platform::ScrollDirection dir, const ComplicatedDistance& delta, BlackBerry::Platform::ScrollDirection negDir, BlackBerry::Platform::ScrollDirection posDir)
{
    return dir == negDir ? delta.isNegative() : dir == posDir ? delta.isPositive() : false;
}

static inline bool areNodesPerpendicularlyNavigatable(BlackBerry::Platform::ScrollDirection dir, const WebCore::IntRect& baseRect, const WebCore::IntRect& targetRect)
{
    ComplicatedDistance distX, distY;
    distX.calcDistance(baseRect.x(), baseRect.maxX(), targetRect.x(), targetRect.maxX());
    distY.calcDistance(baseRect.y(), baseRect.maxY(), targetRect.y(), targetRect.maxY());

    return distY < distX ? dir == BlackBerry::Platform::ScrollUp || dir == BlackBerry::Platform::ScrollDown
        : dir == BlackBerry::Platform::ScrollLeft || dir == BlackBerry::Platform::ScrollRight;
}

struct NodeDistance {
    ComplicatedDistance m_dist;
    ComplicatedDistance m_perpendicularDist;
    WebCore::IntRect m_rect;

    bool isCloser(const NodeDistance& o, BlackBerry::Platform::ScrollDirection dir) const
    {
        return m_dist.m_type == o.m_dist.m_type
            ? (m_dist == o.m_dist ? m_perpendicularDist < o.m_perpendicularDist
                : m_dist < o.m_dist ? m_perpendicularDist.m_type >= o.m_perpendicularDist.m_type
                || !areNodesPerpendicularlyNavigatable(dir, o.m_rect, m_rect)
                : m_perpendicularDist.m_type > o.m_perpendicularDist.m_type
                && areNodesPerpendicularlyNavigatable(dir, m_rect, o.m_rect))
            : m_dist.m_type > o.m_dist.m_type;
    }
};

static inline bool tellDistance(BlackBerry::Platform::ScrollDirection dir
                                , const WebCore::IntRect& baseRect
                                , NodeDistance& nodeDist)
{
    ComplicatedDistance distX, distY;
    distX.calcDistance(baseRect.x(), baseRect.maxX(), nodeDist.m_rect.x(), nodeDist.m_rect.maxX());
    distY.calcDistance(baseRect.y(), baseRect.maxY(), nodeDist.m_rect.y(), nodeDist.m_rect.maxY());

    bool horizontal = distY < distX;
    if (horizontal ? checkDirection(dir, distX, BlackBerry::Platform::ScrollLeft, BlackBerry::Platform::ScrollRight) : checkDirection(dir, distY, BlackBerry::Platform::ScrollUp, BlackBerry::Platform::ScrollDown)) {
        nodeDist.m_dist = horizontal ? distX : distY;
        nodeDist.m_perpendicularDist = horizontal ? distY : distX;
        return true;
    }
    return false;
}

void WebPage::assignFocus(BlackBerry::Platform::FocusDirection direction)
{
    d->assignFocus(direction);
}

void WebPagePrivate::assignFocus(BlackBerry::Platform::FocusDirection direction)
{
    ASSERT((int) BlackBerry::Platform::FocusDirectionNone == (int) WebCore::FocusDirectionNone);
    ASSERT((int) BlackBerry::Platform::FocusDirectionForward == (int) WebCore::FocusDirectionForward);
    ASSERT((int) BlackBerry::Platform::FocusDirectionBackward == (int) WebCore::FocusDirectionBackward);

    // First we clear the focus, since we want to focus either initial or the last
    // focusable element in the webpage (according to the TABINDEX), or simply clear
    // the focus.
    clearFocusNode();

    switch (direction) {
    case FocusDirectionForward:
    case FocusDirectionBackward:
        m_page->focusController()->setInitialFocus((WebCore::FocusDirection) direction, 0);
        break;
    case FocusDirectionNone:
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

bool WebPagePrivate::focusField(bool focus)
{
    Frame* frame = focusedOrMainFrame();
    if (!frame)
        return false;

    // If focus is false, try to unfocus the current focused element.
    if (!focus) {
        Document* doc = frame->document();
        if (doc && doc->focusedNode() && doc->focusedNode()->isElementNode())
            static_cast<Element*>(doc->focusedNode())->blur();
    }

    m_page->focusController()->setFocused(focus);

    return true;
}

bool WebPagePrivate::moveToNextField(BlackBerry::Platform::ScrollDirection dir, int desiredScrollAmount)
{
    Frame* frame = focusedOrMainFrame();
    if (!frame)
        return false;

    return moveToNextField(frame, dir, desiredScrollAmount);
}

bool WebPagePrivate::moveToNextField(Frame* frame, BlackBerry::Platform::ScrollDirection dir, int desiredScrollAmount)
{
    Document* doc = frame->document();
    FrameView* view = frame->view();
    if (!doc || !view || view->needsLayout())
        return false;

    // The layout needs to be up-to-date to determine if a node is focusable.
    doc->updateLayoutIgnorePendingStylesheets();

    Node* origFocusedNode = doc->focusedNode();

    Node* startNode = m_page->focusController()->findFocusableNode(FocusDirectionForward, doc, origFocusedNode, 0);

    // Last node? get back to beginning.
    if (!startNode && origFocusedNode)
        startNode = m_page->focusController()->findFocusableNode(FocusDirectionForward, doc, 0, 0);

    if (!startNode || startNode == origFocusedNode)
        return false;

    Node* origRadioGroup = origFocusedNode
        && ((origFocusedNode->hasTagName(HTMLNames::inputTag)
        && static_cast<HTMLInputElement*>(origFocusedNode)->isRadioButton()))
        ? origFocusedNode->parentNode() : 0;

    WebCore::IntRect visibleRect = IntRect(IntPoint(), actualVisibleSize());
    // Constrain the rect if this is a subframe.
    if (view->parent())
        visibleRect.intersect(getRecursiveVisibleWindowRect(view));

    WebCore::IntRect baseRect;
    if (origRadioGroup)
        baseRect = getNodeWindowRect(origRadioGroup);
    else if (origFocusedNode)
        baseRect = getNodeWindowRect(origFocusedNode);
    else {
        baseRect = visibleRect;
        switch (dir) {
        case BlackBerry::Platform::ScrollLeft:
            baseRect.setX(baseRect.maxX());
        case BlackBerry::Platform::ScrollRight:
            baseRect.setWidth(0);
            break;
        case BlackBerry::Platform::ScrollUp:
            baseRect.setY(baseRect.maxY());
        case BlackBerry::Platform::ScrollDown:
            baseRect.setHeight(0);
            break;
        }
    }

    Node* bestChoice = 0;
    NodeDistance bestChoiceDistance;
    Node* lastCandidate = 0;

    WebCore::IntRect bestChoiceRect = baseRect;

    Node* cur = startNode;

    do {
        if (cur->isFocusable()) {
            Node* curRadioGroup = (cur->hasTagName(HTMLNames::inputTag)
                && static_cast<HTMLInputElement*>(cur)->isRadioButton())
                ? cur->parentNode() : 0;
            if (!curRadioGroup || static_cast<HTMLInputElement*>(cur)->checked()) {
                WebCore::IntRect nodeRect = curRadioGroup ? getNodeWindowRect(curRadioGroup) : getNodeWindowRect(cur);
                // Note, if for some reason the nodeRect is 0x0, intersects will not match. This shouldn't be a
                // problem as we don't want to focus on a non-visible node.
                if (visibleRect.intersects(nodeRect)) {
                    NodeDistance nodeDistance;
                    nodeDistance.m_rect = nodeRect;
                    if (tellDistance(dir, baseRect, nodeDistance)) {
                        if (!bestChoice
                            || nodeDistance.isCloser(bestChoiceDistance, dir)) {
                            bestChoice = cur;
                            bestChoiceDistance = nodeDistance;
                            bestChoiceRect = nodeRect;
                        }
                    } else
                        lastCandidate = cur;
                }
            }
        }

        cur = m_page->focusController()->findFocusableNode(FocusDirectionForward, doc, cur, 0);

        // Last node? go back
        if (!cur && origFocusedNode)
            cur = m_page->focusController()->findFocusableNode(FocusDirectionForward, doc, 0, 0);
    } while (cur && cur != origFocusedNode && cur != startNode);

    if (!bestChoice && !origFocusedNode) {
        // In the case that previously no node is on focus.
        bestChoice = lastCandidate;
    }

    if (bestChoice) {
        // FIXME: Temporarily disable automatical scrolling for HTML email.
#if 0
        if (Document* nodeDoc = bestChoice->document()) {
            if (FrameView* nodeView = nodeDoc->view())
                nodeView->scrollRectIntoViewRecursively(bestChoice->getRect());
        }
#endif

        if (bestChoice->isElementNode())
            static_cast<Element*>(bestChoice)->focus(true);
        else
            m_page->focusController()->setFocusedNode(bestChoice, bestChoice->document()->frame());

        // Trigger a repaint of the node rect as it frequently isn't updated due to JavaScript being
        // disabled. See RIM Bug #1242.
        m_backingStore->d->repaint(bestChoiceRect, true /*contentChanged*/, false /*immediate*/);

        return true;
    }

    if (Frame* parent = frame->tree()->parent()) {
        if (moveToNextField(parent, dir, desiredScrollAmount))
            return true;
    }

    return false;
}

BlackBerry::Platform::IntRect WebPagePrivate::focusNodeRect()
{
    Frame* frame = focusedOrMainFrame();
    if (!frame)
        return BlackBerry::Platform::IntRect();

    Document* doc = frame->document();
    FrameView* view = frame->view();
    if (!doc || !view || view->needsLayout())
        return BlackBerry::Platform::IntRect();

    WebCore::IntRect focusRect = rectForNode(doc->focusedNode());
    focusRect = adjustRectOffsetForFrameOffset(focusRect, doc->focusedNode());
    focusRect = mapToTransformed(focusRect);
    clipToTransformedContentsRect(focusRect);
    return focusRect;
}

PassRefPtr<Node> WebPagePrivate::contextNode(TargetDetectionStrategy strategy)
{
    EventHandler* eventHandler = focusedOrMainFrame()->eventHandler();
    const FatFingersResult lastFatFingersResult = m_touchEventHandler->lastFatFingersResult();
    bool isTouching = lastFatFingersResult.isValid() && strategy == RectBased;

    // Unpress the mouse button always.
    if (eventHandler->mousePressed())
        eventHandler->setMousePressed(false);

    // Check if we're using LinkToLink and the user is not touching the screen.
    if (m_webSettings->doesGetFocusNodeContext() && !isTouching) {
        RefPtr<Node> node;
        node = m_page->focusController()->focusedOrMainFrame()->document()->focusedNode();
        if (node) {
            WebCore::IntRect visibleRect = IntRect(IntPoint(), actualVisibleSize());
            if (!visibleRect.intersects(getNodeWindowRect(node.get())))
                return 0;
        }
        return node.release();
    }

    // Check for text input.
    if (isTouching && lastFatFingersResult.isTextInput())
        return lastFatFingersResult.validNode();

    IntPoint contentPos;
    if (isTouching)
        contentPos = lastFatFingersResult.adjustedPosition();
    else
        contentPos = mapFromViewportToContents(m_lastMouseEvent.pos());

    if (strategy == RectBased) {
        FatFingersResult result = FatFingers(this, lastFatFingersResult.adjustedPosition(), FatFingers::Text).findBestPoint();
        return result.validNode();
    }

    HitTestResult result = eventHandler->hitTestResultAtPoint(contentPos, false /*allowShadowContent*/);
    return result.innerNode();
}

static inline int distanceBetweenPoints(WebCore::IntPoint p1, WebCore::IntPoint p2)
{
    // Change int to double, because (dy * dy) can cause int overflow in reality, e.g, (-46709 * -46709).
    double dx = static_cast<double>(p1.x() - p2.x());
    double dy = static_cast<double>(p1.y() - p2.y());
    return sqrt((dx * dx) + (dy * dy));
}

Node* WebPagePrivate::bestNodeForZoomUnderPoint(const WebCore::IntPoint& point)
{
    IntPoint pt = mapFromTransformed(point);
    IntRect clickRect(pt.x() - blockClickRadius, pt.y() - blockClickRadius, 2 * blockClickRadius, 2 * blockClickRadius);
    Node* originalNode = nodeForZoomUnderPoint(point);
    if (!originalNode)
        return 0;
    Node* node = bestChildNodeForClickRect(originalNode, clickRect);
    return node ? adjustedBlockZoomNodeForZoomLimits(node) : adjustedBlockZoomNodeForZoomLimits(originalNode);
}

Node* WebPagePrivate::bestChildNodeForClickRect(Node* parentNode, const WebCore::IntRect& clickRect)
{
    if (!parentNode)
        return 0;

    int bestDistance = std::numeric_limits<int>::max();

    Node* node = parentNode->firstChild();
    Node* bestNode = 0;
    while (node) {
        IntRect rect = rectForNode(node);
        if (clickRect.intersects(rect)) {
            int distance = distanceBetweenPoints(rect.center(), clickRect.center());
            Node* bestChildNode = bestChildNodeForClickRect(node, clickRect);
            if (bestChildNode) {
                IntRect bestChildRect = rectForNode(bestChildNode);
                int bestChildDistance = distanceBetweenPoints(bestChildRect.center(), clickRect.center());
                if (bestChildDistance < distance && bestChildDistance < bestDistance) {
                    bestNode = bestChildNode;
                    bestDistance = bestChildDistance;
                } else {
                    if (distance < bestDistance) {
                        bestNode = node;
                        bestDistance = distance;
                    }
                }
            } else {
                if (distance < bestDistance) {
                    bestNode = node;
                    bestDistance = distance;
                }
            }
        }
        node = node->nextSibling();
    }

    return bestNode;
}

double WebPagePrivate::maxBlockZoomScale() const
{
    return std::min(maximumBlockZoomScale, maximumScale());
}

Node* WebPagePrivate::nodeForZoomUnderPoint(const WebCore::IntPoint& point)
{
    if (!m_mainFrame)
        return 0;

    HitTestResult result = m_mainFrame->eventHandler()->hitTestResultAtPoint(mapFromTransformed(point), false);

    Node* node = result.innerNonSharedNode();

    if (!node)
        return 0;

    RenderObject* renderer = node->renderer();
    while (!renderer) {
        node = node->parentNode();
        renderer = node->renderer();
    }

    return node;
}

Node* WebPagePrivate::adjustedBlockZoomNodeForZoomLimits(Node* node)
{
    Node* initialNode = node;
    RenderObject* renderer = node->renderer();
    bool acceptableNodeSize = newScaleForBlockZoomRect(rectForNode(node), 1.0, 0) < maxBlockZoomScale();

    while (!renderer || !acceptableNodeSize) {
        node = node->parentNode();

        if (!node)
            return initialNode;

        renderer = node->renderer();
        acceptableNodeSize = newScaleForBlockZoomRect(rectForNode(node), 1.0, 0) < maxBlockZoomScale();
    }

    return node;
}

bool WebPagePrivate::compareNodesForBlockZoom(Node* n1, Node* n2)
{
    if (!n1 || !n2)
        return false;

    return (n2 == n1) || n2->isDescendantOf(n1);
}

double WebPagePrivate::newScaleForBlockZoomRect(const WebCore::IntRect& rect, double oldScale, double margin)
{
    if (rect.isEmpty())
        return std::numeric_limits<double>::max();

    ASSERT(rect.width() + margin);

    double newScale = oldScale * static_cast<double>(transformedActualVisibleSize().width()) / (rect.width() + margin);

    return newScale;
}

WebCore::IntRect WebPagePrivate::rectForNode(Node* node)
{
    if (!node)
        return IntRect();

    RenderObject* renderer = node->renderer();

    if (!renderer)
        return IntRect();

    // Return rect in un-transformed content coordinates.
    IntRect blockRect;

    // FIXME: Ensure this works with iframes
    if (m_webPage->settings()->textReflowMode() == WebSettings::TextReflowEnabled && renderer->isText()) {
        RenderBlock* renderBlock = renderer->containingBlock();
        int xOffset = 0;
        int yOffset = 0;
        while (!renderBlock->isRoot()) {
            xOffset += renderBlock->x();
            yOffset += renderBlock->y();
            renderBlock = renderBlock->containingBlock();
        }
        const RenderText* renderText = toRenderText(renderer);
        IntRect linesBox = renderText->linesBoundingBox();
        blockRect = IntRect(xOffset + linesBox.x(), yOffset + linesBox.y(), linesBox.width(), linesBox.height());
    } else
        blockRect = renderer->absoluteClippedOverflowRect();

    if (renderer->isText()) {
        RenderBlock* rb = renderer->containingBlock();

        // Inefficient? way to find width when floats intersect a block
        int blockWidth = 0;
        int lineCount = rb->lineCount();
        for (int i = 0; i < lineCount; i++)
            blockWidth = max(blockWidth, rb->availableLogicalWidthForLine(i, false));

        blockRect.setWidth(blockWidth);
        blockRect.setX(blockRect.x() + rb->logicalLeftOffsetForLine(1, false));
    }

    // Strip off padding.
    if (renderer->style()->hasPadding()) {
        blockRect.setX(blockRect.x() + renderer->style()->paddingLeft().value());
        blockRect.setY(blockRect.y() + renderer->style()->paddingTop().value());
        blockRect.setWidth(blockRect.width() - renderer->style()->paddingRight().value());
        blockRect.setHeight(blockRect.height() - renderer->style()->paddingBottom().value());
    }

    return blockRect;
}

WebCore::IntPoint WebPagePrivate::frameOffset(const Frame* frame) const
{
    ASSERT(frame);

    // FIXME: This function can be called when page is being destroyed and JS triggers selection change.
    // We could break the call chain at upper levels, but I think it is better to check the frame pointer
    // here because the pointer is explicitly cleared in WebPage::destroy().
    if (!mainFrame())
        return WebCore::IntPoint();

    // Convert 0,0 in the frame's coordinate system to window coordinates to
    // get the frame's global position, and return this position in the main
    // frame's coordinates.  (So the main frame's coordinates will be 0,0.)
    return mainFrame()->view()->windowToContents(frame->view()->contentsToWindow(IntPoint::zero()));
}

WebCore::IntRect WebPagePrivate::adjustRectOffsetForFrameOffset(const WebCore::IntRect& rect, const Node* node)
{
    if (!node)
        return rect;

    // Adjust the offset of the rect if it is in an iFrame/frame or set of iFrames/frames
    // FIXME: can we just use frameOffset instead of this big routine?
    const Node* tnode = node;
    IntRect adjustedRect = rect;
    do {
        Frame* frame = tnode->document()->frame();
        if (frame) {
            Node* ownerNode = static_cast<Node*>(frame->ownerElement());
            tnode = ownerNode;
            if (ownerNode && (ownerNode->hasTagName(HTMLNames::iframeTag) || ownerNode->hasTagName(HTMLNames::frameTag))) {
                IntRect iFrameRect;
                do {
                    iFrameRect = rectForNode(ownerNode);
                    adjustedRect.move(iFrameRect.x(), iFrameRect.y());
                    adjustedRect.intersect(iFrameRect);
                    ownerNode = ownerNode->parentNode();
                } while (iFrameRect.isEmpty() && ownerNode);
            } else
                break;
        }
    } while (tnode = tnode->parentNode());

    return adjustedRect;
}

WebCore::IntRect WebPagePrivate::blockZoomRectForNode(Node* node)
{
    if (!node || contentsSize().isEmpty())
        return IntRect();

    Node* tnode = node;
    m_currentBlockZoomAdjustedNode = tnode;

    IntRect blockRect = rectForNode(tnode);

    IntRect originalRect = blockRect;

    int originalArea = originalRect.width() * originalRect.height();
    int pageArea = contentsSize().width() * contentsSize().height();
    double blockToPageRatio = (double)(pageArea - originalArea) / pageArea;
    double blockExpansionRatio = 5.0 * blockToPageRatio * blockToPageRatio;

    if (!tnode->hasTagName(HTMLNames::imgTag) && !tnode->hasTagName(HTMLNames::inputTag) && !tnode->hasTagName(HTMLNames::textareaTag))
        while (tnode = tnode->parentNode()) {
            ASSERT(tnode);
            IntRect tRect = rectForNode(tnode);
            int tempBlockArea = tRect.width() * tRect.height();
            // Don't expand the block if it will be too large relative to the content.
            if ((double)(pageArea - tempBlockArea) / pageArea < 0.15)
                break;
            if (tRect.isEmpty())
                continue; // No renderer.
            if (tempBlockArea < 1.1 * originalArea)
                continue; // The size of this parent is very close to the child, no need to go to this parent.
            if (tempBlockArea < blockExpansionRatio * originalArea) {
                blockRect = tRect;
                m_currentBlockZoomAdjustedNode = tnode;
            } else
                break;
        }

    blockRect = adjustRectOffsetForFrameOffset(blockRect, node);
    blockRect = mapToTransformed(blockRect);
    clipToTransformedContentsRect(blockRect);

#if DEBUG_BLOCK_ZOOM
    // Re-paint the backingstore to screen to erase other annotations.
    m_backingStore->d->resumeScreenAndBackingStoreUpdates(BackingStore::Blit);

    // Render a black square over the calculated block and a gray square over the original block for visual inspection.
    originalRect = mapToTransformed(originalRect);
    clipToTransformedContentsRect(originalRect);
    IntRect renderRect = mapFromTransformedContentsToTransformedViewport(blockRect);
    IntRect originalRenderRect = mapFromTransformedContentsToTransformedViewport(originalRect);
    IntSize viewportSize = transformedViewportSize();
    renderRect.intersect(IntRect(0, 0, viewportSize.width(), viewportSize.height()));
    originalRenderRect.intersect(IntRect(0, 0, viewportSize.width(), viewportSize.height()));
    m_backingStore->d->clearWindow(renderRect, 0, 0, 0);
    m_backingStore->d->clearWindow(originalRenderRect, 120, 120, 120);
    m_backingStore->d->invalidateWindow(renderRect);
#endif

    return blockRect;
}

void WebPage::blockZoomAnimationFinished()
{
    d->zoomBlock();
}

// This function should not be called directly.
// It is called after the animation ends (see above).
void WebPagePrivate::zoomBlock()
{
    if (!m_mainFrame)
        return;

    IntPoint anchor(roundUntransformedPoint(mapFromTransformedFloatPoint(m_finalBlockPoint)));
    bool willUseTextReflow = false;

#if ENABLE(VIEWPORT_REFLOW)
    willUseTextReflow = m_webPage->settings()->textReflowMode() != WebSettings::TextReflowDisabled;
    toggleTextReflowIfTextReflowEnabledOnlyForBlockZoom(m_shouldReflowBlock);
    setNeedsLayout();
#endif

    TransformationMatrix zoom;
    zoom.scale(m_blockZoomFinalScale);
    *m_transformationMatrix = zoom;
    m_client->resetBitmapZoomScale(m_blockZoomFinalScale);
    m_backingStore->d->suspendScreenAndBackingStoreUpdates();
    updateViewportSize();

#if ENABLE(VIEWPORT_REFLOW)
    requestLayoutIfNeeded();
    if (willUseTextReflow && m_shouldReflowBlock) {
        IntRect reflowedRect = rectForNode(m_currentBlockZoomAdjustedNode.get());
        reflowedRect = adjustRectOffsetForFrameOffset(reflowedRect, m_currentBlockZoomAdjustedNode.get());
        reflowedRect.move(roundTransformedPoint(m_finalBlockPointReflowOffset).x(), roundTransformedPoint(m_finalBlockPointReflowOffset).y());
        RenderObject* renderer = m_currentBlockZoomAdjustedNode->renderer();
        IntPoint topLeftPoint(reflowedRect.location());
        if (renderer && renderer->isText()) {
            ETextAlign textAlign = renderer->style()->textAlign();
            IntPoint textAnchor;
            switch (textAlign) {
            case CENTER:
            case WEBKIT_CENTER:
                textAnchor = IntPoint(reflowedRect.x() + (reflowedRect.width() - actualVisibleSize().width()) / 2, topLeftPoint.y());
                break;
            case LEFT:
            case WEBKIT_LEFT:
                textAnchor = topLeftPoint;
                break;
            case RIGHT:
            case WEBKIT_RIGHT:
                textAnchor = IntPoint(reflowedRect.x() + reflowedRect.width() - actualVisibleSize().width(), topLeftPoint.y());
                break;
            case TAAUTO:
            case JUSTIFY:
            default:
                if (renderer->style()->isLeftToRightDirection())
                    textAnchor = topLeftPoint;
                else
                    textAnchor = IntPoint(reflowedRect.x() + reflowedRect.width() - actualVisibleSize().width(), topLeftPoint.y());
                break;
            }
            setScrollPosition(textAnchor);
        } else
            renderer->style()->isLeftToRightDirection() ? setScrollPosition(topLeftPoint) : setScrollPosition(IntPoint(reflowedRect.x() + reflowedRect.width() - actualVisibleSize().width(), topLeftPoint.y()));
    } else if (willUseTextReflow) {
        IntRect finalRect = rectForNode(m_currentBlockZoomAdjustedNode.get());
        finalRect = adjustRectOffsetForFrameOffset(finalRect, m_currentBlockZoomAdjustedNode.get());
        setScrollPosition(IntPoint(0, finalRect.y() + m_finalBlockPointReflowOffset.y()));
        resetBlockZoom();
    }
#endif
    if (!willUseTextReflow) {
        setScrollPosition(anchor);
        if (!m_shouldReflowBlock)
            resetBlockZoom();
    }

    notifyTransformChanged();
    m_backingStore->d->clearWindow();
    m_backingStore->d->resumeScreenAndBackingStoreUpdates(BackingStore::RenderAndBlit);
    m_client->zoomChanged(m_webPage->isMinZoomed(), m_webPage->isMaxZoomed(), !shouldZoomOnEscape(), currentScale());
}


void WebPagePrivate::resetBlockZoom()
{
    m_currentBlockZoomNode = 0;
    m_currentBlockZoomAdjustedNode = 0;
    m_shouldReflowBlock = false;
}

WebPage::WebPage(WebPageClient* client, const WebString& pageGroupName, const Platform::IntRect& rect)
{
    globalInitialize();
    d = new WebPagePrivate(this, client, rect);
    d->init(pageGroupName);
}

void WebPage::destroyWebPageCompositor()
{
#if USE(ACCELERATED_COMPOSITING)
    // Destroy the layer renderer in a sync command before we destroy the backing store,
    // to flush any pending compositing messages on the compositing thread.
    // The backing store is indirectly deleted by the 'detachFromParent' call below.
    d->syncDestroyCompositorOnCompositingThread();
#endif
}

void WebPage::destroy()
{
    // TODO: need to verify if this call needs to be made before calling
    // WebPage::destroyWebPageCompositor()
    d->m_backingStore->d->suspendScreenAndBackingStoreUpdates();

    // Close the backforward list and release the cached pages.
    d->m_page->backForward()->close();
    pageCache()->releaseAutoreleasedPagesNow();

    FrameLoader* loader = d->m_mainFrame->loader();

    // Remove main frame's backing store client from the map
    // to prevent FrameLoaderClientBlackyBerry::detachFromParent2(),
    // which is called by loader->detachFromParent(), deleting it.
    // We will delete it in ~WebPagePrivate().
    // Reason: loader->detachFromParent() may ping back to backing store
    // indirectly through ChromeClientBlackBerry::invalidateContentsAndWindow().
    // see RIM PR #93256.
    d->removeBackingStoreClientForFrame(d->m_mainFrame);

    // Set m_mainFrame to 0 to avoid calls back in to the backingstore during webpage deletion.
    d->m_mainFrame = 0;
    if (loader)
        loader->detachFromParent();

    delete this;
}

WebPage::~WebPage()
{
    delete d;
    d = 0;
}

WebPageClient* WebPage::client() const
{
    return d->m_client;
}

void WebPage::load(const char* url, const char* networkToken, bool isInitial)
{
    d->load(url, networkToken, "GET", Platform::NetworkRequest::UseProtocolCachePolicy, 0, 0, 0, 0, isInitial, false);
}

void WebPage::loadExtended(const char* url, const char* networkToken, const char* method, Platform::NetworkRequest::CachePolicy cachePolicy, const char* data, size_t dataLength, const char* const* headers, size_t headersLength, bool mustHandleInternally)
{
    d->load(url, networkToken, method, cachePolicy, data, dataLength, headers, headersLength, false, mustHandleInternally, false, "");
}

void WebPage::loadFile(const char* path, const char* overrideContentType)
{
    std::string fileUrl(path);
    if (!fileUrl.find("/"))
        fileUrl.insert(0, "file://");
    else if (fileUrl.find("file:///"))
        return;

    d->load(fileUrl.c_str(), 0, "GET", Platform::NetworkRequest::UseProtocolCachePolicy, 0, 0, 0, 0, false, false, false, overrideContentType);
}

void WebPage::loadString(const char* string, const char* baseURL, const char* mimeType)
{
    d->loadString(string, baseURL, mimeType);
}

void WebPage::download(const Platform::NetworkRequest& request)
{
    d->load(request.getUrlRef().c_str(), 0, "GET", Platform::NetworkRequest::UseProtocolCachePolicy, 0, 0, 0, 0, false, false, true, "");
}

bool WebPage::executeJavaScript(const char* script, JavaScriptDataType& returnType, WebString& returnValue)
{
    return d->executeJavaScript(script, returnType, returnValue);
}

bool WebPage::executeJavaScriptInIsolatedWorld(const std::wstring& script, JavaScriptDataType& returnType, WebString& returnValue)
{
    // On our platform wchar_t is unsigned int and UChar is unsigned short
    // so we have to convert using ICU conversion function
    int lengthCopied = 0;
    UErrorCode error = U_ZERO_ERROR;
    const int length = script.length() + 1 /*null termination char*/;
    UChar data[length];

    // FIXME: PR 138162 is giving U_INVALID_CHAR_FOUND error
    u_strFromUTF32(data, length, &lengthCopied, reinterpret_cast<const UChar32*>(script.c_str()), script.length(), &error);
    BLACKBERRY_ASSERT(error == U_ZERO_ERROR);
    if (error != U_ZERO_ERROR) {
        BlackBerry::Platform::logAlways(BlackBerry::Platform::LogLevelCritical, "WebPage::executeJavaScriptInIsolatedWorld failed to convert UTF16 to JavaScript!");
        return false;
    }
    WTF::String str = WTF::String(data, lengthCopied);
    WebCore::ScriptSourceCode sourceCode(str, KURL());
    return d->executeJavaScriptInIsolatedWorld(sourceCode, returnType, returnValue);
}

bool WebPage::executeJavaScriptInIsolatedWorld(const char* script, JavaScriptDataType& returnType, WebString& returnValue)
{
    WebCore::ScriptSourceCode sourceCode(WTF::String::fromUTF8(script), KURL());
    return d->executeJavaScriptInIsolatedWorld(sourceCode, returnType, returnValue);
}

void WebPage::stopLoading()
{
    d->stopCurrentLoad();
}

void WebPage::prepareToDestroy()
{
    d->prepareToDestroy();
}

int WebPage::backForwardListLength() const
{
    return d->m_page->getHistoryLength();
}

bool WebPage::canGoBackOrForward(int delta) const
{
    return d->m_page->canGoBackOrForward(delta);
}

bool WebPage::goBackOrForward(int delta)
{
    if (d->m_page->canGoBackOrForward(delta)) {
        d->m_page->goBackOrForward(delta);
        return true;
    }
    return false;
}

void WebPage::goToBackForwardEntry(BackForwardId id)
{
    HistoryItem* item = historyItemFromBackForwardId(id);
    ASSERT(item);
    d->m_page->goToItem(item, FrameLoadTypeIndexedBackForward);
}

void WebPage::reload()
{
    d->m_mainFrame->loader()->reload(/* bypassCache */ true);
}

void WebPage::reloadFromCache()
{
    d->m_mainFrame->loader()->reload(/* bypassCache */ false);
}

WebSettings* WebPage::settings() const
{
    return d->m_webSettings;
}

bool WebPage::isVisible() const
{
    return d->m_visible;
}

void WebPage::setVisible(bool visible)
{
    if (d->m_visible == visible)
        return;

    d->m_visible = visible;

    if (!visible) {
        d->suspendBackingStore();

        // Remove this WebPage from the visible pages list.
        size_t foundIndex = visibleWebPages()->find(this);
        if (foundIndex != WTF::notFound)
            visibleWebPages()->remove(foundIndex);

        // Return the backing store to the last visible WebPage.
        if (BackingStorePrivate::currentBackingStoreOwner() == this && !visibleWebPages()->isEmpty())
            visibleWebPages()->last()->d->resumeBackingStore();

#if USE(ACCELERATED_COMPOSITING)
        // Root layer commit is not necessary for invisible tabs.
        // And release layer resources can reduce memory consumption.
        d->suspendRootLayerCommit();
#endif
        return;
    }

#if USE(ACCELERATED_COMPOSITING)
    d->resumeRootLayerCommit();
#endif

    // Push this WebPage to the top of the visible pages list.
    if (!visibleWebPages()->isEmpty() && visibleWebPages()->last() != this) {
        size_t foundIndex = visibleWebPages()->find(this);
        if (foundIndex != WTF::notFound)
            visibleWebPages()->remove(foundIndex);
    }
    visibleWebPages()->append(this);

    if (BackingStorePrivate::currentBackingStoreOwner()
        && BackingStorePrivate::currentBackingStoreOwner() != this)
        BackingStorePrivate::currentBackingStoreOwner()->d->suspendBackingStore();

    // resumeBackingStore will set the current owner to this webpage.
    // If we set the owner prematurely, then the tiles will not be reset.
    d->resumeBackingStore();
}

void WebPagePrivate::selectionChanged(Frame* frame)
{
    m_inputHandler->selectionChanged();

    // FIXME: This is a hack!
    // To ensure the selection being changed has its frame 'focused', lets
    // set it as focused ourselves (PR #104724).
    m_page->focusController()->setFocusedFrame(frame);
}

void WebPagePrivate::updateDelegatedOverlays(bool dispatched)
{
    // Track a dispatched message, we don't want to flood the webkit thread.
    // There can be as many as one more message enqued as needed but never less.
    if (dispatched)
        m_updateDelegatedOverlaysDispatched = false;
    else if (m_updateDelegatedOverlaysDispatched) {
        // Early return if there is message already pending on the webkit thread.
        return;
    }

    if (BlackBerry::Platform::webKitThreadMessageClient()->isCurrentThread()) {
        // Must be called on the WebKit thread.
        if (m_selectionHandler->isSelectionActive())
            m_selectionHandler->selectionPositionChanged(true /*visualChangeOnly*/);

    } else if (m_selectionHandler->isSelectionActive()) {
        // Don't bother dispatching to webkit thread if selection and tap highlight are not active.
        m_updateDelegatedOverlaysDispatched = true;
        BlackBerry::Platform::webKitThreadMessageClient()->dispatchMessage(BlackBerry::Platform::createMethodCallMessage(&WebPagePrivate::updateDelegatedOverlays, this, true /*dispatched*/));
    }
}

void WebPage::setCaretHighlightStyle(BlackBerry::Platform::CaretHighlightStyle style)
{
}

bool WebPage::setBatchEditingActive(bool active)
{
    return d->m_inputHandler->setBatchEditingActive(active);
}

bool WebPage::setInputSelection(unsigned int start, unsigned int end)
{
    return d->m_inputHandler->setSelection(start, end);
}

int WebPage::inputCaretPosition() const
{
    return d->m_inputHandler->caretPosition();
}

void WebPage::popupListClosed(int size, bool* selecteds)
{
    d->m_inputHandler->setPopupListIndexes(size, selecteds);
}

void WebPage::popupListClosed(int index)
{
    d->m_inputHandler->setPopupListIndex(index);
}

void WebPage::setDateTimeInput(const WebString& value)
{
    d->m_inputHandler->setInputValue(String(value.impl()));
}

void WebPage::setColorInput(const WebString& value)
{
    d->m_inputHandler->setInputValue(String(value.impl()));
}

ActiveNodeContext WebPage::activeNodeContext(TargetDetectionStrategy strategy) const
{
    return d->activeNodeContext(strategy);
}

void WebPage::setVirtualViewportSize(int width, int height)
{
    d->m_virtualViewportWidth = width;
    d->m_virtualViewportHeight = height;
}

void WebPage::resetVirtualViewportOnCommitted(bool reset)
{
    d->m_resetVirtualViewportOnCommitted = reset;
}

WebCore::IntSize WebPagePrivate::recomputeVirtualViewportFromViewportArguments()
{
    static ViewportArguments defaultViewportArguments;
    if (m_viewportArguments == defaultViewportArguments)
        return IntSize();

    int desktopWidth = defaultMaxLayoutSize().width();
    int deviceWidth = BlackBerry::Platform::Graphics::Screen::width();
    int deviceHeight = BlackBerry::Platform::Graphics::Screen::height();
    FloatSize currentPPI = BlackBerry::Platform::Graphics::Screen::pixelsPerInch(-1);
    int deviceDPI = int(roundf((currentPPI.width() + currentPPI.height()) / 2));
    if (m_viewportArguments.targetDensityDpi == ViewportArguments::ValueAuto) {
        // Auto means 160dpi if we leave it alone. This looks terrible for pages wanting 1:1.
        // FIXME: This is insufficient for devices with high dpi, as they will render content unreadably small.
        m_viewportArguments.targetDensityDpi = deviceDPI;
    }

    ViewportAttributes result = WebCore::computeViewportAttributes(m_viewportArguments, desktopWidth, deviceWidth, deviceHeight, deviceDPI, m_defaultLayoutSize);
    return IntSize(result.layoutSize.width(), result.layoutSize.height());
}

#if ENABLE(EVENT_MODE_METATAGS)
void WebPagePrivate::didReceiveCursorEventMode(WebCore::CursorEventMode mode)
{
    if (mode != m_cursorEventMode)
        m_client->cursorEventModeChanged(toPlatformCursorEventMode(mode));
    m_cursorEventMode = mode;
}

void WebPagePrivate::didReceiveTouchEventMode(WebCore::TouchEventMode mode)
{
    if (mode != m_touchEventMode)
        m_client->touchEventModeChanged(toPlatformTouchEventMode(mode));
    m_touchEventMode = mode;
}
#endif

void WebPagePrivate::dispatchViewportPropertiesDidChange(const WebCore::ViewportArguments& arguments)
{
    static ViewportArguments defaultViewportArguments;
    if (arguments == defaultViewportArguments)
        return;

    m_viewportArguments = arguments;

    // 0 width or height in viewport arguments makes no sense, and results in a very large initial scale.
    // In real world, a 0 width or height is usually caused by a syntax error in "content" field of viewport
    // meta tag, for example, using semicolon instead of comma as separator ("width=device-width; initial-scale=1.0").
    // We don't have a plan to tolerate the semicolon separator, but we can avoid applying 0 width/height.
    // I default it to ValueDeviceWidth rather than ValueAuto because in more cases the web site wants "device-width"
    // when they specify the viewport width.
    if (!m_viewportArguments.width)
        m_viewportArguments.width = ViewportArguments::ValueDeviceWidth;
    if (!m_viewportArguments.height)
        m_viewportArguments.height = ViewportArguments::ValueDeviceHeight;

    setUserScalable(arguments.userScalable == ViewportArguments::ValueAuto ? true : arguments.userScalable);
    if (arguments.initialScale > 0)
        setInitialScale(arguments.initialScale);
    if (arguments.minimumScale > 0)
        setMinimumScale(arguments.minimumScale);
    if (arguments.maximumScale > 0)
        setMaximumScale(arguments.maximumScale);

    IntSize virtualViewport = recomputeVirtualViewportFromViewportArguments();
    m_webPage->setVirtualViewportSize(virtualViewport.width(), virtualViewport.height());

    if (loadState() == BlackBerry::WebKit::WebPagePrivate::Committed)
        zoomToInitialScaleOnLoad();
}

void WebPagePrivate::onInputLocaleChanged(bool isRTL)
{
    if (isRTL != m_webSettings->isWritingDirectionRTL()) {
        m_webSettings->setWritingDirectionRTL(isRTL);
        m_inputHandler->handleInputLocaleChanged(isRTL);
    }
}

void WebPage::setScreenOrientation(int orientation)
{
    d->m_pendingOrientation = orientation;
}

void WebPage::applyPendingOrientationIfNeeded()
{
    if (d->m_pendingOrientation != -1)
        d->setScreenOrientation(d->m_pendingOrientation);
}

void WebPagePrivate::suspendBackingStore()
{
#if USE(ACCELERATED_COMPOSITING)
    resetCompositingSurface();
#endif
}

void WebPagePrivate::resumeBackingStore()
{
    ASSERT(m_webPage->isVisible());

#if USE(ACCELERATED_COMPOSITING)
    setNeedsOneShotDrawingSynchronization();
#endif

    bool directRendering = m_backingStore->d->shouldDirectRenderingToWindow();
    if (!m_backingStore->d->isActive()
        || shouldResetTilesWhenShown()
        || directRendering) {
        // We need to reset all tiles so that we do not show any tiles whose content may
        // have been replaced by another WebPage instance (i.e. another tab).
        BackingStorePrivate::setCurrentBackingStoreOwner(m_webPage);
        m_backingStore->d->orientationChanged(); // Updates tile geometry and creates visible tile buffer
        m_backingStore->d->resetTiles(true /*resetBackground*/);
        m_backingStore->d->updateTiles(false /*updateVisible*/, false /*immediate*/);
        // This value may have changed, so we need to update it.
        directRendering = m_backingStore->d->shouldDirectRenderingToWindow();
        if (m_backingStore->d->renderVisibleContents() && !m_backingStore->d->isSuspended() && !directRendering)
            m_backingStore->d->blitVisibleContents();
    } else {
        // Rendering was disabled while we were hidden, so we need to update all tiles.
        m_backingStore->d->updateTiles(true /*updateVisible*/, false /*immediate*/);
    }

    setShouldResetTilesWhenShown(false);
}

void WebPagePrivate::setScreenOrientation(int orientation)
{
    HashSet<PluginView*>::const_iterator it = m_pluginViews.begin();
    HashSet<PluginView*>::const_iterator last = m_pluginViews.end();
    for (; it != last; ++it)
        (*it)->handleOrientationEvent(orientation);

    m_pendingOrientation = -1;

#if ENABLE(ORIENTATION_EVENTS)
    if (m_mainFrame->orientation() == orientation)
        return;
    for (RefPtr<Frame> frame = m_mainFrame; frame; frame = frame->tree()->traverseNext())
        frame->sendOrientationChangeEvent(orientation);
#endif
}

Platform::IntSize WebPage::viewportSize() const
{
    return d->transformedActualVisibleSize();
}

void WebPage::setViewportSize(const BlackBerry::Platform::IntSize& viewportSize, bool ensureFocusElementVisible)
{
    d->setViewportSize(viewportSize, ensureFocusElementVisible);
}

void WebPagePrivate::screenRotated()
{
    // This call will cause the client to reallocate the window buffer to new size,
    // which needs to be serialized with usage of the window buffer. Accomplish
    // this by sending a sync message to the compositing thread. All other usage of
    // the window buffer happens on the compositing thread.
    if (!BlackBerry::Platform::userInterfaceThreadMessageClient()->isCurrentThread()) {
        BlackBerry::Platform::userInterfaceThreadMessageClient()->dispatchSyncMessage(
            BlackBerry::Platform::createMethodCallMessage(
                &WebPagePrivate::screenRotated, this));
        return;
    }

    SurfacePool::globalSurfacePool()->notifyScreenRotated();
    m_client->notifyScreenRotated();
}

void WebPagePrivate::setViewportSize(const WebCore::IntSize& transformedActualVisibleSize, bool ensureFocusElementVisible)
{
    if (m_pendingOrientation == -1 && transformedActualVisibleSize == this->transformedActualVisibleSize())
        return;

    // Suspend all screen updates to the backingstore to make sure no-one tries to blit
    // while the window surface and the BackingStore are out of sync.
    m_backingStore->d->suspendScreenAndBackingStoreUpdates();

    // The screen rotation is a major state transition that in this case is not properly
    // communicated to the backing store, since it does early return in most methods when
    // not visible.
    if (!m_visible || !m_backingStore->d->isActive())
        setShouldResetTilesWhenShown(true);

    bool hasPendingOrientation = m_pendingOrientation != -1;
    if (hasPendingOrientation)
        screenRotated();

    // The window buffers might have been recreated, cleared, moved, etc., so:
    m_backingStore->d->windowFrontBufferState()->clearBlittedRegion();
    m_backingStore->d->windowBackBufferState()->clearBlittedRegion();

    IntSize viewportSizeBefore = actualVisibleSize();
    FloatPoint centerOfVisibleContentsRect = this->centerOfVisibleContentsRect();
    bool newVisibleRectContainsOldVisibleRect = (m_actualVisibleHeight <= transformedActualVisibleSize.height())
                                          && (m_actualVisibleWidth <= transformedActualVisibleSize.width());

    bool atInitialScale = currentScale() == initialScale();
    bool atTop = !scrollPosition().y();
    bool atLeft = !scrollPosition().x();

    // We need to reorient the visibleTileRect because the following code
    // could cause BackingStore::transformChanged to be called, where it
    // is used.
    // It is only dependent on the transformedViewportSize which has been
    // updated by now.
    m_backingStore->d->createVisibleTileBuffer();

    setDefaultLayoutSize(transformedActualVisibleSize);

    // Recompute our virtual viewport.
    bool needsLayout = false;
    static ViewportArguments defaultViewportArguments;
    if (!(m_viewportArguments == defaultViewportArguments)) {
        // We may need to infer the width and height for the viewport with respect to the rotation.
        IntSize newVirtualViewport = recomputeVirtualViewportFromViewportArguments();
        ASSERT(!newVirtualViewport.isEmpty());
        m_webPage->setVirtualViewportSize(newVirtualViewport.width(), newVirtualViewport.height());
        m_mainFrame->view()->setUseFixedLayout(useFixedLayout());
        m_mainFrame->view()->setFixedLayoutSize(fixedLayoutSize());
        needsLayout = true;
    }

    // We switch this strictly after recomputing our virtual viewport as zoomToFitScale is dependent
    // upon these values and so is the virtual viewport recalculation.
    m_actualVisibleWidth = transformedActualVisibleSize.width();
    m_actualVisibleHeight = transformedActualVisibleSize.height();

    IntSize viewportSizeAfter = actualVisibleSize();

    IntPoint offset(roundf((viewportSizeBefore.width() - viewportSizeAfter.width()) / 2.0),
                    roundf((viewportSizeBefore.height() - viewportSizeAfter.height()) / 2.0));

    // As a special case, if we were anchored to the top left position at
    // the beginning of the rotation then preserve that anchor.
    if (atTop)
        offset.setY(0);
    if (atLeft)
        offset.setX(0);

    // If we're about to overscroll, cap the offset to valid content.
    IntPoint bottomRight(
        scrollPosition().x() + viewportSizeAfter.width(),
        scrollPosition().y() + viewportSizeAfter.height());

    if (bottomRight.x() + offset.x() > contentsSize().width())
        offset.setX(contentsSize().width() - bottomRight.x());
    if (bottomRight.y() + offset.y() > contentsSize().height())
        offset.setY(contentsSize().height() - bottomRight.y());
    if (scrollPosition().x() + offset.x() < 0)
        offset.setX(-scrollPosition().x());
    if (scrollPosition().y() + offset.y() < 0)
        offset.setY(-scrollPosition().y());

    // ...before scrolling, because the backing store will align its
    // tile matrix with the viewport as reported by the ScrollView.
    scrollBy(offset.x(), offset.y());
    notifyTransformedScrollChanged();

    m_backingStore->d->orientationChanged();
    m_backingStore->d->actualVisibleSizeChanged(transformedActualVisibleSize);

    // Update view mode only after we have updated the actual
    // visible size and reset the contents rect if necessary.
    if (setViewMode(viewMode()))
        needsLayout = true;

    // We need to update the viewport size of the WebCore::ScrollView...
    updateViewportSize(!hasPendingOrientation /*setFixedReportedSize*/, false /*sendResizeEvent*/);
    notifyTransformedContentsSizeChanged();

    // If automatic zooming is disabled, prevent zooming below.
    if (!m_webSettings->isZoomToFitOnLoad()) {
        atInitialScale = false;

        // Normally, if the contents size is smaller than the layout width,
        // we would zoom in. If zoom is disabled, we need to do something else,
        // or there will be artifacts due to non-rendered areas outside of the
        // contents size. If there is a virtual viewport, we are not allowed
        // to modify the fixed layout size, however.
        if (!hasVirtualViewport() && contentsSize().width() < m_defaultLayoutSize.width()) {
            m_mainFrame->view()->setUseFixedLayout(useFixedLayout());
            m_mainFrame->view()->setFixedLayoutSize(m_defaultLayoutSize);
            needsLayout = true;
        }
    }

    if (needsLayout)
        setNeedsLayout();

    // Need to resume so that the backingstore will start recording the invalidated
    // rects from below.
    m_backingStore->d->resumeScreenAndBackingStoreUpdates(BackingStore::None);

    // We might need to layout here to get a correct contentsSize so that zoomToFit
    // is calculated correctly.
    requestLayoutIfNeeded();

    // As a special case if we were zoomed to the initial scale at the beginning
    // of the rotation then preserve that zoom level even when it is zoomToFit.
    double scale = atInitialScale ? initialScale() : currentScale();

    // Do our own clamping.
    scale = clampedScale(scale);

    if (hasPendingOrientation) {
        // Set the fixed reported size here so that innerWidth|innerHeight works
        // with this new scale.
        TransformationMatrix rotationMatrix;
        rotationMatrix.scale(scale);
        IntRect viewportRect = IntRect(IntPoint(0, 0), transformedActualVisibleSize);
        IntRect actualVisibleRect
            = enclosingIntRect(rotationMatrix.inverse().mapRect(FloatRect(viewportRect)));
        m_mainFrame->view()->setFixedReportedSize(actualVisibleRect.size());
    }

    // We're going to need to send a resize event to JavaScript because
    // innerWidth and innerHeight depend on fixed reported size.
    // This is how we support mobile pages where JavaScript resizes
    // the page in order to get around the fixed layout size, e.g.
    // google maps when it detects a mobile user agent.
    if (shouldSendResizeEvent())
        m_mainFrame->eventHandler()->sendResizeEvent();

    // As a special case if we were anchored to the top left position at the beginning
    // of the rotation then preserve that anchor.
    FloatPoint anchor = centerOfVisibleContentsRect;
    if (atTop)
        anchor.setY(0);
    if (atLeft)
        anchor.setX(0);

    // Try and zoom here with clamping on.
    if (m_backingStore->d->shouldDirectRenderingToWindow()) {
        bool success = zoomAboutPoint(scale, anchor, false /*enforceScaleClamping*/, true /*forceRendering*/);
        if (!success && ensureFocusElementVisible)
            ensureContentVisible(!newVisibleRectContainsOldVisibleRect);
    } else if (!scheduleZoomAboutPoint(scale, anchor, false /*enforceScaleClamping*/, true /*forceRendering*/)) {
        // Suspend all screen updates to the backingstore.
        m_backingStore->d->suspendScreenAndBackingStoreUpdates();

        // If the zoom failed, then we should still preserve the special case of scroll position.
        IntPoint scrollPosition = this->scrollPosition();
        if (atTop)
            scrollPosition.setY(0);
        if (atLeft)
            scrollPosition.setX(0);
        setScrollPosition(scrollPosition);

        // These might have been altered even if we didn't zoom so notify the client.
        notifyTransformedContentsSizeChanged();
        notifyTransformedScrollChanged();

        if (!needsLayout) {
            // The visible tiles for scroll must be up-to-date before we blit since we are not performing a layout.
            m_backingStore->d->updateTilesForScrollOrNotRenderedRegion();
        }

        if (ensureFocusElementVisible)
            ensureContentVisible(!newVisibleRectContainsOldVisibleRect);

        if (needsLayout) {
            m_backingStore->d->resetTiles(true);
            m_backingStore->d->updateTiles(false /*updateVisible*/, false /*immediate*/);
        }

        // If we need layout then render and blit, otherwise just blit as our viewport has changed.
        m_backingStore->d->resumeScreenAndBackingStoreUpdates(needsLayout ? BackingStore::RenderAndBlit : BackingStore::Blit);
    } else if (ensureFocusElementVisible)
        ensureContentVisible(!newVisibleRectContainsOldVisibleRect);
}

void WebPage::setDefaultLayoutSize(int width, int height)
{
    IntSize size(width, height);
    if (size == d->m_defaultLayoutSize)
        return;

    d->setDefaultLayoutSize(size);
    bool needsLayout = d->setViewMode(d->viewMode());
    if (needsLayout) {
        d->setNeedsLayout();
        if (!d->isLoading())
            d->requestLayoutIfNeeded();
    }
}

void WebPagePrivate::setDefaultLayoutSize(const WebCore::IntSize& size)
{
    IntSize screenSize = BlackBerry::Platform::Graphics::Screen::size();
    ASSERT(size.width() <= screenSize.width() && size.height() <= screenSize.height());
    m_defaultLayoutSize = size.expandedTo(minimumLayoutSize).shrunkTo(screenSize);
}

bool WebPage::mouseEvent(const BlackBerry::Platform::MouseEvent& mouseEvent, bool* wheelDeltaAccepted)
{
    if (!d->m_mainFrame->view())
        return false;

    PluginView* pluginView = d->m_fullScreenPluginView.get();
    if (pluginView)
        return d->dispatchMouseEventToFullScreenPlugin(pluginView, mouseEvent);

    if (mouseEvent.type() == BlackBerry::Platform::MouseEvent::MouseAborted) {
        d->m_mainFrame->eventHandler()->setMousePressed(false);
        return false;
    }

    d->m_pluginMayOpenNewTab = true;

    d->m_lastUserEventTimestamp = currentTime();
    int clickCount = (d->m_selectionHandler->isSelectionActive() || mouseEvent.type() != BlackBerry::Platform::MouseEvent::MouseMove) ? 1 : 0;

    // Set the button type.
    WebCore::MouseButton buttonType = NoButton;
    if (mouseEvent.isLeftButton())
        buttonType = LeftButton;
    else if (mouseEvent.isRightButton())
        buttonType = RightButton;
    else if (mouseEvent.isMiddleButton())
        buttonType = MiddleButton;

    // Create our event.
    WebCore::PlatformMouseEvent platformMouseEvent(d->mapFromTransformed(mouseEvent.position()),
                                                   d->mapFromTransformed(mouseEvent.screenPosition()),
                                                   toWebCoreMouseEventType(mouseEvent.type()), clickCount, buttonType, PointingDevice);
    d->m_lastMouseEvent = platformMouseEvent;
    bool success = d->handleMouseEvent(platformMouseEvent);

    if (mouseEvent.wheelTicks()) {
        WebCore::PlatformWheelEvent wheelEvent(d->mapFromTransformed(mouseEvent.position()),
                                               d->mapFromTransformed(mouseEvent.screenPosition()),
                                               0, -mouseEvent.wheelDelta(),
                                               0, -mouseEvent.wheelTicks(),
                                               WebCore::ScrollByPixelWheelEvent,
                                               false, false, false, false);
        if (wheelDeltaAccepted)
            *wheelDeltaAccepted = d->handleWheelEvent(wheelEvent);
    } else if (wheelDeltaAccepted)
        *wheelDeltaAccepted = false;

    return success;
}

bool WebPagePrivate::dispatchMouseEventToFullScreenPlugin(PluginView* plugin, const BlackBerry::Platform::MouseEvent& event)
{
    NPEvent npEvent;
    NPMouseEvent mouseEvent;

    mouseEvent.x = event.screenPosition().x();
    mouseEvent.y = event.screenPosition().y();

    switch (event.type()) {
    case BlackBerry::Platform::MouseEvent::MouseButtonDown:
        mouseEvent.type = MOUSE_BUTTON_DOWN;
        m_pluginMouseButtonPressed = true;
        break;
    case BlackBerry::Platform::MouseEvent::MouseButtonUp:
        mouseEvent.type = MOUSE_BUTTON_UP;
        m_pluginMouseButtonPressed = false;
        break;
    case BlackBerry::Platform::MouseEvent::MouseMove:
        mouseEvent.type = MOUSE_MOTION;
        break;
    default:
        return false;
    }

    mouseEvent.flags = 0;
    mouseEvent.button = m_pluginMouseButtonPressed;

    npEvent.type = NP_MouseEvent;
    npEvent.data = &mouseEvent;

    return plugin->dispatchFullScreenNPEvent(npEvent);
}

void WebPage::setVirtualKeyboardVisible(bool visible)
{
    d->showVirtualKeyboard(visible);
}

bool WebPagePrivate::handleMouseEvent(WebCore::PlatformMouseEvent& mouseEvent)
{
    WebCore::EventHandler* eventHandler = m_mainFrame->eventHandler();

    if (mouseEvent.eventType() == WebCore::MouseEventMoved)
        return eventHandler->mouseMoved(mouseEvent);

    if (mouseEvent.eventType() == WebCore::MouseEventScroll)
        return true;

    Node* node = 0;
    if (mouseEvent.inputMethod() == TouchScreen) {
        const FatFingersResult lastFatFingersResult = m_touchEventHandler->lastFatFingersResult();

        // Fat fingers can deal with shadow content.
        if (node = lastFatFingersResult.validNode())
            node = node->shadowAncestorNode();
    }

    if (!node) {
        HitTestResult result = eventHandler->hitTestResultAtPoint(mapFromViewportToContents(mouseEvent.pos()), false /*allowShadowContent*/);
        node = result.innerNode();
    }

    if (mouseEvent.eventType() == WebCore::MouseEventPressed) {
        if (m_inputHandler->willOpenPopupForNode(node)) {
            // Do not allow any human generated mouse or keyboard events to select <option>s in the list box
            // because we use a pop up dialog to handle the actual selections. This prevents options from
            // being selected prior to displaying the pop up dialog. The contents of the listbox are for
            // display only.
            //
            // FIXME: We explicitly do not forward this event to WebCore so as to preserve symmetry with
            // the MouseEventReleased handling (below). This has the side-effect that mousedown events
            // are not fired for human generated mouse press events. See RIM Bug #1579.

            // We do focus <select>/<option> on mouse down so that a Focus event is fired and have the
            // element painted in its focus state on repaint.
            ASSERT(node->isElementNode());
            if (node->isElementNode()) {
                Element* element = static_cast<Element*>(node);
                element->focus();
            }
        } else
            eventHandler->handleMousePressEvent(mouseEvent);
    } else if (mouseEvent.eventType() == WebCore::MouseEventReleased) {
        // FIXME: For <select> and <options> elements, we explicitly do not forward this event to WebCore so
        // as to preserve symmetry with the MouseEventPressed handling (above). This has the side-effect that
        // mouseup events are not fired on such elements for human generated mouse release events. See RIM Bug #1579.
        if (!m_inputHandler->didNodeOpenPopup(node))
            eventHandler->handleMouseReleaseEvent(mouseEvent);
    }

    return true;
}

bool WebPagePrivate::handleWheelEvent(WebCore::PlatformWheelEvent& wheelEvent)
{
    WebCore::EventHandler* eventHandler = m_mainFrame->eventHandler();

    return eventHandler->handleWheelEvent(wheelEvent);
}

bool WebPage::touchEvent(const BlackBerry::Platform::TouchEvent& event)
{
#if DEBUG_TOUCH_EVENTS
    switch (event.m_type) {
    case BlackBerry::Platform::TouchEvent::TouchEnd:
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "WebPage::touchEvent Touch End");
        break;
    case BlackBerry::Platform::TouchEvent::TouchStart:
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "WebPage::touchEvent Touch Start");
        break;
    case BlackBerry::Platform::TouchEvent::TouchMove:
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "WebPage::touchEvent Touch Move");
        break;
    }

    for (int i = 0; i < event.m_points.size(); i++) {
        switch (event.m_points[i].m_state) {
        case BlackBerry::Platform::TouchPoint::TouchPressed:
            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "WebPage::touchEvent %d Touch Pressed (%d, %d)", event.m_points[i].m_id, event.m_points[i].m_pos.x(), event.m_points[i].m_pos.y());
            break;
        case BlackBerry::Platform::TouchPoint::TouchReleased:
            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "WebPage::touchEvent %d Touch Released (%d, %d)", event.m_points[i].m_id, event.m_points[i].m_pos.x(), event.m_points[i].m_pos.y());
            break;
        case BlackBerry::Platform::TouchPoint::TouchMoved:
            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelCritical, "WebPage::touchEvent %d Touch Moved (%d, %d)", event.m_points[i].m_id, event.m_points[i].m_pos.x(), event.m_points[i].m_pos.y());
            break;
        }
    }
#endif

#if ENABLE(TOUCH_EVENTS)

    PluginView* pluginView = d->m_fullScreenPluginView.get();
    if (pluginView)
        return d->dispatchTouchEventToFullScreenPlugin(pluginView, event);

    if (!d->m_mainFrame)
        return false;

    BlackBerry::Platform::TouchEvent tEvent = event;
    for (unsigned i = 0; i < event.m_points.size(); i++) {
        tEvent.m_points[i].m_pos = d->mapFromTransformed(tEvent.m_points[i].m_pos);
        tEvent.m_points[i].m_screenPos = d->mapFromTransformed(tEvent.m_points[i].m_screenPos);
    }

    BlackBerry::Platform::Gesture tapGesture;
    if (event.hasGesture(BlackBerry::Platform::Gesture::SingleTap))
        d->m_pluginMayOpenNewTab = true;
    else if (tEvent.m_type == BlackBerry::Platform::TouchEvent::TouchStart || tEvent.m_type == BlackBerry::Platform::TouchEvent::TouchCancel)
        d->m_pluginMayOpenNewTab = false;

    bool handled = false;

    if (d->m_needTouchEvents && !event.hasGesture(BlackBerry::Platform::Gesture::Injected))
        handled = d->m_mainFrame->eventHandler()->handleTouchEvent(PlatformTouchEvent(&tEvent));

    // Unpress mouse if touch end is consumed by a JavaScript touch handler, otherwise the mouse state will remain pressed
    // which could either mess up the internal mouse state or start text selection on the next mouse move/down.
    if (tEvent.m_type == BlackBerry::Platform::TouchEvent::TouchEnd && handled && d->m_mainFrame->eventHandler()->mousePressed())
        d->m_touchEventHandler->touchEventCancel();

    if (d->m_preventDefaultOnTouchStart) {
        if (tEvent.m_type == BlackBerry::Platform::TouchEvent::TouchEnd || tEvent.m_type == BlackBerry::Platform::TouchEvent::TouchCancel)
            d->m_preventDefaultOnTouchStart = false;
        return true;
    }

    if (handled) {
        if (tEvent.m_type == BlackBerry::Platform::TouchEvent::TouchStart)
            d->m_preventDefaultOnTouchStart = true;
        return true;
    }

    if (event.hasGesture(BlackBerry::Platform::Gesture::TouchHold))
        d->m_touchEventHandler->touchHoldEvent();
#endif

    return false;
}

void WebPage::setScrollOriginPoint(const BlackBerry::Platform::IntPoint& point)
{
    BlackBerry::Platform::IntPoint untransformedPoint = d->mapFromTransformed(point);
    d->setScrollOriginPoint(untransformedPoint);
}

void WebPagePrivate::setScrollOriginPoint(const BlackBerry::Platform::IntPoint& point)
{
    if (!m_hasInRegionScrollableAreas)
        return;

    m_client->notifyInRegionScrollingStartingPointChanged(inRegionScrollerForPoint(point));
}

bool WebPagePrivate::dispatchTouchEventToFullScreenPlugin(PluginView* plugin, const BlackBerry::Platform::TouchEvent& event)
{
    NPTouchEvent npTouchEvent;

    if (event.hasGesture(BlackBerry::Platform::Gesture::DoubleTap))
        npTouchEvent.type = TOUCH_EVENT_DOUBLETAP;
    else if (event.hasGesture(BlackBerry::Platform::Gesture::TouchHold))
        npTouchEvent.type = TOUCH_EVENT_TOUCHHOLD;
    else {
        switch (event.m_type) {
        case BlackBerry::Platform::TouchEvent::TouchStart:
            npTouchEvent.type = TOUCH_EVENT_START;
            break;
        case BlackBerry::Platform::TouchEvent::TouchEnd:
            npTouchEvent.type = TOUCH_EVENT_END;
            break;
        case BlackBerry::Platform::TouchEvent::TouchMove:
            npTouchEvent.type = TOUCH_EVENT_MOVE;
            break;
        case BlackBerry::Platform::TouchEvent::TouchCancel:
            npTouchEvent.type = TOUCH_EVENT_CANCEL;
            break;
        default:
            return false;
        }
    }

    npTouchEvent.points = 0;
    npTouchEvent.size = event.m_points.size();
    if (npTouchEvent.size) {
        npTouchEvent.points = new NPTouchPoint[npTouchEvent.size];
        for (int i = 0; i < npTouchEvent.size; i++) {
            npTouchEvent.points[i].touchId = event.m_points[i].m_id;
            npTouchEvent.points[i].clientX = event.m_points[i].m_screenPos.x();
            npTouchEvent.points[i].clientY = event.m_points[i].m_screenPos.y();
            npTouchEvent.points[i].screenX = event.m_points[i].m_screenPos.x();
            npTouchEvent.points[i].screenY = event.m_points[i].m_screenPos.y();
            npTouchEvent.points[i].pageX = event.m_points[i].m_pos.x();
            npTouchEvent.points[i].pageY = event.m_points[i].m_pos.y();
        }
    }

    NPEvent npEvent;
    npEvent.type = NP_TouchEvent;
    npEvent.data = &npTouchEvent;

    bool handled = plugin->dispatchFullScreenNPEvent(npEvent);

    if (npTouchEvent.type == TOUCH_EVENT_DOUBLETAP && !handled) {
        // Send Touch Up if double tap not consumed.
        npTouchEvent.type = TOUCH_EVENT_END;
        npEvent.data = &npTouchEvent;
        handled = plugin->dispatchFullScreenNPEvent(npEvent);
    }
    delete[] npTouchEvent.points;
    return handled;
}

bool WebPage::touchPointAsMouseEvent(const BlackBerry::Platform::TouchPoint& point)
{
    PluginView* pluginView = d->m_fullScreenPluginView.get();
    if (pluginView)
        return d->dispatchTouchPointAsMouseEventToFullScreenPlugin(pluginView, point);

    d->m_lastUserEventTimestamp = currentTime();

    BlackBerry::Platform::TouchPoint tPoint = point;
    tPoint.m_pos = d->mapFromTransformed(tPoint.m_pos);
    tPoint.m_screenPos = d->mapFromTransformed(tPoint.m_screenPos);

    return d->m_touchEventHandler->handleTouchPoint(tPoint);
}

bool WebPagePrivate::dispatchTouchPointAsMouseEventToFullScreenPlugin(WebCore::PluginView* pluginView, const BlackBerry::Platform::TouchPoint& point)
{
    NPEvent npEvent;
    NPMouseEvent mouse;

    switch (point.m_state) {
    case BlackBerry::Platform::TouchPoint::TouchPressed:
        mouse.type = MOUSE_BUTTON_DOWN;
        break;
    case BlackBerry::Platform::TouchPoint::TouchReleased:
        mouse.type = MOUSE_BUTTON_UP;
        break;
    case BlackBerry::Platform::TouchPoint::TouchMoved:
        mouse.type = MOUSE_MOTION;
        break;
    case BlackBerry::Platform::TouchPoint::TouchStationary:
        return false;
    }

    mouse.x = point.m_screenPos.x();
    mouse.y = point.m_screenPos.y();
    mouse.button = mouse.type != MOUSE_BUTTON_UP;
    mouse.flags = 0;
    npEvent.type = NP_MouseEvent;
    npEvent.data = &mouse;

    return pluginView->dispatchFullScreenNPEvent(npEvent);
}

void WebPage::touchEventCancel()
{
    d->m_pluginMayOpenNewTab = false;
    d->m_touchEventHandler->touchEventCancel();
}

void WebPage::touchEventCancelAndClearFocusedNode()
{
    d->m_touchEventHandler->touchEventCancelAndClearFocusedNode();
}

Frame* WebPagePrivate::focusedOrMainFrame() const
{
    return m_page->focusController()->focusedOrMainFrame();
}

Frame* WebPagePrivate::mainFrame() const
{
    return m_mainFrame;
}

void WebPagePrivate::clearFocusNode()
{
    Frame* frame = focusedOrMainFrame();
    if (!frame)
        return;
    ASSERT(frame->document());

    if (frame->document()->focusedNode())
        frame->page()->focusController()->setFocusedNode(0, frame);
}

bool WebPagePrivate::scrollNodeRecursively(WebCore::Node* node, const WebCore::IntSize& delta)
{
    if (delta.isZero())
        return true;

    if (!node)
        return false;

    WebCore::RenderObject* renderer = node->renderer();
    if (!renderer)
        return false;

    WebCore::FrameView* view = renderer->view()->frameView();
    if (!view)
        return false;

    // Try scrolling the renderer.
    if (scrollRenderer(renderer, delta))
        return true;

    // We've hit the page, don't scroll it and return false.
    if (view == m_mainFrame->view())
        return false;

    // Try scrolling the FrameView.
    if (canScrollInnerFrame(view->frame())) {
        WebCore::IntSize viewDelta = delta;
        IntPoint newViewOffset = view->scrollPosition();
        IntPoint maxViewOffset = view->maximumScrollPosition();
        adjustScrollDelta(maxViewOffset, newViewOffset, viewDelta);

        if (!viewDelta.isZero()) {
            view->setCanBlitOnScroll(false);

            BackingStoreClient* backingStoreClient = backingStoreClientForFrame(view->frame());
            if (backingStoreClient) {
                backingStoreClient->setIsClientGeneratedScroll(true);
                backingStoreClient->setIsScrollNotificationSuppressed(true);
            }

            m_inRegionScrollStartingNode = view->frame()->document();

            view->scrollBy(viewDelta);

            if (backingStoreClient) {
                backingStoreClient->setIsClientGeneratedScroll(false);
                backingStoreClient->setIsScrollNotificationSuppressed(false);
            }

            return true;
        }
    }

    // Try scrolling the node of the enclosing frame.
    Frame* frame = node->document()->frame();
    if (frame) {
        Node* ownerNode = static_cast<Node*>(frame->ownerElement());
        if (scrollNodeRecursively(ownerNode, delta))
            return true;
    }

    return false;
}

void WebPagePrivate::adjustScrollDelta(const WebCore::IntPoint& maxOffset, const WebCore::IntPoint& currentOffset, WebCore::IntSize& delta) const
{
    if (currentOffset.x() + delta.width() > maxOffset.x())
        delta.setWidth(min(maxOffset.x() - currentOffset.x(), delta.width()));

    if (currentOffset.x() + delta.width() < 0)
        delta.setWidth(max(-currentOffset.x(), delta.width()));

    if (currentOffset.y() + delta.height() > maxOffset.y())
        delta.setHeight(min(maxOffset.y() - currentOffset.y(), delta.height()));

    if (currentOffset.y() + delta.height() < 0)
        delta.setHeight(max(-currentOffset.y(), delta.height()));
}

static Node* enclosingLayerNode(RenderLayer*);

bool WebPagePrivate::scrollRenderer(WebCore::RenderObject* renderer, const WebCore::IntSize& delta)
{
    RenderLayer* layer = renderer->enclosingLayer();
    if (!layer)
        return false;

    // Try to scroll layer.
    bool restrictedByLineClamp = false;
    if (renderer->parent())
        restrictedByLineClamp = !renderer->parent()->style()->lineClamp().isNone();

    if (renderer->hasOverflowClip() && !restrictedByLineClamp) {
        WebCore::IntSize layerDelta = delta;
        WebCore::IntPoint maxOffset(layer->scrollWidth() - layer->renderBox()->clientWidth(), layer->scrollHeight() - layer->renderBox()->clientHeight());
        WebCore::IntPoint currentOffset(layer->scrollXOffset(), layer->scrollYOffset());
        adjustScrollDelta(maxOffset, currentOffset, layerDelta);
        if (!layerDelta.isZero()) {
            m_inRegionScrollStartingNode = enclosingLayerNode(layer);
            WebCore::IntPoint newOffset = currentOffset + layerDelta;
            layer->scrollToOffset(newOffset.x(), newOffset.y());
            renderer->repaint(true);
            return true;
        }
    }

    while (layer = layer->parent()) {
        if (canScrollRenderBox(layer->renderBox()))
            return scrollRenderer(layer->renderBox(), delta);
    }

    return false;
}

static void handleScrolling(unsigned short character, WebPagePrivate* scroller)
{
    const int scrollFactor = 20;
    int dx = 0, dy = 0;
    switch (character) {
    case KEYCODE_LEFT:
        dx = -scrollFactor;
        break;
    case KEYCODE_RIGHT:
        dx = scrollFactor;
        break;
    case KEYCODE_UP:
        dy = -scrollFactor;
        break;
    case KEYCODE_DOWN:
        dy = scrollFactor;
        break;
    case KEYCODE_PG_UP:
        ASSERT(scroller);
        dy = scrollFactor - scroller->actualVisibleSize().height();
        break;
    case KEYCODE_PG_DOWN:
        ASSERT(scroller);
        dy = scroller->actualVisibleSize().height() - scrollFactor;
        break;
    }

    if (dx || dy) {
        // Don't use the scrollBy function because it triggers the scroll as originating from BlackBerry
        // but then it expects a separate invalidate which isn't sent in this case.
        ASSERT(scroller && scroller->m_mainFrame && scroller->m_mainFrame->view());
        IntPoint pos(scroller->scrollPosition() + IntSize(dx, dy));

        // Prevent over scrolling for arrows and Page up/down.
        if (pos.x() < 0)
            pos.setX(0);
        if (pos.y() < 0)
            pos.setY(0);
        if (pos.x() + scroller->actualVisibleSize().width() > scroller->contentsSize().width())
            pos.setX(scroller->contentsSize().width() - scroller->actualVisibleSize().width());
        if (pos.y() + scroller->actualVisibleSize().height() > scroller->contentsSize().height())
            pos.setY(scroller->contentsSize().height() - scroller->actualVisibleSize().height());

        scroller->m_mainFrame->view()->setScrollPosition(pos);
        scroller->m_client->scrollChanged(pos);
    }
}

bool WebPage::keyEvent(const BlackBerry::Platform::KeyboardEvent& keyboardEvent)
{
    if (!d->m_mainFrame->view())
        return false;

    ASSERT(d->m_page->focusController());

    bool handled = d->m_inputHandler->handleKeyboardInput(keyboardEvent);

    if (!handled && keyboardEvent.type() == BlackBerry::Platform::KeyboardEvent::KeyDown && !d->m_inputHandler->isInputMode()) {
        IntPoint previousPos = d->scrollPosition();
        handleScrolling(keyboardEvent.character(), d);
        handled = previousPos != d->scrollPosition();
    }

    return handled;
}

void WebPage::navigationMoveEvent(const unsigned short character, bool shiftDown, bool altDown)
{
    if (!d->m_mainFrame->view())
        return;

    ASSERT(d->m_page->focusController());
    d->m_inputHandler->handleNavigationMove(character, shiftDown, altDown);
}

bool WebPage::deleteTextRelativeToCursor(unsigned int leftOffset, unsigned int rightOffset)
{
    return d->m_inputHandler->deleteTextRelativeToCursor(leftOffset, rightOffset);
}

spannable_string_t* WebPage::selectedText(int32_t flags)
{
    return d->m_inputHandler->selectedText(flags);
}

spannable_string_t* WebPage::textBeforeCursor(int32_t length, int32_t flags)
{
    return d->m_inputHandler->textBeforeCursor(length, flags);
}

spannable_string_t* WebPage::textAfterCursor(int32_t length, int32_t flags)
{
    return d->m_inputHandler->textAfterCursor(length, flags);
}

extracted_text_t* WebPage::extractedTextRequest(extracted_text_request_t* request, int32_t flags)
{
    return d->m_inputHandler->extractedTextRequest(request, flags);
}

int32_t WebPage::setComposingRegion(int32_t start, int32_t end)
{
    return d->m_inputHandler->setComposingRegion(start, end);
}

int32_t WebPage::finishComposition()
{
    return d->m_inputHandler->finishComposition();
}

int32_t WebPage::setComposingText(spannable_string_t* spannableString, int32_t relativeCursorPosition)
{
    return d->m_inputHandler->setComposingText(spannableString, relativeCursorPosition);
}

int32_t WebPage::commitText(spannable_string_t* spannableString, int32_t relativeCursorPosition)
{
    return d->m_inputHandler->commitText(spannableString, relativeCursorPosition);
}

void WebPage::spellCheckingEnabled(bool enabled)
{
    static_cast<EditorClientBlackBerry*>(d->m_page->editorClient())->enableSpellChecking(enabled);
}

void WebPage::selectionCancelled()
{
    d->m_selectionHandler->cancelSelection();
}

bool WebPage::selectionContains(const Platform::IntPoint& point)
{
    return d->m_selectionHandler->selectionContains(d->mapFromTransformed(point));
}

WebString WebPage::title() const
{
    if (d->m_mainFrame->document())
        return d->m_mainFrame->loader()->documentLoader()->title().string();
    return WebString();
}

WebString WebPage::selectedText() const
{
    return d->m_selectionHandler->selectedText();
}

WebString WebPage::cutSelectedText()
{
    WebString selectedText = d->m_selectionHandler->selectedText();
    if (!selectedText.isEmpty())
        d->m_inputHandler->deleteSelection();
    return selectedText;
}

void WebPage::insertText(const WebString& string)
{
    d->m_inputHandler->insertText(string);
}

void WebPage::clearCurrentInputField()
{
    d->m_inputHandler->clearField();
}

void WebPage::cut()
{
    d->m_inputHandler->cut();
}

void WebPage::copy()
{
    d->m_inputHandler->copy();
}

void WebPage::paste()
{
    d->m_inputHandler->paste();
}

void WebPage::setSelection(const Platform::IntPoint& startPoint, const Platform::IntPoint& endPoint)
{
    // Transform this events coordinates to webkit content coordinates.
    // FIXME: Don't transform the sentinel, because it may be transformed to a floating number
    // which could be rounded to 0 or other numbers. This workaround should be removed after
    // the error of roundUntransformedPoint() is fixed.
    WebCore::IntPoint start =
            WebCore::IntPoint(startPoint) == DOMSupport::InvalidPoint ?
            DOMSupport::InvalidPoint :
            d->mapFromTransformed(startPoint);
    WebCore::IntPoint end =
            WebCore::IntPoint(endPoint) == DOMSupport::InvalidPoint ?
            DOMSupport::InvalidPoint :
            d->mapFromTransformed(endPoint);

    d->m_selectionHandler->setSelection(start, end);
}

void WebPage::setCaretPosition(const Platform::IntPoint& position)
{
    // Handled by selection handler as it's point based.
    // Transform this events coordinates to webkit content coordinates.
    d->m_selectionHandler->setCaretPosition(d->mapFromTransformed(position));
}

void WebPage::selectAtPoint(const Platform::IntPoint& location)
{
    // Transform this events coordinates to webkit content coordinates if it
    // is not the sentinel value.
    WebCore::IntPoint selectionLocation =
            WebCore::IntPoint(location) == DOMSupport::InvalidPoint ?
            DOMSupport::InvalidPoint :
            d->mapFromTransformed(location);

    d->m_selectionHandler->selectAtPoint(selectionLocation);
}

// Returned scroll position is in transformed coordinates.
Platform::IntPoint WebPage::scrollPosition() const
{
    return d->transformedScrollPosition();
}

// Setting the scroll position is in transformed coordinates.
void WebPage::setScrollPosition(const Platform::IntPoint& point)
{
    if (d->transformedPointEqualsUntransformedPoint(point, d->scrollPosition()))
        return;

    // If the user recently performed an event, this new scroll position
    // could possibly be a result of that. Or not, this is just a heuristic.
    if (currentTime() - d->m_lastUserEventTimestamp < manualScrollInterval)
        d->m_userPerformedManualScroll = true;

    d->m_backingStoreClient->setIsClientGeneratedScroll(true);
    d->m_mainFrame->view()->setCanOverscroll(true);
    d->setScrollPosition(d->mapFromTransformed(point));
    d->m_mainFrame->view()->setCanOverscroll(false);
    d->m_backingStoreClient->setIsClientGeneratedScroll(false);
}

WebString WebPage::textEncoding()
{
    Frame* frame = d->focusedOrMainFrame();
    if (!frame)
        return "";

    Document* document = frame->document();
    if (!document)
        return "";

    return document->loader()->writer()->encoding();
}

WebString WebPage::forcedTextEncoding()
{
    Frame* frame = d->focusedOrMainFrame();
    if (!frame)
        return "";

    Document* document = frame->document();
    if (!document)
        return "";

    return document->loader()->overrideEncoding();
}

void WebPage::setForcedTextEncoding(const char* encoding)
{
    if (encoding && d->focusedOrMainFrame() && d->focusedOrMainFrame()->loader() && d->focusedOrMainFrame()->loader())
        return d->focusedOrMainFrame()->loader()->reloadWithOverrideEncoding(encoding);
}

// FIXME: Move to DOMSupport.
bool WebPagePrivate::canScrollInnerFrame(Frame* frame) const
{
    if (!frame)
        return false;

    if (!frame->view())
        return false;

    // Not having an owner element means that we are on the mainframe.
    if (!frame->ownerElement())
        return false;

    ASSERT(frame != m_mainFrame);

    WebCore::IntSize visibleSize = frame->view()->visibleContentRect().size();
    WebCore::IntSize contentsSize = frame->view()->contentsSize();

    bool canBeScrolled = contentsSize.height() > visibleSize.height() || contentsSize.width() > visibleSize.width();

    // Lets also consider the 'scrolling' attribute of inner frames.
    return canBeScrolled && (frame->ownerElement()->scrollingMode() != ScrollbarAlwaysOff);
}

// The RenderBox::canbeScrolledAndHasScrollableArea method returns true for the
// following scenario, for example:
// (1) a div that has a vertical overflow but no horizontal overflow
//     with overflow-y: hidden and overflow-x: auto set.
// The version below fixes it.
// FIXME: Fix it upstream!
bool WebPagePrivate::canScrollRenderBox(RenderBox* box)
{
    if (!box)
        return false;

    if (!box->hasOverflowClip())
        return false;

    if (box->scrollsOverflowX() && (box->scrollWidth() != box->clientWidth())
        || box->scrollsOverflowY() && (box->scrollHeight() != box->clientHeight()))
        return true;

    Node* node = box->node();
    return node && (node->rendererIsEditable() || node->isDocumentNode());
}

static RenderLayer* parentLayer(RenderLayer* layer)
{
    ASSERT(layer);

    if (layer->parent())
        return layer->parent();

    RenderObject* renderer = layer->renderer();
    if (renderer->document() && renderer->document()->ownerElement() && renderer->document()->ownerElement()->renderer())
        return renderer->document()->ownerElement()->renderer()->enclosingLayer();

    return 0;
}

// FIXME: Make RenderLayer::enclosingElement public so this one can be removed.
static Node* enclosingLayerNode(RenderLayer* layer)
{
    for (RenderObject* r = layer->renderer(); r; r = r->parent()) {
        if (Node* e = r->node())
            return e;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

void pushBackInRegionScrollable(std::vector<Platform::ScrollViewBase>& vector, InRegionScroller scroller, WebPagePrivate* webPage)
{
    ASSERT(webPage);
    ASSERT(!scroller.isNull());

    scroller.setCanPropagateScrollingToEnclosingScrollable(!isNonRenderViewFixedPositionedContainer(scroller.layer()));
    vector.push_back(scroller);
    if (vector.size() == 1) {
        // FIXME: Use RenderLayer::renderBox()->node() instead?
        webPage->m_inRegionScrollStartingNode = enclosingLayerNode(scroller.layer());
    }
}

std::vector<Platform::ScrollViewBase> WebPagePrivate::inRegionScrollerForPoint(const Platform::IntPoint& point)
{
    std::vector<Platform::ScrollViewBase> validReturn;
    std::vector<Platform::ScrollViewBase> emptyReturn;

    HitTestResult result = m_mainFrame->eventHandler()->hitTestResultAtPoint(mapFromViewportToContents(point), false /*allowShadowContent*/);
    Node* node = result.innerNonSharedNode();
    if (!node)
        return emptyReturn;

    RenderObject* renderer = node->renderer();
    // FIXME: Validate with elements with visibility:hidden.
    if (!renderer)
        return emptyReturn;

    RenderLayer* layer = renderer->enclosingLayer();

    do {
        RenderObject* renderer = layer->renderer();

        if (renderer->isRenderView()) {
            if (RenderView* renderView = toRenderView(renderer)) {
                FrameView* view = renderView->frameView();
                if (!view)
                    return emptyReturn;

                if (canScrollInnerFrame(view->frame())) {
                    pushBackInRegionScrollable(validReturn, InRegionScroller(this, layer), this);
                    continue;
                }
            }
        } else if (canScrollRenderBox(layer->renderBox())) {
            pushBackInRegionScrollable(validReturn, InRegionScroller(this, layer), this);
            continue;
        }

        // If we run into a fix positioned layer, set the
        // last scrollable in-region object as not able to
        // propagate scroll to its parent scrollable.
        if (isNonRenderViewFixedPositionedContainer(layer) && validReturn.size()) {
            Platform::ScrollViewBase& end = validReturn.back();
            end.setCanPropagateScrollingToEnclosingScrollable(false);
        }

    } while (layer = parentLayer(layer));

    if (validReturn.empty())
        return emptyReturn;

    return validReturn;
}

BackingStore* WebPage::backingStore() const
{
    return d->m_backingStore;
}

bool WebPage::zoomToFit()
{
    if (d->contentsSize().isEmpty() || !d->isUserScalable())
        return false;

    d->m_userPerformedManualZoom = true;

    //TODO: We may need to use (0,0) as the anchor point when textReflow is enabled.
    // IF the minimum font size is ginormous, we may still want the scroll position to be 0,0
    return d->zoomAboutPoint(d->zoomToFitScale(), d->centerOfVisibleContentsRect());
}

void WebPagePrivate::setTextReflowAnchorPoint(const BlackBerry::Platform::IntPoint& focalPoint)
{
    // Should only be invoked when text reflow is enabled
    ASSERT(m_webPage->settings()->textReflowMode() == WebSettings::TextReflowEnabled);

    m_currentPinchZoomNode = bestNodeForZoomUnderPoint(focalPoint);
    if (!m_currentPinchZoomNode)
        return;

    IntRect nodeRect = rectForNode(m_currentPinchZoomNode.get());
    m_anchorInNodeRectRatio.set(static_cast<float>(mapFromTransformed(focalPoint).x() - nodeRect.x()) / nodeRect.width(),
                                static_cast<float>(mapFromTransformed(focalPoint).y() - nodeRect.y()) / nodeRect.height());
}

bool WebPage::pinchZoomAboutPoint(double scale, int x, int y)
{
    IntPoint anchor(x, y);
    d->m_userPerformedManualZoom = true;
    d->m_userPerformedManualScroll = true;

    if (d->m_webPage->settings()->textReflowMode() == WebSettings::TextReflowEnabled) {
        d->setTextReflowAnchorPoint(anchor);
        // Theoretically, d->nodeForZoomUnderPoint(anchor) can return null.
        if (!d->m_currentPinchZoomNode)
            return false;
    }

    return d->zoomAboutPoint(scale, d->mapFromTransformed(anchor));
}

#if ENABLE(VIEWPORT_REFLOW)
void WebPagePrivate::toggleTextReflowIfTextReflowEnabledOnlyForBlockZoom(bool shouldEnableTextReflow)
{
    if (m_webPage->settings()->textReflowMode() == WebSettings::TextReflowEnabledOnlyForBlockZoom)
        m_page->settings()->setTextReflowEnabled(shouldEnableTextReflow);
}
#endif

bool WebPage::blockZoom(int x, int y)
{
    if (!d->m_mainFrame->view() || !d->isUserScalable())
        return false;

    Node* node = d->bestNodeForZoomUnderPoint(IntPoint(x, y));
    if (!node)
        return false;

    IntRect nodeRect = d->rectForNode(node);
    IntRect blockRect;
    bool endOfBlockZoomMode = d->compareNodesForBlockZoom(d->m_currentBlockZoomAdjustedNode.get(), node);
    const double oldScale = d->m_transformationMatrix->m11();
    double newScale = 0.0;
    const double margin = endOfBlockZoomMode ? 0 : blockZoomMargin * 2 * oldScale;
    bool isFirstZoom = false;

    if (endOfBlockZoomMode) {
        // End of block zoom mode
        IntRect rect = d->blockZoomRectForNode(node);
        blockRect = IntRect(0, rect.y(), d->transformedContentsSize().width(), d->transformedContentsSize().height() - rect.y());
        d->m_shouldReflowBlock = false;
    } else {
        // Start/continue block zoom mode
        Node* tempBlockZoomAdjustedNode = d->m_currentBlockZoomAdjustedNode.get();
        blockRect = d->blockZoomRectForNode(node);

        // Don't use a block if it is too close to the size of the actual contents.
        // We allow this for images only so that they can be zoomed tight to the screen.
        if (!node->hasTagName(HTMLNames::imgTag)) {
            IntRect tRect = d->mapFromTransformed(blockRect);
            int blockArea = tRect.width() * tRect.height();
            int pageArea = d->contentsSize().width() * d->contentsSize().height();
            double blockToPageRatio = (double)(pageArea - blockArea) / pageArea;
            if (blockToPageRatio < 0.15) {
                // Restore old adjust node because zoom was canceled.
                d->m_currentBlockZoomAdjustedNode = tempBlockZoomAdjustedNode;
                return false;
            }
        }

        if (blockRect.isEmpty() || !blockRect.width() || !blockRect.height())
            return false;

        if (!d->m_currentBlockZoomNode.get())
            isFirstZoom = true;

        d->m_currentBlockZoomNode = node;
        d->m_shouldReflowBlock = true;
    }

    newScale = std::min(d->newScaleForBlockZoomRect(blockRect, oldScale, margin), d->maxBlockZoomScale());
    newScale = std::max(newScale, minimumScale());

#if DEBUG_BLOCK_ZOOM
    // Render the double tap point for visual reference.
    IntRect renderRect(x, y, 1, 1);
    renderRect = d->mapFromTransformedContentsToTransformedViewport(renderRect);
    IntSize viewportSize = d->transformedViewportSize();
    renderRect.intersect(IntRect(0, 0, viewportSize.width(), viewportSize.height()));
    d->m_backingStore->d->clearWindow(renderRect, 0, 0, 0);
    d->m_backingStore->d->invalidateWindow(renderRect);

    // Uncomment this to return in order to see the blocks being selected.
//    d->m_client->zoomChanged(isMinZoomed(), isMaxZoomed(), isAtInitialZoom(), currentZoomLevel());
//    return true;
#endif

#if ENABLE(VIEWPORT_REFLOW)
    // If reflowing, adjust the reflow-width of text node to make sure the font is a reasonable size.
    if (d->m_currentBlockZoomNode && d->m_shouldReflowBlock && settings()->textReflowMode() != WebSettings::TextReflowDisabled) {
        RenderObject* renderer = d->m_currentBlockZoomNode->renderer();
        if (renderer && renderer->isText()) {
            double newFontSize = renderer->style()->fontSize() * newScale;
            if (newFontSize < d->m_webSettings->defaultFontSize()) {
                newScale = std::min(static_cast<double>(d->m_webSettings->defaultFontSize()) / renderer->style()->fontSize(), d->maxBlockZoomScale());
                newScale = std::max(newScale, minimumScale());
            }
            blockRect.setWidth(oldScale * static_cast<double>(d->transformedActualVisibleSize().width()) / newScale);
            // Re-calculate the scale here to take in to account the margin.
            newScale = std::min(d->newScaleForBlockZoomRect(blockRect, oldScale, margin), d->maxBlockZoomScale());
            newScale = std::max(newScale, minimumScale()); // Still, it's not allowed to be smaller than minimum scale.
        }
    }
#endif

    // Align the zoomed block in the screen.
    double newBlockHeight = d->mapFromTransformed(blockRect).height();
    double newBlockWidth = d->mapFromTransformed(blockRect).width();
    double scaledViewportWidth = static_cast<double>(d->actualVisibleSize().width()) * oldScale / newScale;
    double scaledViewportHeight = static_cast<double>(d->actualVisibleSize().height()) * oldScale / newScale;
    double dx = std::max(0.0, (scaledViewportWidth - newBlockWidth) / 2.0);
    double dy = std::max(0.0, (scaledViewportHeight - newBlockHeight) / 2.0);

    RenderObject* renderer = d->m_currentBlockZoomAdjustedNode->renderer();
    FloatPoint anchor;
    FloatPoint topLeftPoint(d->mapFromTransformed(blockRect).location());
    if (renderer && renderer->isText()) {
        ETextAlign textAlign = renderer->style()->textAlign();
        switch (textAlign) {
        case CENTER:
        case WEBKIT_CENTER:
            anchor = FloatPoint(nodeRect.x() + (nodeRect.width() - scaledViewportWidth) / 2, topLeftPoint.y());
            break;
        case LEFT:
        case WEBKIT_LEFT:
            anchor = topLeftPoint;
            break;
        case RIGHT:
        case WEBKIT_RIGHT:
            anchor = FloatPoint(nodeRect.x() + nodeRect.width() - scaledViewportWidth, topLeftPoint.y());
            break;
        case TAAUTO:
        case JUSTIFY:
        default:
            if (renderer->style()->isLeftToRightDirection())
                anchor = topLeftPoint;
            else
                anchor = FloatPoint(nodeRect.x() + nodeRect.width() - scaledViewportWidth, topLeftPoint.y());
            break;
        }
    } else
        anchor = renderer->style()->isLeftToRightDirection() ? topLeftPoint : FloatPoint(nodeRect.x() + nodeRect.width() - scaledViewportWidth, topLeftPoint.y());

    if (newBlockHeight <= scaledViewportHeight) {
        // The block fits in the viewport so center it.
        d->m_finalBlockPoint = FloatPoint(anchor.x() - dx, anchor.y() - dy);
    } else {
        // The block is longer than the viewport so top align it and add 3 pixel margin.
        d->m_finalBlockPoint = FloatPoint(anchor.x() - dx, anchor.y() - 3);
    }

#if ENABLE(VIEWPORT_REFLOW)
    // We don't know how long the reflowed block will be so we position it at the top of the screen with a small margin.
    if (settings()->textReflowMode() != WebSettings::TextReflowDisabled) {
        d->m_finalBlockPoint = FloatPoint(anchor.x() - dx, anchor.y() - 3);
        d->m_finalBlockPointReflowOffset = FloatPoint(-dx, -3);
    }
#endif

    // Make sure that the original node rect is visible in the screen after the zoom. This is necessary because the identified block rect might
    // not be the same as the original node rect, and it could force the original node rect off the screen.
    FloatRect br(anchor, FloatSize(scaledViewportWidth, scaledViewportHeight));
    IntPoint clickPoint = d->mapFromTransformed(IntPoint(x, y));
    if (!br.contains(clickPoint)) {
        d->m_finalBlockPointReflowOffset.move(0, (clickPoint.y() - scaledViewportHeight / 2) - d->m_finalBlockPoint.y());
        d->m_finalBlockPoint = FloatPoint(d->m_finalBlockPoint.x(), clickPoint.y() - scaledViewportHeight / 2);
    }

    // Clamp the finalBlockPoint to not cause any overflow scrolling.
    if (d->m_finalBlockPoint.x() < 0) {
        d->m_finalBlockPoint.setX(0);
        d->m_finalBlockPointReflowOffset.setX(0);
    } else if (d->m_finalBlockPoint.x() + scaledViewportWidth > d->contentsSize().width()) {
        d->m_finalBlockPoint.setX(d->contentsSize().width() - scaledViewportWidth);
        d->m_finalBlockPointReflowOffset.setX(0);
    }

    if (d->m_finalBlockPoint.y() < 0) {
        d->m_finalBlockPoint.setY(0);
        d->m_finalBlockPointReflowOffset.setY(0);
    } else if (d->m_finalBlockPoint.y() + scaledViewportHeight > d->contentsSize().height()) {
        d->m_finalBlockPoint.setY(d->contentsSize().height() - scaledViewportHeight);
        d->m_finalBlockPointReflowOffset.setY(0);
    }

    d->m_finalBlockPoint = d->mapToTransformedFloatPoint(d->m_finalBlockPoint);

    // Don't block zoom if the user is zooming and the new scale is only marginally different from the
    // oldScale with only a marginal change in scroll position. Ignore scroll difference in the special case
    // that the zoom level is the minimumScale.
    if (!endOfBlockZoomMode && abs(newScale - oldScale) / oldScale < 0.15) {
        const double minimumDisplacement = 0.15 * d->transformedActualVisibleSize().width();
        if (oldScale == d->minimumScale() || (distanceBetweenPoints(roundTransformedPoint(d->mapToTransformed(d->scrollPosition())), roundTransformedPoint(d->m_finalBlockPoint)) < minimumDisplacement && abs(newScale - oldScale) / oldScale < 0.10)) {
            if (isFirstZoom) {
                d->resetBlockZoom();
                return false;
            }
            // Zoom out of block zoom.
            blockZoom(x, y);
            return true;
        }
    }

    d->m_blockZoomFinalScale = newScale;

    // We set this here to make sure we don't try to re-render the page at a different zoom level during loading.
    d->m_userPerformedManualZoom = true;
    d->m_userPerformedManualScroll = true;
    d->m_client->animateBlockZoom(d->m_finalBlockPoint, d->m_blockZoomFinalScale);

    return true;
}

bool WebPage::isMaxZoomed() const
{
    return (d->currentScale() == d->maximumScale()) || !d->isUserScalable();
}

bool WebPage::isMinZoomed() const
{
    return (d->currentScale() == d->minimumScale()) || !d->isUserScalable();
}

bool WebPage::isAtInitialZoom() const
{
    return (d->currentScale() == d->initialScale()) || !d->isUserScalable();
}

bool WebPagePrivate::shouldZoomOnEscape() const
{
    if (!isUserScalable())
        return false;

    // If the initial scale is not reachable, don't try to zoom.
    if (initialScale() < minimumScale() || initialScale() > maximumScale())
        return false;

    // Don't ever zoom in when we press escape.
    if (initialScale() >= currentScale())
        return false;

    return currentScale() != initialScale();
}

void WebPage::zoomToInitialScale()
{
    if (!d->isUserScalable())
        return;

    d->zoomAboutPoint(d->initialScale(), d->centerOfVisibleContentsRect());
}

bool WebPage::zoomToOneOne()
{
    if (!d->isUserScalable())
        return false;

    double scale = 1.0;
    return d->zoomAboutPoint(scale, d->centerOfVisibleContentsRect());
}

bool WebPage::moveToNextField(BlackBerry::Platform::ScrollDirection dir, int desiredScrollAmount)
{
    return d->moveToNextField(dir, desiredScrollAmount);
}

BlackBerry::Platform::IntRect WebPage::focusNodeRect()
{
    return d->focusNodeRect();
}

void WebPage::setFocused(bool focused)
{
    WebCore::FocusController* focusController = d->m_page->focusController();
    focusController->setActive(focused);
    if (focused) {
        Frame* frame = focusController->focusedFrame();
        if (!frame)
            focusController->setFocusedFrame(d->m_mainFrame);
    }
    focusController->setFocused(focused);
}

bool WebPage::focusField(bool focus)
{
    return d->focusField(focus);
}

bool WebPage::linkToLinkOnClick()
{
    Frame* frame = d->focusedOrMainFrame();
    if (!frame)
        return false;

    Document* doc = frame->document();
    if (!doc)
        return false;

    Node* focusedNode = doc->focusedNode();

    if (!focusedNode)
        return false;

    // Make sure the node is visible before triggering the click.
    WebCore::IntRect visibleRect = IntRect(IntPoint(), d->actualVisibleSize());
    // Constrain the rect if this is a subframe.
    if (frame->view()->parent())
        visibleRect.intersect(d->getRecursiveVisibleWindowRect(frame->view()));

    if (!visibleRect.intersects(getNodeWindowRect(focusedNode)))
        return false;

    // Check if the popup opened.
    d->m_inputHandler->didNodeOpenPopup(focusedNode);
    // Create click event.
    focusedNode->dispatchSimulatedClick(0, true);
    return true;
}

bool WebPage::findNextString(const char* text, bool forward)
{
    return d->m_selectionHandler->findNextString(WTF::String::fromUTF8(text), forward);
}

bool WebPage::findNextUnicodeString(const unsigned short* text, bool forward)
{
    return d->m_selectionHandler->findNextString(WTF::String(text), forward);
}

void WebPage::runLayoutTests()
{
#if ENABLE_DRT
    // FIXME: do we need API to toggle this?
    d->m_page->settings()->setDeveloperExtrasEnabled(true);

    if (!d->m_dumpRenderTree)
        d->m_dumpRenderTree = new DumpRenderTree(this);
    d->m_dumpRenderTree->runTests();
#endif
}

bool WebPage::enableScriptDebugger()
{
#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (d->m_scriptDebugger)
        return true;

    d->m_scriptDebugger = adoptPtr(new JavaScriptDebuggerBlackBerry(this));

    return !!d->m_scriptDebugger;
#endif
}

bool WebPage::disableScriptDebugger()
{
#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (!d->m_scriptDebugger)
        return true;

    d->m_scriptDebugger.clear();
    return true;
#endif
}

void WebPage::addBreakpoint(const unsigned short* url, unsigned urlLength, int lineNumber, const unsigned short* condition, unsigned conditionLength)
{
#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (d->m_scriptDebugger)
        d->m_scriptDebugger->addBreakpoint(url, urlLength, lineNumber, condition, conditionLength);
#endif
}

void WebPage::updateBreakpoint(const unsigned short* url, unsigned urlLength, int lineNumber, const unsigned short* condition, unsigned conditionLength)
{
#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (d->m_scriptDebugger)
        d->m_scriptDebugger->updateBreakpoint(url, urlLength, lineNumber, condition, conditionLength);
#endif
}

void WebPage::removeBreakpoint(const unsigned short* url, unsigned urlLength, int lineNumber)
{
#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (d->m_scriptDebugger)
        d->m_scriptDebugger->removeBreakpoint(url, urlLength, lineNumber);
#endif
}

bool WebPage::pauseOnExceptions()
{
#if ENABLE(JAVASCRIPT_DEBUGGER)
    return d->m_scriptDebugger ? d->m_scriptDebugger->pauseOnExceptions() : false;
#endif
}

void WebPage::setPauseOnExceptions(bool pause)
{
#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (d->m_scriptDebugger)
        d->m_scriptDebugger->setPauseOnExceptions(pause);
#endif
}

void WebPage::pauseInDebugger()

{
#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (d->m_scriptDebugger)
        d->m_scriptDebugger->pauseInDebugger();
#endif
}

void WebPage::resumeDebugger()
{
#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (d->m_scriptDebugger)
        d->m_scriptDebugger->resumeDebugger();
#endif
}

void WebPage::stepOverStatementInDebugger()
{
#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (d->m_scriptDebugger)
        d->m_scriptDebugger->stepOverStatementInDebugger();
#endif
}

void WebPage::stepIntoStatementInDebugger()
{
#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (d->m_scriptDebugger)
        d->m_scriptDebugger->stepIntoStatementInDebugger();
#endif
}

void WebPage::stepOutOfFunctionInDebugger()
{
#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (d->m_scriptDebugger)
        d->m_scriptDebugger->stepOutOfFunctionInDebugger();
#endif
}

unsigned WebPage::timeoutForJavaScriptExecution() const
{
    return WebCore::Settings::timeoutForJavaScriptExecution(d->m_page->groupName());
}

void WebPage::setTimeoutForJavaScriptExecution(unsigned ms)
{
    WebCore::Settings::setTimeoutForJavaScriptExecution(d->m_page->groupName(), ms);

    WebCore::Document* doc = d->m_page->mainFrame()->document();
    if (!doc)
        return;

    doc->globalData()->timeoutChecker.setTimeoutInterval(ms);
}

JSContextRef WebPage::scriptContext() const
{
    if (!d->m_mainFrame)
        return 0;
    JSC::Bindings::RootObject *root = d->m_mainFrame->script()->bindingRootObject();
    if (!root)
        return 0;
    JSC::ExecState *exec = root->globalObject()->globalExec();
    return toRef(exec);
}

JSValueRef WebPage::windowObject() const
{
    return toRef(d->m_mainFrame->script()->globalObject(WebCore::mainThreadNormalWorld()));
}

#if ENABLE(WEBDOM)
WebDOMDocument WebPage::document() const
{
    if (!d->m_mainFrame)
        return WebDOMDocument();
    return WebDOMDocument(d->m_mainFrame->document());
}
#endif

// Serialize only the members of HistoryItem which are needed by the client,
// and copy them into a SharedArray. Also include the HistoryItem pointer which
// will be used by the client as an opaque reference to identify the item.
void WebPage::getBackForwardList(SharedArray<BackForwardEntry>& result, unsigned int& resultSize) const
{
    HistoryItemVector entries = static_cast<WebCore::BackForwardListImpl*>(d->m_page->backForward()->client())->entries();
    resultSize = entries.size();
    result.reset(new BackForwardEntry[resultSize]);

    for (unsigned i = 0; i < resultSize; ++i) {
        RefPtr<HistoryItem> entry = entries[i];
        BackForwardEntry& resultEntry = result[i];
        resultEntry.url = entry->urlString();
        resultEntry.originalUrl = entry->originalURLString();
        resultEntry.title = entry->title();
        resultEntry.networkToken = entry->viewState().networkToken;
        resultEntry.lastVisitWasHTTPNonGet = entry->lastVisitWasHTTPNonGet();
        resultEntry.id = backForwardIdFromHistoryItem(entry.get());

        // Make sure the HistoryItem is not disposed while the result list is still being used, to make sure the pointer is not reused
        // will be balanced by deref in releaseBackForwardEntry.
        entry->ref();
    }
}

void WebPage::releaseBackForwardEntry(BackForwardId id) const
{
    HistoryItem* item = historyItemFromBackForwardId(id);
    ASSERT(item);
    item->deref();
}

void WebPage::clearBrowsingData()
{
    clearMemoryCaches();
    clearAppCache(d->m_page->groupName());
    clearLocalStorage();
    clearCookieCache();
    clearHistory();
    clearPluginSiteData();
}

void WebPage::clearHistory()
{
    // Don't clear the back-forward list as we might like to keep it.
}

void WebPage::clearCookies()
{
    clearCookieCache();
}

void WebPage::clearLocalStorage()
{
    BlackBerry::WebKit::clearLocalStorage(d->m_page->groupName());
    BlackBerry::WebKit::clearDatabase(d->m_page->groupName());
    clearStorage();
}

void WebPage::clearCache()
{
    clearMemoryCaches();
    clearAppCache(d->m_page->groupName());
}

void WebPage::clearBackForwardList(bool keepCurrentPage) const
{
    BackForwardListImpl* backForwardList = static_cast<BackForwardListImpl*>(d->m_page->backForward()->client());
    RefPtr<HistoryItem> currentItem = backForwardList->currentItem();
    while (!backForwardList->entries().isEmpty())
        backForwardList->removeItem(backForwardList->entries().last().get());
    if (keepCurrentPage)
        backForwardList->addItem(currentItem);
}

bool WebPage::isEnableLocalAccessToAllCookies() const
{
    return cookieManager().canLocalAccessAllCookies();
}

void WebPage::setEnableLocalAccessToAllCookies(bool enabled)
{
    cookieManager().setCanLocalAccessAllCookies(enabled);
}

ResourceHolder* WebPage::getImageFromContext()
{
    WebCore::Node* node = d->m_currentContextNode.get();
    if (!node)
        return 0;

    // FIXME: We only support HTMLImageElement for now.
    if (node->hasTagName(WebCore::HTMLNames::imgTag)) {
        WebCore::HTMLImageElement* image = static_cast<WebCore::HTMLImageElement*>(node);
        if (CachedResource* cachedResource = image->cachedImage()) {
            if (ResourceHolder* holder = ResourceHolderImpl::create(*cachedResource))
                return holder;
        }
    }

    return 0;
}

void WebPage::addVisitedLink(const unsigned short* url, unsigned int length)
{
    ASSERT(d->m_page);
    d->m_page->group().addVisitedLink(url, length);
}

#if ENABLE(WEBDOM)
WebDOMNode WebPage::nodeAtPoint(int x, int y)
{
    HitTestResult result = d->m_mainFrame->eventHandler()->hitTestResultAtPoint(d->mapFromTransformed(WebCore::IntPoint(x, y)), false);
    Node* node = result.innerNonSharedNode();
    return WebDOMNode(node);
}

bool WebPage::getNodeRect(const WebDOMNode& node, Platform::IntRect& result)
{
    Node* nodeImpl = node.impl();
    if (nodeImpl && nodeImpl->renderer()) {
        result = nodeImpl->getRect();
        return true;
    }

    return false;
}

bool WebPage::setNodeFocus(const WebDOMNode& node, bool on)
{
    Node* nodeImpl = node.impl();

    if (nodeImpl && nodeImpl->isFocusable()) {
        Document* doc = nodeImpl->document();
        if (Page* page = doc->page()) {
            // Modify if focusing on node or turning off focused node.
            if (on) {
                page->focusController()->setFocusedNode(nodeImpl, doc->frame());
                if (nodeImpl->isElementNode())
                    static_cast<Element*>(nodeImpl)->updateFocusAppearance(true);
                d->m_inputHandler->didNodeOpenPopup(nodeImpl);
            } else if (doc->focusedNode() == nodeImpl) // && !on
                page->focusController()->setFocusedNode(0, doc->frame());

            return true;
        }
    }
    return false;
}

bool WebPage::setNodeHovered(const WebDOMNode& node, bool on)
{
    Node* nodeImpl = node.impl();
    if (nodeImpl) {
        nodeImpl->setHovered(on);
        return true;
    }
    return false;
}

bool WebPage::nodeHasHover(const WebDOMNode& node)
{
    Node* nodeImpl = node.impl();
    if (nodeImpl) {
        RenderStyle* style = nodeImpl->renderStyle();
        if (style)
            return style->affectedByHoverRules();
    }
    return false;
}
#endif

WTF::String WebPagePrivate::findPatternStringForUrl(const KURL& url) const
{
    if ((m_webSettings->shouldHandlePatternUrls() && protocolIs(url, "pattern"))
            || protocolIs(url, "tel")
            || protocolIs(url, "wtai")
            || protocolIs(url, "cti")
            || protocolIs(url, "mailto")
            || protocolIs(url, "sms")
            || protocolIs(url, "pin")) {
        return url;
    }
    return WTF::String();
}

bool WebPage::defersLoading() const
{
    return d->m_page->defersLoading();
}

bool WebPage::willFireTimer()
{
    if (d->isLoading())
        return true;

    return d->m_backingStore->d->willFireTimer();
}

void WebPage::clearStorage()
{
}

void WebPage::notifyPagePause()
{
    HashSet<PluginView*>::const_iterator it = d->m_pluginViews.begin();
    HashSet<PluginView*>::const_iterator last = d->m_pluginViews.end();
    for (; it != last; ++it)
        (*it)->handlePauseEvent();
}

void WebPage::notifyPageResume()
{
    HashSet<PluginView*>::const_iterator it = d->m_pluginViews.begin();
    HashSet<PluginView*>::const_iterator last = d->m_pluginViews.end();
    for (; it != last; ++it)
        (*it)->handleResumeEvent();
}

void WebPage::notifyPageBackground()
{
#if USE(ACCELERATED_COMPOSITING)
    d->suspendRootLayerCommit();
#endif

    HashSet<PluginView*>::const_iterator it = d->m_pluginViews.begin();
    HashSet<PluginView*>::const_iterator last = d->m_pluginViews.end();
    for (; it != last; ++it)
        (*it)->handleBackgroundEvent();
}

void WebPage::notifyPageForeground()
{
#if USE(ACCELERATED_COMPOSITING)
    d->resumeRootLayerCommit();
#endif

    HashSet<PluginView*>::const_iterator it = d->m_pluginViews.begin();
    HashSet<PluginView*>::const_iterator last = d->m_pluginViews.end();
    for (; it != last; ++it)
        (*it)->handleForegroundEvent();
}

void WebPage::notifyPageFullScreenAllowed()
{
    HashSet<PluginView*>::const_iterator it = d->m_pluginViews.begin();
    HashSet<PluginView*>::const_iterator last = d->m_pluginViews.end();
    for (; it != last; ++it)
        (*it)->handleFullScreenAllowedEvent();
}

void WebPage::notifyPageFullScreenExit()
{
    HashSet<PluginView*>::const_iterator it = d->m_pluginViews.begin();
    HashSet<PluginView*>::const_iterator last = d->m_pluginViews.end();
    for (; it != last; ++it)
        (*it)->handleFullScreenExitEvent();
}

void WebPage::notifyDeviceIdleStateChange(bool enterIdle)
{
    HashSet<PluginView*>::const_iterator it = d->m_pluginViews.begin();
    HashSet<PluginView*>::const_iterator last = d->m_pluginViews.end();
    for (; it != last; ++it)
        (*it)->handleIdleEvent(enterIdle);
}

void WebPage::notifyAppActivationStateChange(ActivationStateType activationState)
{
    HashSet<PluginView*>::const_iterator it = d->m_pluginViews.begin();
    HashSet<PluginView*>::const_iterator last = d->m_pluginViews.end();

#if ENABLE(VIDEO)
    if (activationState == ActivationActive)
        MediaPlayerPrivate::notifyAppActivatedEvent(true);
    else
        MediaPlayerPrivate::notifyAppActivatedEvent(false);
#endif

    for (; it != last; ++it) {
        switch (activationState) {
        case ActivationActive:
            (*it)->handleAppActivatedEvent();
            break;
        case ActivationInactive:
            (*it)->handleAppDeactivatedEvent();
            break;
        case ActivationStandby:
            (*it)->handleAppStandbyEvent();
            break;
        default: // FIXME: Get rid of the default to force a compiler error instead of using a runtime error. See PR #121109.
            ASSERT(0);
            break;
        }
    }
}

void WebPage::notifySwipeEvent()
{
    HashSet<PluginView*>::const_iterator it = d->m_pluginViews.begin();
    HashSet<PluginView*>::const_iterator last = d->m_pluginViews.end();
    for (; it != last; ++it)
       (*it)->handleSwipeEvent();
}

void WebPage::notifyScreenPowerStateChanged(bool powered)
{
    HashSet<PluginView*>::const_iterator it = d->m_pluginViews.begin();
    HashSet<PluginView*>::const_iterator last = d->m_pluginViews.end();
    for (; it != last; ++it)
       (*it)->handleScreenPowerEvent(powered);
}

void WebPage::notifyFullScreenVideoExited(bool done)
{
#if ENABLE(VIDEO)
    if (HTMLMediaElement* mediaElement = static_cast<HTMLMediaElement*>(d->m_fullscreenVideoNode.get()))
        mediaElement->exitFullscreen();
#endif
}

void WebPage::clearPluginSiteData()
{
    PluginDatabase* database = PluginDatabase::installedPlugins(true);

    if (!database)
        return;

    Vector<PluginPackage*> plugins = database->plugins();

    Vector<PluginPackage*>::const_iterator end = plugins.end();
    for (Vector<PluginPackage*>::const_iterator it = plugins.begin(); it != end; ++it)
        (*it)->clearSiteData(String());
}

void WebPage::onInputLocaleChanged(bool isRTL)
{
    d->onInputLocaleChanged(isRTL);
}

void WebPage::onNetworkAvailabilityChanged(bool available)
{
    updateOnlineStatus(available);
}

void WebPage::onCertificateStoreLocationSet(const WebString& caPath)
{
#if ENABLE(VIDEO)
    MediaPlayerPrivate::setCertificatePath(caPath);
#endif
}

void WebPage::enableWebInspector()
{
    d->m_page->inspectorController()->connectFrontend();
    d->m_page->settings()->setDeveloperExtrasEnabled(true);
}

void WebPage::disableWebInspector()
{
    d->m_page->inspectorController()->disconnectFrontend();
    d->m_page->settings()->setDeveloperExtrasEnabled(false);
}

void WebPage::enablePasswordEcho()
{
    d->m_page->settings()->setPasswordEchoEnabled(true);
}

void WebPage::disablePasswordEcho()
{
    d->m_page->settings()->setPasswordEchoEnabled(false);
}

void WebPage::dispatchInspectorMessage(const char* message, int length)
{
    String stringMessage(message, length);
    d->m_page->inspectorController()->dispatchMessageFromFrontend(stringMessage);
}

Frame* WebPage::mainFrame() const
{
    return d->m_mainFrame;
}

#if USE(ACCELERATED_COMPOSITING)
void WebPagePrivate::drawLayersOnCommit()
{
    if (!BlackBerry::Platform::userInterfaceThreadMessageClient()->isCurrentThread()) {
        // This method will only be called when the layer appearance changed due to
        // animations. And only if we don't need a one shot drawing sync.
        ASSERT(!needsOneShotDrawingSynchronization());

        if (!m_webPage->isVisible() || !m_backingStore->d->isActive())
            return;

        m_backingStore->d->willDrawLayersOnCommit();

        BlackBerry::Platform::userInterfaceThreadMessageClient()->dispatchMessage(
            BlackBerry::Platform::createMethodCallMessage(&WebPagePrivate::drawLayersOnCommit, this));
        return;
    }

    if (m_client->window()->windowUsage() == BlackBerry::Platform::Graphics::Window::GLES2Usage) {
        m_backingStore->d->blitVisibleContents();
        return; // blitVisibleContents() includes drawSubLayers() in this case.
    }

    if (!drawSubLayers())
        return;

    // If we use the compositing surface, we need to re-blit the
    // backingstore and blend the compositing surface on top of that
    // in order to get the newly drawn layers on screen.
    if (!SurfacePool::globalSurfacePool()->compositingSurface())
        return;

    // If there are no visible layers, return early.
    if (lastCompositingResults().isEmpty() && lastCompositingResults().wasEmpty)
        return;

    if (m_backingStore->d->shouldDirectRenderingToWindow())
        return;

    m_backingStore->d->blitVisibleContents();
}

bool WebPagePrivate::drawSubLayers(const WebCore::IntRect& dstRect, const WebCore::FloatRect& contents)
{
    ASSERT(BlackBerry::Platform::userInterfaceThreadMessageClient()->isCurrentThread());
    if (!BlackBerry::Platform::userInterfaceThreadMessageClient()->isCurrentThread())
        return false;

    if (m_compositor) {
        m_compositor->setCompositingOntoMainWindow(
            m_client->window()->windowUsage() == BlackBerry::Platform::Graphics::Window::GLES2Usage);
        return m_compositor->drawLayers(dstRect, contents);
    }

    return false;
}

bool WebPagePrivate::drawSubLayers()
{
    ASSERT(BlackBerry::Platform::userInterfaceThreadMessageClient()->isCurrentThread());
    if (!BlackBerry::Platform::userInterfaceThreadMessageClient()->isCurrentThread())
        return false;

    return m_backingStore->d->drawSubLayers();
}

void WebPagePrivate::scheduleRootLayerCommit()
{
    if (!m_frameLayers || !m_frameLayers->hasLayer())
        return;

    m_needsCommit = true;
    if (!m_rootLayerCommitTimer->isActive())
        m_rootLayerCommitTimer->startOneShot(0);
}

static bool needsLayoutRecursive(FrameView* view)
{
    if (view->needsLayout())
        return true;

    bool subframesNeedsLayout = false;
    const HashSet<RefPtr<Widget> >* viewChildren = view->children();
    HashSet<RefPtr<Widget> >::const_iterator end = viewChildren->end();
    for (HashSet<RefPtr<Widget> >::const_iterator current = viewChildren->begin(); current != end && !subframesNeedsLayout; ++current) {
        Widget* widget = (*current).get();
        if (widget->isFrameView())
            subframesNeedsLayout |= needsLayoutRecursive(static_cast<FrameView*>(widget));
    }

    return subframesNeedsLayout;
}

WebCore::LayerRenderingResults WebPagePrivate::lastCompositingResults() const
{
    if (m_compositor)
        return m_compositor->lastCompositingResults();
    return WebCore::LayerRenderingResults();
}

void WebPagePrivate::commitRootLayer(const WebCore::IntRect& layoutRectForCompositing,
                                     const WebCore::IntSize& contentsSizeForCompositing)
{
    if (!m_frameLayers || !m_compositor)
        return;

    m_compositor->setLayoutRectForCompositing(layoutRectForCompositing);
    m_compositor->setContentsSizeForCompositing(contentsSizeForCompositing);
    m_compositor->commit(m_frameLayers->rootLayer());
}

bool WebPagePrivate::commitRootLayerIfNeeded()
{
    if (m_suspendRootLayerCommit)
        return false;

    if (!m_needsCommit)
        return false;

    if (!m_frameLayers || !m_frameLayers->hasLayer())
        return false;

    FrameView* view = m_mainFrame->view();
    if (!view)
        return false;

    // If we sync compositing layers when a layout is pending, we may cause painting of compositing
    // layer content to occur before layout has happened, which will cause paintContents() to bail.
    if (needsLayoutRecursive(view)) {
        // In case of one shot drawing synchronization, you
        // should first layoutIfNeeded, render, then commit and draw the layers.
        ASSERT(!needsOneShotDrawingSynchronization());
        return false;
    }

    m_needsCommit = false;
    // We get here either due to the commit timer, which would have called
    // render if a one shot sync was needed. Or we get called from render
    // before the timer times out, which means we are doing a one shot anyway.
    m_needsOneShotDrawingSynchronization = false;

    if (m_rootLayerCommitTimer->isActive())
        m_rootLayerCommitTimer->stop();

    m_frameLayers->commitOnWebKitThread(currentScale());
    updateDelegatedOverlays();

    // Stash the visible content rect according to webkit thread
    // This is the rectangle used to layout fixed positioned elements,
    // and that's what the layer renderer wants.
    IntRect layoutRectForCompositing(scrollPosition(), actualVisibleSize());
    IntSize contentsSizeForCompositing = contentsSize();

    // Commit changes made to the layers synchronously with the compositing thread.
    BlackBerry::Platform::userInterfaceThreadMessageClient()->dispatchSyncMessage(
        BlackBerry::Platform::createMethodCallMessage(
            &WebPagePrivate::commitRootLayer,
            this,
            layoutRectForCompositing,
            contentsSizeForCompositing));

    return true;
}

void WebPagePrivate::rootLayerCommitTimerFired(Timer<WebPagePrivate>*)
{
    if (m_suspendRootLayerCommit)
        return;

    // The commit timer may have fired just before the layout timer, or for some
    // other reason we need layout. It's not allowed to commit when a layout is
    // pending, becaues a commit can cause parts of the web page to be rendered
    // to texture.
    // The layout can also turn of compositing altogether, so we need to be prepared
    // to handle a one shot drawing synchronization after the layout.
    requestLayoutIfNeeded();

    bool isSingleTargetWindow = SurfacePool::globalSurfacePool()->compositingSurface()
        || m_client->window()->windowUsage() == BlackBerry::Platform::Graphics::Window::GLES2Usage;

    // If we are doing direct rendering and have a single rendering target,
    // committing is equivalent to a one shot drawing synchronization.
    // We need to re-render the web page, re-render the layers, and
    // then blit them on top of the re-rendered web page.
    if (isSingleTargetWindow && m_backingStore->d->shouldDirectRenderingToWindow())
        setNeedsOneShotDrawingSynchronization();

    if (needsOneShotDrawingSynchronization()) {
        const IntRect windowRect = IntRect(IntPoint(0, 0), viewportSize());
        m_backingStore->d->repaint(windowRect, true /*contentChanged*/, true /*immediate*/);
        return;
    }

    // If the web page needs layout, the commit will fail.
    // No need to draw the layers if nothing changed.
    if (commitRootLayerIfNeeded())
        drawLayersOnCommit();
}

void WebPagePrivate::setIsAcceleratedCompositingActive(bool active)
{
    // Backing store can be null here because it happens during teardown.
    if (m_isAcceleratedCompositingActive == active || !m_backingStore)
        return;

    m_isAcceleratedCompositingActive = active;

    ASSERT(m_client);
#if !ENABLE_COMPOSITING_SURFACE
    ASSERT(m_client->compositingWindow());
    if (m_client && m_client->compositingWindow() && m_client->window() != m_client->compositingWindow())
        m_client->compositingWindow()->setVisible(active);
#endif

    if (!active) {
        m_compositor.clear();
        resetCompositingSurface();
        return;
    }

    if (!m_compositor) {
        m_compositor = adoptPtr(new WebPageCompositor(this));
        m_isAcceleratedCompositingActive = m_compositor->hardwareCompositing();
        if (!m_isAcceleratedCompositingActive)
            m_compositor.clear();
    }
}

void WebPagePrivate::resetCompositingSurface()
{
    if (!BlackBerry::Platform::userInterfaceThreadMessageClient()->isCurrentThread()) {
        BlackBerry::Platform::userInterfaceThreadMessageClient()->dispatchMessage(
            BlackBerry::Platform::createMethodCallMessage(
                &WebPagePrivate::resetCompositingSurface, this));
        return;
    }

    if (m_compositor)
        m_compositor->setLastCompositingResults(WebCore::LayerRenderingResults());
}

void WebPagePrivate::setRootLayerWebKitThread(Frame* frame, LayerWebKitThread* layer)
{
    // This method updates the FrameLayers based on input from WebCore.
    // FrameLayers keeps track of the layer proxies attached to frames.
    // We will have to compute a new root layer and update the compositor.
    if (!layer && !m_frameLayers)
        return;

    if (!layer) {
         ASSERT(m_frameLayers);
         m_frameLayers->removeLayerByFrame(frame);
         if (!m_frameLayers->hasLayer())
             m_frameLayers.clear();
    } else {
        if (!m_frameLayers)
            m_frameLayers = adoptPtr(new FrameLayers(this));

        if (!m_frameLayers->containsLayerForFrame(frame))
            m_frameLayers->addLayer(frame, layer);

        ASSERT(m_frameLayers);
    }

    LayerCompositingThread* rootLayerCompositingThread = 0;
    if (m_frameLayers && m_frameLayers->rootLayer())
        rootLayerCompositingThread = m_frameLayers->rootLayer()->layerCompositingThread();

    setRootLayerCompositingThread(rootLayerCompositingThread);
}

void WebPagePrivate::setRootLayerCompositingThread(LayerCompositingThread* layer)
{
    if (!BlackBerry::Platform::userInterfaceThreadMessageClient()->isCurrentThread()) {
        BlackBerry::Platform::userInterfaceThreadMessageClient()->dispatchSyncMessage(
            BlackBerry::Platform::createMethodCallMessage(
                &WebPagePrivate::setRootLayerCompositingThread, this, layer));
        return;
    }

    // Depending on whether we have a root layer or not,
    // this method will turn on or off accelerated compositing.
    if (!layer) {
         // Don't ASSERT(m_compositor) here because we may be called in
         // the process of destruction of WebPage where we have already
         // called syncDestroyCompositorOnCompositingThread() to destroy
         // the compositor.
         setIsAcceleratedCompositingActive(false);
         return;
    }

    if (!m_compositor)
        setIsAcceleratedCompositingActive(true);

    // Don't ASSERT(m_compositor) here because setIsAcceleratedCompositingActive(true)
    // may not turn accelerated compositing on since m_backingStore is 0.
    if (m_compositor)
        m_compositor->setRootLayer(layer);
}

void WebPagePrivate::destroyCompositor()
{
     m_compositor.clear();
}

void WebPagePrivate::syncDestroyCompositorOnCompositingThread()
{
    if (!m_compositor)
        return;

    BlackBerry::Platform::userInterfaceThreadMessageClient()->dispatchSyncMessage(
        BlackBerry::Platform::createMethodCallMessage(
            &WebPagePrivate::destroyCompositor, this));
}

void WebPagePrivate::destroyLayerResources()
{
     m_compositor->releaseLayerResources();
}

void WebPagePrivate::suspendRootLayerCommit()
{
    if (m_suspendRootLayerCommit)
        return;

    m_suspendRootLayerCommit = true;

    if (!m_frameLayers || !m_frameLayers->hasLayer() || !m_compositor)
        return;

    BlackBerry::Platform::userInterfaceThreadMessageClient()->dispatchSyncMessage(
        BlackBerry::Platform::createMethodCallMessage(
            &WebPagePrivate::destroyLayerResources, this));
}

void WebPagePrivate::resumeRootLayerCommit()
{
    if (!m_suspendRootLayerCommit)
        return;

    m_suspendRootLayerCommit = false;
    m_needsCommit = true;

    // Recreate layer resources if needed.
    commitRootLayerIfNeeded();
}

bool WebPagePrivate::needsOneShotDrawingSynchronization()
{
    return m_needsOneShotDrawingSynchronization;
}

void WebPagePrivate::setNeedsOneShotDrawingSynchronization()
{
    // This means we have to commit layers on next render, or render on the next commit,
    // whichever happens first.
    m_needsCommit = true;
    m_needsOneShotDrawingSynchronization = true;
}
#endif // USE(ACCELERATED_COMPOSITING)

void WebPagePrivate::enterFullscreenForNode(Node* node)
{
#if ENABLE(VIDEO)
    if (!node || !node->hasTagName(HTMLNames::videoTag))
        return;

    MediaPlayer* player = static_cast<HTMLMediaElement*>(node)->player();
    if (!player)
        return;

    MediaPlayerPrivate* mmrPlayer = static_cast<MediaPlayerPrivate*>(player->implementation());
    if (!mmrPlayer)
        return;

    BlackBerry::Platform::Graphics::Window* window = mmrPlayer->windowGet();
    if (!window)
        return;

    unsigned x, y, width, height;
    mmrPlayer->windowPositionGet(x, y, width, height);

    const char* contextName = mmrPlayer->mmrContextNameGet();
    if (!contextName)
        return;

    mmrPlayer->setFullscreenWebPageClient(m_client);
    m_fullscreenVideoNode = node;
    m_client->fullscreenStart(contextName, window, x, y, width, height);
#endif
}

void WebPagePrivate::exitFullscreenForNode(Node* node)
{
#if ENABLE(VIDEO)
    if (m_fullscreenVideoNode.get()) {
        m_client->fullscreenStop();
        m_fullscreenVideoNode = 0;
    }

    if (!node || !node->hasTagName(HTMLNames::videoTag))
        return;

    MediaPlayer* player = static_cast<HTMLMediaElement*>(node)->player();
    if (!player)
        return;

    MediaPlayerPrivate* mmrPlayer = static_cast<MediaPlayerPrivate*>(player->implementation());
    if (!mmrPlayer)
        return;

    // Fullscreen mode is being turned off, so MediaPlayerPrivate no longer needs the pointer.
    mmrPlayer->setFullscreenWebPageClient(0);
#endif
}

// MARK: WebSettingsDelegate.

void WebPagePrivate::didChangeSettings(WebSettings* webSettings)
{
    Settings* coreSettings = m_page->settings();
    m_page->setGroupName(webSettings->pageGroupName());
    coreSettings->setXSSAuditorEnabled(webSettings->xssAuditorEnabled());
    coreSettings->setLoadsImagesAutomatically(webSettings->loadsImagesAutomatically());
    coreSettings->setShouldDrawBorderWhileLoadingImages(webSettings->shouldDrawBorderWhileLoadingImages());
    coreSettings->setScriptEnabled(webSettings->isJavaScriptEnabled());
    coreSettings->setPrivateBrowsingEnabled(webSettings->isPrivateBrowsingEnabled());
    coreSettings->setDefaultFixedFontSize(webSettings->defaultFixedFontSize());
    coreSettings->setDefaultFontSize(webSettings->defaultFontSize());
    coreSettings->setMinimumFontSize(webSettings->minimumFontSize());
    coreSettings->setSerifFontFamily(webSettings->serifFontFamily().impl());
    coreSettings->setFixedFontFamily(webSettings->fixedFontFamily().impl());
    coreSettings->setSansSerifFontFamily(webSettings->sansSerifFontFamily().impl());
    coreSettings->setStandardFontFamily(webSettings->standardFontFamily().impl());
    coreSettings->setJavaScriptCanOpenWindowsAutomatically(webSettings->canJavaScriptOpenWindowsAutomatically());
    coreSettings->setAllowScriptsToCloseWindows(webSettings->canJavaScriptOpenWindowsAutomatically()); // Why are we using the same value as setJavaScriptCanOpenWindowsAutomatically()?
    coreSettings->setPluginsEnabled(webSettings->arePluginsEnabled());
    coreSettings->setDefaultTextEncodingName(webSettings->defaultTextEncodingName().impl());
    coreSettings->setDownloadableBinaryFontsEnabled(webSettings->downloadableBinaryFontsEnabled());
    coreSettings->setSpatialNavigationEnabled(m_webSettings->isSpatialNavigationEnabled());

    // UserScalable should be reset by new settings.
    setUserScalable(webSettings->isUserScalable());

    WebString stylesheetURL = webSettings->userStyleSheetString();
    if (stylesheetURL.isEmpty())
        stylesheetURL = webSettings->userStyleSheetLocation();
    if (!stylesheetURL.isEmpty())
        coreSettings->setUserStyleSheetLocation(KURL(KURL(), stylesheetURL));

    coreSettings->setFirstScheduledLayoutDelay(webSettings->firstScheduledLayoutDelay());
    coreSettings->setUseCache(webSettings->useWebKitCache());

#if ENABLE(SQL_DATABASE)
    // DatabaseTracker can only be initialized for once, so it doesn't
    // make sense to change database path after DatabaseTracker has
    // already been initialized.
    static bool dbinit = false;
    if (!dbinit && !webSettings->databasePath().isEmpty()) {
        dbinit = true;
        DatabaseTracker::initializeTracker(webSettings->databasePath());
    }

    // The directory of cacheStorage for one page group can only be initialized once.
    static bool acinit = false;
    if (!acinit && !webSettings->appCachePath().isEmpty()) {
        acinit = true;
        WebCore::cacheStorage().setCacheDirectory(webSettings->appCachePath());
    }

    coreSettings->setLocalStorageDatabasePath(webSettings->localStoragePath());
    WebCore::Database::setIsAvailable(webSettings->isDatabasesEnabled());
    WebCore::DatabaseSync::setIsAvailable(webSettings->isDatabasesEnabled());

    coreSettings->setLocalStorageEnabled(webSettings->isLocalStorageEnabled());
    coreSettings->setOfflineWebApplicationCacheEnabled(webSettings->isAppCacheEnabled());

    m_page->group().groupSettings()->setLocalStorageQuotaBytes(webSettings->localStorageQuota());
    coreSettings->setUsesPageCache(webSettings->maximumPagesInCache());
    coreSettings->setFrameFlatteningEnabled(webSettings->isFrameFlatteningEnabled());
#endif

#if ENABLE(WEB_SOCKETS)
    WebCore::WebSocket::setIsAvailable(webSettings->areWebSocketsEnabled());
#endif

#if ENABLE(VIEWPORT_REFLOW)
    coreSettings->setTextReflowEnabled(webSettings->textReflowMode() == WebSettings::TextReflowEnabled);
#endif

    // FIXME: We don't want HTMLTokenizer to yield for anything other than email case because
    // call to currentTime() is too expensive on our platform. See RIM Bug #746.
    coreSettings->setShouldUseFirstScheduledLayoutDelay(webSettings->isEmailMode());
    coreSettings->setProcessHTTPEquiv(!webSettings->isEmailMode());

    coreSettings->setShouldUseCrossOriginProtocolCheck(!webSettings->allowCrossSiteRequests());

    WebCore::cookieManager().setPrivateMode(webSettings->isPrivateBrowsingEnabled());

    if (m_mainFrame && m_mainFrame->view()) {
        Color backgroundColor(webSettings->backgroundColor());
        m_mainFrame->view()->updateBackgroundRecursively(backgroundColor, backgroundColor.hasAlpha());
    }
}

IntSize WebPagePrivate::defaultMaxLayoutSize()
{
    static IntSize size;

    if (size.isEmpty())
        size = IntSize(std::max(1024, BlackBerry::Platform::Graphics::Screen::landscapeWidth()),
                       std::max(768, BlackBerry::Platform::Graphics::Screen::landscapeHeight()));

    return size;
}

WebString WebPage::textHasAttribute(const WebString& query) const
{
    Document* doc = d->m_page->focusController()->focusedOrMainFrame()->document();
    if (doc)
        return doc->queryCommandValue(query);
    return "";
}

void WebPage::setJavaScriptCanAccessClipboard(bool enabled)
{
    d->m_page->settings()->setJavaScriptCanAccessClipboard(enabled);
}

#if USE(ACCELERATED_COMPOSITING)
void WebPagePrivate::blitVisibleContents()
{
    if (m_backingStore->d->shouldDirectRenderingToWindow())
        return;

    m_backingStore->d->blitVisibleContents();
}
#endif

void WebPage::setWebGLEnabled(bool enabled)
{
    if (!BlackBerry::Platform::ITPolicy::isWebGLEnabled()) {
        d->m_page->settings()->setWebGLEnabled(false);
        return;
    }
    d->m_page->settings()->setWebGLEnabled(enabled);
}

bool WebPage::isWebGLEnabled() const
{
    return d->m_page->settings()->webGLEnabled();
}

void WebPagePrivate::setNeedTouchEvents(bool value)
{
    m_needTouchEvents = value;
}

}
}
