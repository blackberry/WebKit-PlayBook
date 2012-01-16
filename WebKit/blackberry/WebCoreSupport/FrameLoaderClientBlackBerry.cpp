/*
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 */

#include "config.h"
#include "FrameLoaderClientBlackBerry.h"

#include "BackingStore.h"
#include "BackingStoreClient.h"
#include "BackingStore_p.h"
#include "BackForwardList.h"
#include "BlackBerryPlatformScreen.h"
#include "Base64.h"
#include "ClientExtension.h"
#include "CString.h"
#include "Chrome.h"
#include "ChromeClientBlackBerry.h"
#include "DumpRenderTreeClient.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "HistoryItem.h"
#include "HTMLHeadElement.h"
#include "HTMLLinkElement.h"
#include "HTMLMediaElement.h"
#include "HTMLMetaElement.h"
#include "HTMLPlugInElement.h"
#include "HTTPParsers.h"
#include "IconDatabase.h"
#include "Image.h"
#include "ImageSource.h"
#include "InputHandler.h"
#include "MIMETypeRegistry.h"
#include "FrameNetworkingContextBlackBerry.h"
#include "NativeImageSkia.h"
#include "NetworkManager.h"
#include "NodeList.h"
#include "NotImplemented.h"
#include "PluginView.h"
#include "PluginWidget.h"
#include "ProgressTracker.h"
#include "RenderEmbeddedObject.h"
#include "ResourceError.h"
#include "ScopePointer.h"
#include "SecurityOrigin.h"
#include "SharedBuffer.h"
#include "TextEncoding.h"
#include "WebPage.h"
#include "WebPage_p.h"
#include "WebPageClient.h"
#include "WebPlugin.h"
#include "WebSettings.h"
#if !OS(QNX)
#include "WebDOMDocument.h"
#endif
#include "JavaScriptCore/APICast.h"
#include "streams/FilterStream.h"
#include "streams/NetworkRequest.h"

#include <BlackBerryPlatformNavigationType.h>
#include <SkBitmap.h>


using WTF::String;
using namespace WebCore;
using namespace Olympia::WebKit;

namespace WebCore {

FrameLoaderClientBlackBerry::FrameLoaderClientBlackBerry()
    : m_frame(0)
    , m_webPage(0)
    , m_sentReadyToRender(false)
    , m_pendingFragmentScrollPolicyFunction(0)
    , m_loadingErrorPage(false)
    , m_clientRedirectIsPending(false)
    , m_clientRedirectIsUserGesture(false)
    , m_pluginView(0)
    , m_hasSentResponseToPlugin(false)
    , m_cancelLoadOnNextData(false)
{
#if !OS(QNX)
    m_invalidateBackForwardTimer = new Timer<FrameLoaderClientBlackBerry>(this, &FrameLoaderClientBlackBerry::invalidateBackForwardTimerFired);
#endif
    m_deferredJobsTimer = new Timer<FrameLoaderClientBlackBerry>(this, &FrameLoaderClientBlackBerry::deferredJobsTimerFired);
}

FrameLoaderClientBlackBerry::~FrameLoaderClientBlackBerry()
{
#if !OS(QNX)
    delete m_invalidateBackForwardTimer;
    m_invalidateBackForwardTimer = 0;
#endif
    delete m_deferredJobsTimer;
    m_deferredJobsTimer = 0;
}

int FrameLoaderClientBlackBerry::playerId() const
{
    if (m_webPage && m_webPage->client())
        return m_webPage->client()->getInstanceId();
    return 0;
}

bool FrameLoaderClientBlackBerry::cookiesEnabled() const
{
    return m_webPage->settings()->areCookiesEnabled();
}

void FrameLoaderClientBlackBerry::dispatchDidAddBackForwardItem(HistoryItem* item) const
{
    // inform the client that the back/forward list has changed
    invalidateBackForwardList();
}

void FrameLoaderClientBlackBerry::dispatchDidRemoveBackForwardItem(HistoryItem* item) const
{
    invalidateBackForwardList();
}

void FrameLoaderClientBlackBerry::dispatchDidChangeBackForwardIndex() const
{
    invalidateBackForwardList();
}

void FrameLoaderClientBlackBerry::dispatchDidChangeLocationWithinPage()
{
    if (!isMainFrame())
        return;

    WTF::String url = m_frame->loader()->url().string();
    WTF::String token = m_frame->loader()->documentLoader()->request().token();

    m_webPage->client()->notifyLoadToAnchor(url.characters(), url.length(), token.characters(), token.length());
}

void FrameLoaderClientBlackBerry::dispatchDidPushStateWithinPage()
{
    //HACK: abuse anchor navigation to achieve history push
    dispatchDidChangeLocationWithinPage();
}

void FrameLoaderClientBlackBerry::dispatchDidReplaceStateWithinPage()
{
    //HACK: abuse anchor navigation to achieve history replace
    dispatchDidChangeLocationWithinPage();
}

void FrameLoaderClientBlackBerry::dispatchDidPopStateWithinPage()
{
    // not needed
}

void FrameLoaderClientBlackBerry::dispatchDidCancelClientRedirect()
{
    m_clientRedirectIsPending = false;
    m_clientRedirectIsUserGesture = false;
}

void FrameLoaderClientBlackBerry::dispatchWillPerformClientRedirect(const KURL& url, double, double)
{
    m_clientRedirectIsPending = true;
    m_clientRedirectIsUserGesture = m_frame->loader()->isProcessingUserGesture();

    if (!isMainFrame())
        return;

    // FIXME: The client redirect may be cancelled, and in any case may not
    // happen for several seconds if the interval and fireDate parameters are
    // set.  Should not send this notification until it is time.
    // See RIM Bug #806
    WTF::String originalUrl = m_frame->loader()->url().string();
    WTF::String finalUrl = url.string();
    m_webPage->client()->notifyClientRedirect(originalUrl.characters(), originalUrl.length(),
            finalUrl.characters(), finalUrl.length());
}

void FrameLoaderClientBlackBerry::dispatchDecidePolicyForMIMEType(FramePolicyFunction function, const WTF::String& mimeType, const ResourceRequest& request)
{
    // FIXME: What should we do for HTTP status code 204 and 205 and "application/zip"?
#if PLATFORM(BLACKBERRY) && OS(QNX)
    PolicyAction policy = PolicyIgnore;

    const ResourceResponse& response = m_frame->loader()->activeDocumentLoader()->response();
    if (WebCore::contentDispositionType(response.httpHeaderField("Content-Disposition")) == WebCore::ContentDispositionAttachment)
        policy = PolicyDownload;
    else if (canShowMIMEType(mimeType))
        policy = PolicyUse;
    else if ((ResourceRequestBase::TargetIsMainFrame == request.targetType())
             && m_webPage->client()->downloadAllowed(request.url().string().utf8().data()))
        policy = PolicyDownload;

    (m_frame->loader()->policyChecker()->*function)(policy);
#else
    (m_frame->loader()->policyChecker()->*function)(canShowMIMEType(mimeType) ? PolicyUse : PolicyIgnore);
#endif
}

void FrameLoaderClientBlackBerry::dispatchDecidePolicyForNavigationAction(FramePolicyFunction function, const NavigationAction& action, const ResourceRequest& request, PassRefPtr<FormState>)
{
    PolicyAction decision = PolicyUse;

    const KURL& url = request.url();

    // Fragment scrolls on the same page should always be handled internally.
    // (Only count as a fragment scroll if we are scrolling to a #fragment url, not back to the top, and reloading
    // the same url is not a fragment scroll even if it has a #fragment.)
    const KURL& currentUrl = m_frame->loader()->url();
    bool isFragmentScroll = url.hasFragmentIdentifier() && url != currentUrl && equalIgnoringFragmentIdentifier(currentUrl, url);
    if (decision == PolicyUse)
        decision = decidePolicyForExternalLoad(request, isFragmentScroll);

#if OS(BLACKBERRY)
    // GS: Warning, this is buggy!  You do not want this.  Really.  You don't.

    // If the client has not delivered all data to the main frame, a fragment
    // scroll to an anchor that hasn't arrived yet will fail. Kick the client
    // and try again.
    if (decision == PolicyUse && isFragmentScroll && isMainFrame()) {
        WTF::String fragment = url.fragmentIdentifier();
        if (fragment != currentUrl.fragmentIdentifier()) {
            delayPolicyCheckUntilFragmentExists(fragment, function);
            return;
        }
    }
#endif

    // Let the client have a chance to say whether this navigation should take
    // be ignored or not

    Olympia::Platform::NetworkRequest platformRequest;
    request.initializePlatformRequest(platformRequest, false /*isInitial*/);
    if (isMainFrame() && !m_webPage->client()->acceptNavigationRequest(
        platformRequest, Olympia::Platform::NavigationType(action.type()))) {
        if (action.type() == NavigationTypeFormSubmitted
            || action.type() == NavigationTypeFormResubmitted)
            m_frame->loader()->resetMultipleFormSubmissionProtection();

        if (action.type() == NavigationTypeLinkClicked && url.hasFragmentIdentifier()) {
            ResourceRequest emptyRequest;
            m_frame->loader()->activeDocumentLoader()->setLastCheckedRequest(emptyRequest);
        }
        decision = PolicyIgnore;
    }

    // If we abort here,  dispatchDidCancelClientRedirect will not be called.
    // So call it by hand.
    if (decision == PolicyIgnore)
        dispatchDidCancelClientRedirect();

    (m_frame->loader()->policyChecker()->*function)(decision);
}

void FrameLoaderClientBlackBerry::delayPolicyCheckUntilFragmentExists(const WTF::String& fragment, FramePolicyFunction function)
{
    ASSERT(isMainFrame());

    if (m_webPage->d->loadState() < WebPagePrivate::Finished && !m_frame->document()->findAnchor(fragment)) {
        // tell the client we need more data, in case the fragment exists but is being held back
        m_webPage->client()->needMoreData();
        m_pendingFragmentScrollPolicyFunction = function;
        m_pendingFragmentScroll = fragment;
        return;
    }

    (m_frame->loader()->policyChecker()->*function)(PolicyUse);
}

void FrameLoaderClientBlackBerry::cancelPolicyCheck()
{
    m_pendingFragmentScrollPolicyFunction = 0;
    m_pendingFragmentScroll = WTF::String();
}

void FrameLoaderClientBlackBerry::doPendingFragmentScroll()
{
    if (m_pendingFragmentScroll.isNull())
        return;

    // make sure to clear the pending members first to avoid recursion
    WTF::String fragment = m_pendingFragmentScroll;
    m_pendingFragmentScroll = WTF::String();

    FramePolicyFunction function = m_pendingFragmentScrollPolicyFunction;
    m_pendingFragmentScrollPolicyFunction = 0;

    delayPolicyCheckUntilFragmentExists(fragment, function);
}

void FrameLoaderClientBlackBerry::dispatchDecidePolicyForNewWindowAction(FramePolicyFunction function, const NavigationAction&, const ResourceRequest& request, PassRefPtr<FormState>, const String& frameName)
{
    // a new window can never be a fragment scroll
    PolicyAction decision = decidePolicyForExternalLoad(request, false);
    (m_frame->loader()->policyChecker()->*function)(decision);
}

void FrameLoaderClientBlackBerry::committedLoad(DocumentLoader* loader, const char* data, int length)
{
	// The structure of this code may seem...a bit odd.  It's structured with two checks on the state
	// of m_pluginView because it's actually receivedData that may cause the request to re-direct data
	// to a PluginView.  This is because receivedData may decide to create a PluginDocument containing
	// a PluginView.  The PluginView will request that the main resource for the frame be redirected
	// to the PluginView.  So after receivedData has been called, this code needs to check whether
	// re-direction to a PluginView has been requested and pass the same data on to the PluginView.
	// Thereafter, all data will be re-directed to the PluginView; i.e., no additional data will go
	// to receivedData.

    if (!m_pluginView) {
        const WTF::String& textEncoding = loader->response().textEncodingName();
        receivedData(data, length, textEncoding);
    }

    if (m_pluginView) {
        if (!m_hasSentResponseToPlugin) {
            m_pluginView->didReceiveResponse(loader->response());
            m_hasSentResponseToPlugin = true;
        }

        if (!m_pluginView)
            return;

        m_pluginView->didReceiveData(data, length);
    }
}

PassRefPtr<Widget> FrameLoaderClientBlackBerry::createPlugin(const IntSize& pluginSize,
    HTMLPlugInElement* element, const KURL& url, const Vector<WTF::String>& paramNames,
    const Vector<WTF::String>& paramValues, const WTF::String& mimeTypeIn, bool loadManually)
{
    WTF::String mimeType(mimeTypeIn);
    if (mimeType.isEmpty()) {
        mimeType = MIMETypeRegistry::getMIMETypeForExtension(url.path().substring(url.path().reverseFind('.') + 1));
        mimeType = Olympia::WebKit::WebSettings::getNormalizedMIMEType(mimeType);
        if (mimeType != "application/x-shockwave-flash")
            mimeType = mimeTypeIn;
    }

    if (mimeType == "application/x-shockwave-flash" || mimeType == "application/jnext-scriptable-plugin") {
        RefPtr<PluginView> pluginView = PluginView::create(m_frame, pluginSize, element, url, paramNames, paramValues, mimeType, loadManually);
        return pluginView;
    } else {
        // Check cached plugin. If shouldCancelled has been set to true,
        // means it can't be loaded as plugin, so load the url in normal way.
        RefPtr<WebPlugin> plugin = getCachedPlugin(element);
        if (plugin && plugin.get()->shouldBeCancelled()) {
            m_frame->loader()->subframeLoader()->requestFrame(element, url, WTF::String());
            return 0;
        }
        return createPluginHolder(pluginSize, static_cast<HTMLElement*>(element), url, paramNames, paramValues, mimeType, false);
    }

    return 0;
}

void FrameLoaderClientBlackBerry::redirectDataToPlugin(Widget* pluginWidget)
{
    ASSERT(!m_pluginView);
    m_pluginView = static_cast<PluginView*>(pluginWidget);
    m_hasSentResponseToPlugin = false;
}

#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
PassRefPtr<Widget> FrameLoaderClientBlackBerry::createMediaPlayerProxyPlugin(const IntSize& pluginSize,
    HTMLMediaElement* element, const KURL& url, const Vector<WTF::String>& paramNames,
    const Vector<WTF::String>& paramValues, const WTF::String& mimeType)
{
    // Check if plugin loading needs to be cancelled.
    RefPtr<WebPlugin> plugin = getCachedPlugin(element);
    if (plugin && plugin.get()->shouldBeCancelled())
        return 0;

    if (!m_frame)
        return 0;

    ASSERT(m_frame->document());

    WTF::String mediaUrl = url.string();
    for (unsigned i = 0; i < paramNames.size(); ++i) {
        if (paramNames[i] == "_media_element_src_") {
            mediaUrl = paramValues[i];
            break;
        }
    }

    Vector<WTF::String> newParamNames = paramNames;
    Vector<WTF::String> newParamValues = paramValues;

    if (element->autoplay()) {
        newParamNames.append("autostart");
        newParamValues.append("true");
    }

    if (element->loop()) {
        newParamNames.append("loop");
        newParamValues.append("true");
    }

    bool isRendererNeeded = element->controls() || element->renderer();

    return createPluginHolder(pluginSize, static_cast<HTMLElement*>(element), m_frame->document()->completeURL(mediaUrl), newParamNames, newParamValues, WTF::String(), mediaUrl.isEmpty());
}

void FrameLoaderClientBlackBerry::hideMediaPlayerProxyPlugin(Widget* widget)
{
    notImplemented();
}

void FrameLoaderClientBlackBerry::showMediaPlayerProxyPlugin(Widget* widget)
{
    notImplemented();
}

#endif

void FrameLoaderClientBlackBerry::mediaReadyStateChanged(int id, int state)
{
    for (HashMap<HTMLElement*, RefPtr<Olympia::WebKit::WebPlugin> >::iterator iter = m_cachedPlugins.begin(); iter != m_cachedPlugins.end(); ++iter) {
        WebPlugin* plugin = iter->second.get();
        if (plugin->playerID() == id) {
            plugin->readyStateChanged(state);
            return;
        }
    }
}

void FrameLoaderClientBlackBerry::mediaVolumeChanged(int id, int volume)
{
    for (HashMap<HTMLElement*, RefPtr<Olympia::WebKit::WebPlugin> >::iterator iter = m_cachedPlugins.begin(); iter != m_cachedPlugins.end(); ++iter) {
        WebPlugin* plugin = iter->second.get();
        if (plugin->playerID() == id) {
            plugin->volumeChanged(volume);
            return;
        }
    }
}

void FrameLoaderClientBlackBerry::mediaDurationChanged(int id, float duration)
{
    for (HashMap<HTMLElement*, RefPtr<Olympia::WebKit::WebPlugin> >::iterator iter = m_cachedPlugins.begin(); iter != m_cachedPlugins.end(); ++iter) {
        WebPlugin* plugin = iter->second.get();
        if (plugin->playerID() == id) {
            plugin->durationChanged(duration);
            return;
        }
    }
}

PassRefPtr<Widget> FrameLoaderClientBlackBerry::createPluginHolder(const IntSize& pluginSize,HTMLElement* element, const KURL& url, const Vector<WTF::String>& paramNames,const Vector<WTF::String>& paramValues, const WTF::String& mimeType, bool isPlaceHolder)
{
    // Check cached plugin, and stop creating more if has exceeded the limit.
    if (m_cachedPlugins.size() >= (int)m_webPage->settings()->maxPluginInstances())
        return 0;

    if (!m_frame)
        return 0;

    // Check MIMEType again, because if an object's content can't be handled and it has no fallback, 
    // FrameLoader::requestObject will let it go here.
    if (!isPlaceHolder && objectContentType(url, mimeType) != ObjectContentOtherPlugin)
        return 0;

    
    int width = pluginSize.width();
    int height = pluginSize.height();

    // Check param setting.
    for (unsigned i = 0; i < paramNames.size(); ++i) {
        if (paramNames[i] == "width")
            width = paramValues[i].toInt();
        else if (paramNames[i] == "height")
            height = paramValues[i].toInt();
    }
    
    RefPtr<WebPlugin> plugin = isPlaceHolder ? adoptRef(new WebPlugin(m_webPage, width, height, element, paramNames, paramValues))
        : getCachedPlugin(element);

    if (!plugin && !isPlaceHolder)
        plugin = adoptRef(new WebPlugin(m_webPage, width, height, element, paramNames, paramValues, url.string()));

    if (!plugin)
        return 0;

    m_cachedPlugins.add(element, plugin);

    RefPtr<PluginWidget> w = adoptRef(new PluginWidget());
    w->setPlatformWidget(plugin.get());
    plugin->setWidget(w);
    // Make sure it's invisible until properly placed into the layout.
    w->setFrameRect(IntRect(-1, -1, width, height));
    return w;
}

void FrameLoaderClientBlackBerry::cleanPluginList()
{
    for (HashMap<HTMLElement*, RefPtr<Olympia::WebKit::WebPlugin> >::iterator iter = m_cachedPlugins.begin(); iter != m_cachedPlugins.end(); ++iter) {
        WebPlugin* plugin = iter->second.get();
        plugin->detach();
    }
    m_cachedPlugins.clear();
}

void FrameLoaderClientBlackBerry::cancelLoadingPlugin(int id)
{
    for (HashMap<HTMLElement*, RefPtr<Olympia::WebKit::WebPlugin> >::iterator iter = m_cachedPlugins.begin(); iter != m_cachedPlugins.end(); ++iter) {
        WebPlugin* plugin = iter->second.get();
        if (plugin->playerID() == id) {
            plugin->setShouldBeCancelled(true);
            plugin->detach();
            if (plugin->element() && plugin->element()->renderer()) {
                HTMLElement* objectElement = plugin->element();
                RenderPart* renderer = toRenderPart(objectElement->renderer());
                renderer->setWidget(0);
                renderer->setNeedsLayout(true);
                m_frame->view()->scheduleRelayoutOfSubtree(objectElement->renderer());
            }
            return;
        }
    }
}

PassRefPtr<WebPlugin> FrameLoaderClientBlackBerry::getCachedPlugin(HTMLElement* element)
{
    if (m_cachedPlugins.contains(element))
        return m_cachedPlugins.get(element);

    return 0;
}

void FrameLoaderClientBlackBerry::receivedData(const char* data, int length, const WTF::String& textEncoding)
{
    if (!m_frame)
        return;

    if (m_cancelLoadOnNextData) {
        m_frame->loader()->activeDocumentLoader()->stopLoading();
        m_frame->loader()->writer()->end();
        m_cancelLoadOnNextData = false;
        return;
    }

    // Set the encoding. This only needs to be done once, but it's harmless to do it again later.
    WTF::String encoding = m_frame->loader()->documentLoader()->overrideEncoding();
    bool userChosen = !encoding.isNull();
    if (encoding.isNull())
        encoding = textEncoding;
    m_frame->loader()->writer()->setEncoding(encoding, userChosen);
    m_frame->loader()->writer()->addData(data, length);
}

void FrameLoaderClientBlackBerry::finishedLoading(DocumentLoader* loader)
{
    if (m_pluginView) {
        m_pluginView->didFinishLoading();
        m_pluginView = 0;
        m_hasSentResponseToPlugin = false;
    } else {
        // Telling the frame we received some data and passing 0 as the data is our
        // way to get work done that is normally done when the first bit of data is
        // received, even for the case of a document with no data (like about:blank)
        committedLoad(loader, 0, 0);

        if (!m_frame->document())
            return;
    }
}

WTF::PassRefPtr<DocumentLoader> FrameLoaderClientBlackBerry::createDocumentLoader(const ResourceRequest& request, const SubstituteData& substituteData)
{
    // make a copy of the request with the token from the original request for this frame
    // (unless it already has a token, in which case the request came from the client)
    ResourceRequest newRequest(request);
    if (m_frame && m_frame->loader() && m_frame->loader()->documentLoader()) {
        const ResourceRequest& originalRequest = m_frame->loader()->documentLoader()->originalRequest();
        if (request.token().isNull() && !originalRequest.token().isNull())
            newRequest.setToken(originalRequest.token());
    }

    // FIXME: this should probably be shared
    RefPtr<DocumentLoader> loader = DocumentLoader::create(newRequest, substituteData);
    if (substituteData.isValid())
        loader->setDeferMainResourceDataLoad(false);
    return loader.release();
}

void FrameLoaderClientBlackBerry::frameLoaderDestroyed()
{
    delete this;
}

void FrameLoaderClientBlackBerry::transitionToCommittedForNewPage()
{
    // Clear cached plugins.
    m_cachedPlugins.clear();

    m_cancelLoadOnNextData = false;

    // In Frame::createView, Frame's FrameView object is set to 0 and  recreated.
    // This operation is not atomic, and an attempt to blit contents might happen
    // in the backing store from another thread (see BackingStorePrivate::blitContents method),
    // so we suspend and resume screen update to make sure we do not get a invalid FrameView
    // state.
    BackingStoreClient* backingStoreClientForFrame = m_webPage->d->backingStoreClientForFrame(m_frame);
    if (backingStoreClientForFrame)
        backingStoreClientForFrame->backingStore()->d->suspendScreenAndBackingStoreUpdates();

    m_frame->createView(m_webPage->d->viewportSize(),     /*viewport*/
                        Color(),                          /*background color*/
                        false,                            /*is transparent*/
                        m_webPage->d->actualVisibleSize(),/*fixed reported size*/
                        m_webPage->d->fixedLayoutSize(),  /*fixed layout size*/
                        m_webPage->d->useFixedLayout(),   /*use fixed layout */
                        ScrollbarAlwaysOff,               /*hor mode*/
                        true,                             /*lock the mode*/
                        ScrollbarAlwaysOff,               /*ver mode*/
                        true);                            /*lock the mode*/

    if (backingStoreClientForFrame)
        backingStoreClientForFrame->backingStore()->d->resumeScreenAndBackingStoreUpdates(BackingStore::None);
    m_frame->view()->updateCanHaveScrollbars();

    // Since the mainframe has a tiled backingstore request to receive all update
    // rects instead of the default which just sends update rects for currently
    // visible viewport
    if (isMainFrame())
        m_frame->view()->setPaintsEntireContents(true);
}

WTF::String FrameLoaderClientBlackBerry::userAgent(const KURL&)
{
    return m_webPage->settings()->userAgentString();
}

bool FrameLoaderClientBlackBerry::canHandleRequest(const ResourceRequest&) const
{
    // FIXME: stub
    return true;
}

bool FrameLoaderClientBlackBerry::canShowMIMEType(const WTF::String& mimeTypeIn) const
{
    // Get normalized type.
    WTF::String mimeType = Olympia::WebKit::WebSettings::getNormalizedMIMEType(mimeTypeIn);

    // FIXME: Seems no other port checks empty MIME type in this function. Should we do that?
    return MIMETypeRegistry::isSupportedImageMIMEType(mimeType) ||
        MIMETypeRegistry::isSupportedNonImageMIMEType(mimeType) ||
        MIMETypeRegistry::isSupportedMediaMIMEType(mimeType) ||
        Olympia::WebKit::WebSettings::isSupportedObjectMIMEType(mimeType) ||
        (mimeType == "application/x-shockwave-flash");
}

bool FrameLoaderClientBlackBerry::canShowMIMETypeAsHTML(const WTF::String&) const
{
    // FIXME: stub
    return true;
}


bool FrameLoaderClientBlackBerry::isMainFrame() const
{
    return m_frame == m_webPage->d->m_mainFrame;
}

void FrameLoaderClientBlackBerry::dispatchDidStartProvisionalLoad()
{
    if (isMainFrame())
        m_webPage->d->setLoadState(WebPagePrivate::Provisional);

    if (m_webPage->d->m_dumpRenderTree)
        m_webPage->d->m_dumpRenderTree->didStartProvisionalLoadForFrame(m_frame);
}

void FrameLoaderClientBlackBerry::dispatchDidReceiveTitle(const WTF::String& title)
{
    if (isMainFrame())
        m_webPage->client()->setPageTitle(title.characters(), title.length());

    if (m_webPage->d->m_dumpRenderTree)
        m_webPage->d->m_dumpRenderTree->didReceiveTitleForFrame(title, m_frame);
}

void FrameLoaderClientBlackBerry::dispatchDidCommitLoad()
{
    m_frame->document()->setExtraLayoutDelay(250);

    SecurityOrigin* securityOrigin = m_frame->document()->securityOrigin();
    Olympia::WebKit::WebSettings* webSettings = m_webPage->settings();

    // whitelist all known protocols if BrowserField2 requires unrestricted requests
    if (webSettings->allowCrossSiteRequests() && !securityOrigin->isEmpty()) {
        securityOrigin->addOriginAccessWhitelistEntry(*securityOrigin, "about", "", true);
        securityOrigin->addOriginAccessWhitelistEntry(*securityOrigin, "data", "", true);
        securityOrigin->addOriginAccessWhitelistEntry(*securityOrigin, "file", "", true);
        securityOrigin->addOriginAccessWhitelistEntry(*securityOrigin, "http", "", true);
        securityOrigin->addOriginAccessWhitelistEntry(*securityOrigin, "https", "", true);
        securityOrigin->addOriginAccessWhitelistEntry(*securityOrigin, "javascript", "", true);
        securityOrigin->addOriginAccessWhitelistEntry(*securityOrigin, "local", "", true);
        securityOrigin->addOriginAccessWhitelistEntry(*securityOrigin, "mailto", "", true);
        securityOrigin->addOriginAccessWhitelistEntry(*securityOrigin, "pattern", "", true);
        securityOrigin->addOriginAccessWhitelistEntry(*securityOrigin, "rtcp", "", true);
        securityOrigin->addOriginAccessWhitelistEntry(*securityOrigin, "rtp", "", true);
        securityOrigin->addOriginAccessWhitelistEntry(*securityOrigin, "rtsp", "", true);
        securityOrigin->addOriginAccessWhitelistEntry(*securityOrigin, "sms", "", true);
        securityOrigin->addOriginAccessWhitelistEntry(*securityOrigin, "tel", "", true);

        // We also want to be able to handle unknown protocols in the client.
        securityOrigin->grantUniversalAccess();
    }

    if (isMainFrame()) {
        m_webPage->d->setLoadState(WebPagePrivate::Committed);

        WTF::String originalUrl = m_frame->loader()->documentLoader()->originalRequest().url().string();
        WTF::String url = m_frame->loader()->documentLoader()->request().url().string();
        WTF::String token = m_frame->loader()->documentLoader()->request().token();

        if (HistoryItem* item = m_webPage->d->m_page->globalHistoryItem())
            item->viewState().networkToken = token;

        // notify the client that the load succeeded or failed (if it failed, this
        // is actually committing the error page, which was set through
        // SubstituteData in dispatchDidFailProvisionalLoad)
        if (m_loadingErrorPage) {
            m_loadingErrorPage = false;
            m_webPage->client()->notifyLoadFailedBeforeCommit(
                originalUrl.characters(), originalUrl.length(),
                    url.characters(), url.length(), token.characters(), token.length());
        } else {
            m_webPage->client()->notifyLoadCommitted(
                originalUrl.characters(), originalUrl.length(),
                    url.characters(), url.length(), token.characters(), token.length());
        }
    }
#if !OS(QNX)
    // TODO: creating a WebDOMDocument for subframes here is a bit heavy since it's not always used; revisit later
    if (!m_loadingErrorPage) {
        JSDOMWindow* jsWindow = m_frame->script()->globalObject(WebCore::mainThreadNormalWorld());
        if (jsWindow) {
            JSContextRef contextRef = toRef(jsWindow->globalExec());
            if (m_webPage->settings()->isEmailMode())
                m_webPage->client()->notifyDocumentCreatedForFrameAsync(m_frame, isMainFrame(), WebDOMDocument(m_frame->document()), contextRef, toRef(jsWindow));
            else
                m_webPage->client()->notifyDocumentCreatedForFrame(m_frame, isMainFrame(), WebDOMDocument(m_frame->document()), contextRef, toRef(jsWindow));
        }
    }
#endif

    if (m_webPage->d->m_dumpRenderTree)
        m_webPage->d->m_dumpRenderTree->didCommitLoadForFrame(m_frame);
}

void FrameLoaderClientBlackBerry::dispatchDidHandleOnloadEvents()
{
    m_webPage->client()->notifyDocumentOnLoad();
}

void FrameLoaderClientBlackBerry::dispatchDidFinishLoad()
{
    didFinishOrFailLoading(ResourceError());

    if (!isMainFrame() || m_webPage->settings()->isEmailMode()
        || !m_frame->document() || !m_frame->document()->head())
        return;

    HTMLHeadElement* headElement = m_frame->document()->head();
    // FIXME: Handle NOSCRIPT special case?

    // Process document metadata
    RefPtr<NodeList> nodeList = headElement->getElementsByTagName("meta");
    unsigned int size = nodeList->length();
    ScopeArray<WebString> headers;

    // this may allocate more space than needed since not all meta elements will be http-equiv
    headers.reset(new WebString[2 * size]);
    unsigned int headersLength = 0;

    for (unsigned int i = 0; i < size; ++i) {
        HTMLMetaElement* metaElement = static_cast<HTMLMetaElement*>(nodeList->item(i));
        if (WTF::equalIgnoringCase(metaElement->name(), "apple-mobile-web-app-capable")
            && WTF::equalIgnoringCase(metaElement->content().stripWhiteSpace(), "yes"))
            m_webPage->client()->setWebAppCapable();
        else {
            WTF::String httpEquiv = metaElement->httpEquiv().stripWhiteSpace();
            WTF::String content = metaElement->content().stripWhiteSpace();

            if (!httpEquiv.isNull() && !content.isNull()) {
                headers[headersLength++] = httpEquiv;
                headers[headersLength++] = content;
            }
        }
    }

    if (headersLength > 0)
        m_webPage->client()->setMetaHeaders(headers, headersLength);

    nodeList = headElement->getElementsByTagName("link");
    size = nodeList->length();

    for (unsigned int i = 0; i < size; ++i) {
        HTMLLinkElement* linkElement = static_cast<HTMLLinkElement*>(nodeList->item(i));
        WTF::String href = linkElement->href().string();
        if (!href.isEmpty()) {
            WTF::String title = linkElement->title();

            if (WTF::equalIgnoringCase(linkElement->rel(), "apple-touch-icon"))
                m_webPage->client()->setLargeIcon(href.latin1().data());
            else if (WTF::equalIgnoringCase(linkElement->rel(), "search")) {
                if (WTF::equalIgnoringCase(linkElement->type(), "application/opensearchdescription+xml"))
                    m_webPage->client()->setSearchProviderDetails(title.utf8().data(), href.utf8().data());
            } else if (WTF::equalIgnoringCase(linkElement->rel(), "alternate")
                && (WTF::equalIgnoringCase(linkElement->type(), "application/rss+xml")
                || WTF::equalIgnoringCase(linkElement->type(), "application/atom+xml")))
                m_webPage->client()->setAlternateFeedDetails(title.utf8().data(), href.utf8().data());
        }
    }

    if (m_webPage->d->m_dumpRenderTree)
        m_webPage->d->m_dumpRenderTree->didFinishLoadForFrame(m_frame);
}

void FrameLoaderClientBlackBerry::dispatchDidFinishDocumentLoad()
{
    if (m_webPage->d->m_dumpRenderTree)
        m_webPage->d->m_dumpRenderTree->didFinishDocumentLoadForFrame(m_frame);
    notImplemented();
}


void FrameLoaderClientBlackBerry::dispatchDidFailLoad(const ResourceError& error)
{
    didFinishOrFailLoading(error);
    if (m_webPage->d->m_dumpRenderTree)
        m_webPage->d->m_dumpRenderTree->didFailLoadForFrame(m_frame);
}

void FrameLoaderClientBlackBerry::didFinishOrFailLoading(const ResourceError& error)
{
    m_frame->document()->setExtraLayoutDelay(0);

    // If we have finished loading a page through history navigation, any
    // attempt to go back to that page through an automatic redirect should be
    // denied to avoid redirect loops.  So save the history navigation urls to
    // check later.  (If this was not a history navigation,
    // m_historyNavigationSourceURLs will be empty, and we should save that
    // too.)
    m_redirectURLsToSkipDueToHistoryNavigation.swap(m_historyNavigationSourceURLs);

    // History navigation is finished so clear the history navigation url.
    m_historyNavigationSourceURLs.clear();

    if (isMainFrame()) {
        m_loadError = error;
        m_webPage->d->setLoadState(error.isNull() ? WebPagePrivate::Finished : WebPagePrivate::Failed);
    }
}

void FrameLoaderClientBlackBerry::dispatchDidFailProvisionalLoad(const ResourceError& error)
{
    if (isMainFrame()) {
        m_loadError = error;
        m_webPage->d->setLoadState(WebPagePrivate::Failed);

        if (error.domain() == ResourceError::platformErrorDomain
                && (error.errorCode() == Olympia::Platform::FilterStream::StatusErrorAlreadyHandled)) {
            // error has already been displayed by client
            return;
        }

        if (error.domain() == "" && error.errorCode() == 0 && error.failingURL() == "" && error.localizedDescription() == "") {
            // don't try to display empty errors returned from the unimplemented error functions in FrameLoaderClientBlackBerry - there's nothing to display anyway
            return;
        }
    }

    if (m_webPage->d->m_dumpRenderTree)
        m_webPage->d->m_dumpRenderTree->didFailProvisionalLoadForFrame(m_frame);

    if (!isMainFrame())
        return;

    WTF::String errorPage = m_webPage->d->m_client->getErrorPage(error.errorCode()
            , error.localizedDescription().isEmpty() ? "" : error.localizedDescription().utf8().data()
            , error.failingURL().isEmpty() ? "" : error.failingURL().utf8().data());

    // make sure we're still in the provisionalLoad state - getErrorPage runs a
    // nested event loop while it's waiting for client resources to load so
    // there's a small window for the user to hit stop
    if (m_frame->loader()->provisionalDocumentLoader()) {
        SubstituteData errorData(utf8Buffer(errorPage), "text/html", "utf-8", KURL(KURL(), error.failingURL()));

        ResourceRequest originalRequest = m_frame->loader()->provisionalDocumentLoader()->originalRequest();

        // Loading using SubstituteData will replace the original request with our
        // error data.  This must be done within dispatchDidFailProvisionalLoad,
        // and do NOT call stopAllLoaders first, because the loader checks the
        // provisionalDocumentLoader to decide the load type; if called any other
        // way, the error page is added to the end of the history instead of
        // replacing the failed load.
        // 
        // If this comes from a back/forward navigation, we need to save the current viewstate
        // to original historyitem, and prevent the restore of view state to the error page.

        if (WebCore::isBackForwardLoadType(m_frame->loader()->loadType())) {
            m_frame->loader()->history()->saveScrollPositionAndViewStateToItem(m_frame->loader()->history()->currentItem());
            ASSERT(m_frame->loader()->history()->provisionalItem());
            m_frame->loader()->history()->provisionalItem()->viewState().shouldSaveViewState = false;
        }
        m_loadingErrorPage = true;
        m_frame->loader()->load(originalRequest, errorData, false);
    }
}

void FrameLoaderClientBlackBerry::dispatchWillSubmitForm(FramePolicyFunction function, PassRefPtr<FormState>)
{
    // FIXME: stub
    (m_frame->loader()->policyChecker()->*function)(PolicyUse);
}

WTF::PassRefPtr<Frame> FrameLoaderClientBlackBerry::createFrame(const KURL& url, const WTF::String& name
    , HTMLFrameOwnerElement* ownerElement, const WTF::String& referrer, bool allowsScrolling, int marginWidth, int marginHeight)
{
    if (!m_webPage)
        return 0;

    FrameLoaderClientBlackBerry* frameLoaderClient = new FrameLoaderClientBlackBerry();
    RefPtr<Frame> childFrame = Frame::create(m_frame->page(), ownerElement, frameLoaderClient);
    frameLoaderClient->setFrame(childFrame.get(), m_webPage);

    // Initialize FrameView
    RefPtr<FrameView> frameView = FrameView::create(childFrame.get());
    childFrame->setView(frameView.get());
    if (!allowsScrolling)
        frameView->setScrollbarModes(ScrollbarAlwaysOff, ScrollbarAlwaysOff);
    if (marginWidth != -1)
        frameView->setMarginWidth(marginWidth);
    if (marginHeight != -1)
        frameView->setMarginHeight(marginHeight);

    childFrame->tree()->setName(name);
    m_frame->tree()->appendChild(childFrame);
    childFrame->init();

    if (!childFrame->tree()->parent())
        return 0;

    BackingStoreClient::create(childFrame.get(), m_frame, m_webPage);

    m_frame->loader()->loadURLIntoChildFrame(url, referrer, childFrame.get());

    if (!childFrame->tree()->parent())
        return 0;

    return childFrame.release();
}

void FrameLoaderClientBlackBerry::didTransferChildFrameToNewDocument(Page* /*oldPage*/)
{
    Page* newPage = m_frame->page();
    m_webPage = static_cast<ChromeClientBlackBerry*>(newPage->chrome()->client())->webPage();
}

ObjectContentType FrameLoaderClientBlackBerry::objectContentType(const KURL& url, const WTF::String& mimeTypeIn)
{
    WTF::String mimeType = mimeTypeIn;
    if (mimeType.isEmpty())
        mimeType = MIMETypeRegistry::getMIMETypeForExtension(url.path().substring(url.path().reverseFind('.') + 1));

    // Get mapped type.
    mimeType = Olympia::WebKit::WebSettings::getNormalizedMIMEType(mimeType);

    ObjectContentType defaultType = FrameLoader::defaultObjectContentType(url, mimeType);
    if (defaultType != ObjectContentNone)
        return defaultType;

    if (Olympia::WebKit::WebSettings::isSupportedObjectMIMEType(mimeType))
        return WebCore::ObjectContentOtherPlugin;
    
    return ObjectContentNone;
}

void FrameLoaderClientBlackBerry::dispatchWillClose()
{
    m_webPage->d->m_inputHandler->frameUnloaded(m_frame);
}

void FrameLoaderClientBlackBerry::setMainDocumentError(DocumentLoader*, const ResourceError& error)
{
    if (m_pluginView) {
        m_pluginView->didFail(error);
        m_pluginView = 0;
        m_hasSentResponseToPlugin = false;
    }
}

void FrameLoaderClientBlackBerry::postProgressStartedNotification()
{
    if (!isMainFrame())
        return;

    // new load started, so clear the error
    m_loadError = ResourceError();
    m_sentReadyToRender = false;
    m_webPage->client()->notifyLoadStarted();
}

void FrameLoaderClientBlackBerry::postProgressEstimateChangedNotification()
{
    if (!isMainFrame() || !m_frame->page())
        return;

    m_webPage->client()->notifyLoadProgress(m_frame->page()->progress()->estimatedProgress() * 100);
}

void FrameLoaderClientBlackBerry::dispatchDidFirstVisuallyNonEmptyLayout()
{
    if (!isMainFrame())
        return;

    Olympia::Platform::log(Olympia::Platform::LogLevelInfo, "dispatchDidFirstVisuallyNonEmptyLayout");

    readyToRender(true);

    // FIXME: We shouldn't be getting here if we are not in the Committed state but we are
    // so we can not assert on that right now. But we only want to do this on load.
    // RIM Bug #555
    if (m_webPage->d->loadState() == WebPagePrivate::Committed) {
        m_webPage->d->zoomToInitialScaleOnLoad(); // set the proper zoom level first
        m_webPage->backingStore()->d->clearVisibleZoom(); // Clear the visible zoom since we're explicitly rendering+blitting below
        m_webPage->backingStore()->d->renderVisibleContents();
    }

    m_webPage->client()->notifyFirstVisuallyNonEmptyLayout();
}

void FrameLoaderClientBlackBerry::postProgressFinishedNotification()
{
    if (!isMainFrame())
        return;

    // empty pages will never have called
    // dispatchDidFirstVisuallyNonEmptyLayout, since they're visually empty, so
    // we may need to call readyToRender now
    readyToRender(false);

    // FIXME: send up a real status code
    m_webPage->client()->notifyLoadFinished(m_loadError.isNull() ? 0 : -1);

    //Notify plugins that are waiting for the page to fully load before starting that
    //the load has completed.
    m_webPage->d->notifyPageOnLoad();
}

void FrameLoaderClientBlackBerry::dispatchDidClearWindowObjectInWorld(DOMWrapperWorld* world)
{
    // Provide the extension object first in case the client or others want to use it
    // FIXME: Conditionally attach extension object based on some flag or whether or not we
    // are browser or something else.
    attachExtensionObjectToFrame(m_frame, m_webPage->client());

    m_webPage->client()->notifyWindowObjectCleared();

    if (m_webPage->d->m_dumpRenderTree)
        m_webPage->d->m_dumpRenderTree->didClearWindowObjectInWorld(world);
}

bool FrameLoaderClientBlackBerry::shouldGoToHistoryItem(HistoryItem*) const
{
    // stub
    return true;
}

void FrameLoaderClientBlackBerry::invalidateBackForwardList() const
{
#if !OS(QNX)
    // since the list will change many times in a row,  batch up all
    // invalidations until next time through the event loop
    if (!m_invalidateBackForwardTimer->isActive())
        m_invalidateBackForwardTimer->startOneShot(0);
#else
    notifyBackForwardListChanged();
#endif
}

void FrameLoaderClientBlackBerry::invalidateBackForwardTimerFired(Timer<FrameLoaderClientBlackBerry>*)
{
    notifyBackForwardListChanged();
}

void FrameLoaderClientBlackBerry::notifyBackForwardListChanged() const
{
    BackForwardList* backForwardList = m_webPage->d->m_page->backForwardList();
    ASSERT(backForwardList);

    unsigned int listSize = backForwardList->entries().size();
    unsigned int currentIndex = backForwardList->backListCount();
    m_webPage->client()->resetBackForwardList(listSize, currentIndex);
}

Frame* FrameLoaderClientBlackBerry::dispatchCreatePage()
{
    Olympia::WebKit::WebPage* webPage = m_webPage->client()->createWindow(0, 0, -1, -1, Olympia::WebKit::WebPageClient::FlagWindowDefault, WebString(), WebString());
    if (!webPage)
        return 0;

    return webPage->d->m_page->mainFrame();
}

void FrameLoaderClientBlackBerry::detachedFromParent2()
{
    BackingStoreClient* backingStoreClientForFrame = m_webPage->d->backingStoreClientForFrame(m_frame);
    if (backingStoreClientForFrame) {
        delete backingStoreClientForFrame;
        backingStoreClientForFrame = 0;
    }

#if !OS(QNX)
    // Stop the timer if it's active. There's a chance
    // that the frame is held by a JS object but the page
    // has been destroyed. In any case, we don't want to
    // run the timer since this point.
    m_invalidateBackForwardTimer->stop();
#endif
    m_webPage->client()->notifyFrameDetached(m_frame);
    m_cachedPlugins.clear();
}

void FrameLoaderClientBlackBerry::dispatchWillSendRequest(DocumentLoader* docLoader, long unsigned int, ResourceRequest& request, const ResourceResponse&)
{
    // If the request is being loaded by the provisional document loader, then
    // it is a new top level request which has not been commited, so update the
    // request type.  (Workaround for <https://bugs.webkit.org/show_bug.cgi?id=48476>.)
    if (docLoader && docLoader == docLoader->frameLoader()->provisionalDocumentLoader()) {
        if (docLoader->frameLoader()->isLoadingMainFrame())
            request.setTargetType(ResourceRequest::TargetIsMainFrame);
        else
            request.setTargetType(ResourceRequest::TargetIsSubframe);
    }

    FrameLoader* loader = m_frame->loader();
    ASSERT(loader);
    if (WebCore::isBackForwardLoadType(loader->loadType())) {
        // Do not use the passed DocumentLoader because it is the loader that
        // will be used for the new request (the DESTINATION of the history
        // navigation - we want to use the current DocumentLoader to record the
        // SOURCE).
        DocumentLoader* docLoader = m_frame->loader()->documentLoader();
        ASSERT(docLoader);
        m_historyNavigationSourceURLs.add(docLoader->url());
        m_historyNavigationSourceURLs.add(docLoader->originalURL());
    }
}

void FrameLoaderClientBlackBerry::loadIconExternally(const WTF::String& originalPageUrl, const WTF::String& finalPageUrl, const WTF::String& iconUrl)
{
    m_webPage->client()->setIconForUrl(originalPageUrl.utf8().data(), finalPageUrl.utf8().data(), iconUrl.utf8().data());
}

void FrameLoaderClientBlackBerry::saveViewStateToItem(HistoryItem* item)
{
    if (!isMainFrame())
        return;

    ASSERT(item);
    HistoryItemViewState& viewState = item->viewState();
    if (viewState.shouldSaveViewState) {
        viewState.orientation = m_webPage->d->mainFrame()->orientation();
        viewState.scale = m_webPage->d->currentScale();
        viewState.shouldReflowBlock = m_webPage->d->m_shouldReflowBlock;
    }
}

void FrameLoaderClientBlackBerry::restoreViewState()
{
    if (!isMainFrame())
        return;

    HistoryItem* currentItem = m_frame->loader()->history()->currentItem();
    ASSERT(currentItem);

    if (!currentItem)
        return;

    // WebPagePrivate is messing up FrameView::wasScrolledByUser() by sending
    // scroll events that look like they were user generated all the time.
    //
    // Even if the user did not scroll, FrameView is gonna think they did.
    // So we use our own bookkeeping code to keep track of whether we were
    // actually scrolled by the user during load.
    //
    // If the user did scroll though, all are going to be in agreement about
    // that, and the worst thing that could happen is that
    // HistoryController::restoreScrollPositionAndViewState calls
    // setScrollPosition with the the same point, which is a NOOP.
    IntSize contentsSize = currentItem->contentsSize();
    IntPoint scrollPosition = currentItem->scrollPoint();
    if (m_webPage->d->m_userPerformedManualScroll)
        scrollPosition = m_webPage->d->scrollPosition();

    HistoryItemViewState& viewState = currentItem->viewState();

    // Also, try to keep the users zoom if any.
    double scale = viewState.scale;
    bool shouldReflowBlock = viewState.shouldReflowBlock;
    if (m_webPage->d->m_userPerformedManualZoom) {
        scale = m_webPage->d->currentScale();
        shouldReflowBlock = m_webPage->d->m_shouldReflowBlock;
    }

    bool scrollChanged = scrollPosition != m_webPage->d->scrollPosition();
    bool scaleChanged = scale != m_webPage->d->currentScale();
    bool reflowChanged = shouldReflowBlock != m_webPage->d->m_shouldReflowBlock;
    bool orientationChanged = viewState.orientation % 180 != m_webPage->d->mainFrame()->orientation() % 180;

    if (!scrollChanged && !scaleChanged && !reflowChanged && !orientationChanged)
        return;

    if (orientationChanged && !m_webPage->d->hasVirtualViewport())
        scale = Olympia::Platform::Graphics::Screen::width() * scale / static_cast<double>(Olympia::Platform::Graphics::Screen::height());

    m_webPage->backingStore()->d->suspendScreenAndBackingStoreUpdates(); // don't flash checkerboard for the setScrollPosition call
    m_frame->view()->setContentsSizeFromHistory(contentsSize);

    // Here we need to set scroll position what we asked for.
    // So we use ScrollView::setCanOverscroll(true).
    bool oldCanOverscroll = m_frame->view()->canOverScroll();
    m_frame->view()->setCanOverscroll(true);
    m_webPage->d->setScrollPosition(scrollPosition);
    m_frame->view()->setCanOverscroll(oldCanOverscroll);

    m_webPage->d->m_shouldReflowBlock = viewState.shouldReflowBlock;

    // will restore updates to backingstore guaranteed!
    if (!m_webPage->d->zoomAboutPoint(scale, m_frame->view()->scrollPosition(), false /* enforceScaleClamping */, true /*forceRendering*/, true /*isRestoringZoomLevel*/)) {
        // If we're already at that scale, then we should still force rendering since
        // our scroll position changed
        m_webPage->backingStore()->d->renderVisibleContents();

        // We need to notify the client of the scroll position and content size change(s) above even if we didn't scale
        m_webPage->d->notifyTransformedContentsSizeChanged();
        m_webPage->d->notifyTransformedScrollChanged();
    }
}

PolicyAction FrameLoaderClientBlackBerry::decidePolicyForExternalLoad(const ResourceRequest& request, bool isFragmentScroll)
{
    const KURL& url = request.url();
    WTF::String pattern = m_webPage->d->findPatternStringForUrl(url);
    if (!pattern.isEmpty()) {
        m_webPage->client()->handleStringPattern(pattern.characters(), pattern.length());
        return PolicyIgnore;
    }

    if (m_webPage->settings()->areLinksHandledExternally()
            && isMainFrame()
            && !request.mustHandleInternally()
            && !isFragmentScroll) {
        Olympia::Platform::NetworkRequest platformRequest;
        request.initializePlatformRequest(platformRequest);
        m_webPage->client()->handleExternalLink(platformRequest, request.anchorText().characters(), request.anchorText().length(), m_clientRedirectIsPending);
        return PolicyIgnore;
    }

    return PolicyUse;
}

void FrameLoaderClientBlackBerry::willDeferLoading()
{
    m_deferredJobsTimer->stop();

    if (!isMainFrame())
        return;

    m_webPage->d->willDeferLoading();
}

void FrameLoaderClientBlackBerry::didResumeLoading()
{
    if (!m_deferredManualScript.isNull())
        m_deferredJobsTimer->startOneShot(0);

    if (!isMainFrame())
        return;

    m_webPage->d->didResumeLoading();
}

void FrameLoaderClientBlackBerry::setDeferredManualScript(const KURL& script)
{
    ASSERT(!m_deferredJobsTimer->isActive());
    m_deferredManualScript = script;
}

void FrameLoaderClientBlackBerry::deferredJobsTimerFired(Timer<FrameLoaderClientBlackBerry>*)
{
    ASSERT(!m_frame->page()->defersLoading());

    if (!m_deferredManualScript.isNull()) {
        // executing the script will set deferred loading, which could trigger this timer again if a script is set.  So clear the script first.
        KURL script = m_deferredManualScript;
        m_deferredManualScript = KURL();

        m_frame->script()->executeIfJavaScriptURL(script);
    }

    ASSERT(!m_frame->page()->defersLoading());
}

void FrameLoaderClientBlackBerry::readyToRender(bool pageIsVisuallyNonEmpty)
{
    // only send the notification once
    if (!m_sentReadyToRender) {
        m_webPage->client()->notifyLoadReadyToRender(pageIsVisuallyNonEmpty);
        m_sentReadyToRender = true;
    }
}

PassRefPtr<FrameNetworkingContext> FrameLoaderClientBlackBerry::createNetworkingContext()
{
    return FrameNetworkingContextBlackBerry::create(m_frame);
}

void FrameLoaderClientBlackBerry::authenticationChallenge(const String& realm, String& username, String& password)
{
    WebString webPageUsername;
    WebString webPagePassword;

    m_webPage->client()->authenticationChallenge(realm.characters(), realm.length(), webPageUsername, webPagePassword);

    username = webPageUsername;
    password = webPagePassword;
}

void FrameLoaderClientBlackBerry::startDownload(const ResourceRequest& request)
{
    Olympia::Platform::NetworkRequest platformRequest;
    request.initializePlatformRequest(platformRequest, false /*isInitial*/);

    m_webPage->client()->downloadRequested(platformRequest);
}

void FrameLoaderClientBlackBerry::download(ResourceHandle* handle, const ResourceRequest&, const ResourceRequest&, const ResourceResponse& r)
{
    Olympia::Platform::FilterStream* stream = NetworkManager::instance()->streamForHandle(handle);
    ASSERT(stream);

    m_webPage->client()->downloadRequested(stream, r.suggestedFilename());
}

void FrameLoaderClientBlackBerry::dispatchDidReceiveIcon()
{
    WTF::String url = m_frame->loader()->url().string();
    Image* img = iconDatabase()->iconForPageURL(url, IntSize(10, 10));
    if (img && img->data()) {
        NativeImageSkia* bitmap= img->nativeImageForCurrentFrame();
        if (!bitmap)
            return;
        bitmap->lockPixels();
        m_webPage->client()->setFavicon(img->width(), img->height(), (unsigned char*)bitmap->getPixels());
        bitmap->unlockPixels();
    }
}

bool FrameLoaderClientBlackBerry::canCachePage() const
{
    // We won't cache pages containing video or audio
    ASSERT(m_frame->document());
    RefPtr<NodeList> nodeList = m_frame->document()->getElementsByTagName("video");
    if (nodeList.get()->length() > 0) 
        return false;
    nodeList = m_frame->document()->getElementsByTagName("audio");
    if (nodeList.get()->length() > 0) 
        return false;

    return true;
}

void FrameLoaderClientBlackBerry::provisionalLoadStarted()
{
    // We would like to hide the virtual keyboard before it navigates to another page
    // so that the scroll offset without keyboard shown will be saved in the history item.
    // and then when the user navigates back, it will scroll to the right position.

    m_webPage->setVirtualKeyboardVisible(false);
}

} // WebCore
