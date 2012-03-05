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
#include "FrameLoaderClientBlackBerry.h"

#include "BackForwardController.h"
#include "BackForwardListImpl.h"
#include "BackingStore.h"
#include "BackingStoreClient.h"
#include "BackingStore_p.h"
#include "Base64.h"
#include "BlackBerryPlatformLog.h"
#include "BlackBerryPlatformScreen.h"
#include "CString.h"
#include "CachedImage.h"
#include "Chrome.h"
#include "ChromeClientBlackBerry.h"
#include "ClientExtension.h"
#include "CookieManager.h"
#include "DumpRenderTreeClient.h"
#include "FrameLoader.h"
#include "FrameNetworkingContextBlackBerry.h"
#include "FrameView.h"
#include "HTMLHeadElement.h"
#include "HTMLImageElement.h"
#include "HTMLLinkElement.h"
#include "HTMLMediaElement.h"
#include "HTMLMetaElement.h"
#include "HTMLPlugInElement.h"
#include "HTTPParsers.h"
#include "HistoryItem.h"
#include "IconDatabase.h"
#include "Image.h"
#include "ImageSource.h"
#include "InputHandler.h"
#include "MIMETypeRegistry.h"
#include "NativeImageSkia.h"
#include "NetworkManager.h"
#include "NodeList.h"
#include "NotImplemented.h"
#include "Page.h"
#include "PluginView.h"
#include "ProgressTracker.h"
#include "RenderEmbeddedObject.h"
#include "ResourceError.h"
#include "ScopePointer.h"
#include "SecurityPolicy.h"
#include "SharedBuffer.h"
#include "TextEncoding.h"
#include "TouchEventHandler.h"
#if ENABLE(WEBDOM)
#include "WebDOMDocument.h"
#endif
#include "WebPage.h"
#include "WebPageClient.h"
#include "WebPage_p.h"
#include "WebSettings.h"

#include <BlackBerryPlatformNavigationType.h>
#include <JavaScriptCore/APICast.h>
#include <SkBitmap.h>
#include <network/FilterStream.h>
#include <network/NetworkRequest.h>


using WTF::String;
using namespace WebCore;
using namespace BlackBerry::WebKit;

// This was copied from file "WebKit/Source/WebKit/mac/Misc/WebKitErrors.h".
enum {
    WebKitErrorCannotShowMIMEType =                             100,
    WebKitErrorCannotShowURL =                                  101,
    WebKitErrorFrameLoadInterruptedByPolicyChange =             102,
    WebKitErrorCannotUseRestrictedPort =                        103,
    WebKitErrorCannotFindPlugIn =                               200,
    WebKitErrorCannotLoadPlugIn =                               201,
    WebKitErrorJavaUnavailable =                                202,
    WebKitErrorPluginWillHandleLoad =                           203
};

namespace WebCore {

FrameLoaderClientBlackBerry::FrameLoaderClientBlackBerry()
    : m_frame(0)
    , m_webPage(0)
    , m_sentReadyToRender(false)
    , m_pendingFragmentScrollPolicyFunction(0)
    , m_loadingErrorPage(false)
    , m_clientRedirectIsPending(false)
    , m_childFrameCreationSuppressed(false)
    , m_pluginView(0)
    , m_hasSentResponseToPlugin(false)
    , m_cancelLoadOnNextData(false)
{
    m_deferredJobsTimer = new Timer<FrameLoaderClientBlackBerry>(this, &FrameLoaderClientBlackBerry::deferredJobsTimerFired);
}

FrameLoaderClientBlackBerry::~FrameLoaderClientBlackBerry()
{
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

    WTF::String url = m_frame->document()->url().string();
    WTF::String token = m_frame->loader()->documentLoader()->request().token();

    m_webPage->client()->notifyLoadToAnchor(url.characters(), url.length(), token.characters(), token.length());
}

void FrameLoaderClientBlackBerry::dispatchDidPushStateWithinPage()
{
    // FIXME: As as workaround we abuse anchor navigation to achieve history push. See PR #119779 for more details.
    dispatchDidChangeLocationWithinPage();
}

void FrameLoaderClientBlackBerry::dispatchDidReplaceStateWithinPage()
{
    // FIXME: As as workaround we abuse anchor navigation to achieve history replace. See PR #119779 for more details.
    dispatchDidChangeLocationWithinPage();
}

void FrameLoaderClientBlackBerry::dispatchDidPopStateWithinPage()
{
    // not needed
}

void FrameLoaderClientBlackBerry::dispatchDidCancelClientRedirect()
{
    m_clientRedirectIsPending = false;
}

void FrameLoaderClientBlackBerry::dispatchWillPerformClientRedirect(const KURL&, double, double)
{
    if (m_webPage->d->m_dumpRenderTree)
        m_webPage->d->m_dumpRenderTree->didDispatchWillPerformClientRedirect();

    m_clientRedirectIsPending = true;
}

void FrameLoaderClientBlackBerry::dispatchDecidePolicyForResponse(FramePolicyFunction function, const WebCore::ResourceResponse& response, const WebCore::ResourceRequest& request)
{
    // FIXME: What should we do for HTTP status code 204 and 205 and "application/zip"?
    PolicyAction policy = PolicyIgnore;

    if (WebCore::contentDispositionType(response.httpHeaderField("Content-Disposition")) == WebCore::ContentDispositionAttachment
        || request.forceDownload())
        policy = PolicyDownload;
    else if (canShowMIMEType(response.mimeType()))
        policy = PolicyUse;
    else if ((ResourceRequest::TargetIsMainFrame == request.targetType())
             && m_webPage->client()->downloadAllowed(request.url().string().utf8().data()))
        policy = PolicyDownload;

    (m_frame->loader()->policyChecker()->*function)(policy);
}

void FrameLoaderClientBlackBerry::dispatchDecidePolicyForNavigationAction(FramePolicyFunction function, const NavigationAction& action, const ResourceRequest& request, PassRefPtr<FormState>)
{
    PolicyAction decision = PolicyUse;

    const KURL& url = request.url();

    // Fragment scrolls on the same page should always be handled internally.
    // (Only count as a fragment scroll if we are scrolling to a #fragment url, not back to the top, and reloading
    // the same url is not a fragment scroll even if it has a #fragment.)
    const KURL& currentUrl = m_frame->document()->url();
    bool isFragmentScroll = url.hasFragmentIdentifier() && url != currentUrl && equalIgnoringFragmentIdentifier(currentUrl, url);
    if (decision == PolicyUse)
        decision = decidePolicyForExternalLoad(request, isFragmentScroll);

    // Let the client have a chance to say whether this navigation should take
    // be ignored or not

    BlackBerry::Platform::NetworkRequest platformRequest;
    request.initializePlatformRequest(platformRequest, false /*isInitial*/);
    if (isMainFrame() && !m_webPage->client()->acceptNavigationRequest(
        platformRequest, BlackBerry::Platform::NavigationType(action.type()))) {
        if (action.type() == NavigationTypeFormSubmitted
            || action.type() == NavigationTypeFormResubmitted)
            m_frame->loader()->resetMultipleFormSubmissionProtection();

        if (action.type() == NavigationTypeLinkClicked && url.hasFragmentIdentifier()) {
            ResourceRequest emptyRequest;
            m_frame->loader()->activeDocumentLoader()->setLastCheckedRequest(emptyRequest);
        }
        decision = PolicyIgnore;
    }

    // If we abort here, dispatchDidCancelClientRedirect will not be called.
    // So call it by hand.
    if (decision == PolicyIgnore)
        dispatchDidCancelClientRedirect();

    (m_frame->loader()->policyChecker()->*function)(decision);

    if (m_webPage->d->m_dumpRenderTree)
        m_webPage->d->m_dumpRenderTree->didDecidePolicyForNavigationAction(action, request);
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
    if (request.isRequestedByPlugin() && ScriptController::processingUserGesture() && !m_webPage->d->m_pluginMayOpenNewTab)
        (m_frame->loader()->policyChecker()->*function)(PolicyIgnore);

    // a new window can never be a fragment scroll
    PolicyAction decision = decidePolicyForExternalLoad(request, false);
    (m_frame->loader()->policyChecker()->*function)(decision);
}

void FrameLoaderClientBlackBerry::committedLoad(DocumentLoader* loader, const char* data, int length)
{
    // The structure of this code may seem...a bit odd. It's structured with two checks on the state
    // of m_pluginView because it's actually receivedData that may cause the request to re-direct data
    // to a PluginView. This is because receivedData may decide to create a PluginDocument containing
    // a PluginView. The PluginView will request that the main resource for the frame be redirected
    // to the PluginView. So after receivedData has been called, this code needs to check whether
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
        mimeType = MIMETypeRegistry::getMIMETypeForPath(url.path());
        mimeType = BlackBerry::WebKit::WebSettings::getNormalizedMIMEType(mimeType);
        if (mimeType != "application/x-shockwave-flash")
            mimeType = mimeTypeIn;
    }

    if (mimeType == "application/x-shockwave-flash" || mimeType == "application/jnext-scriptable-plugin") {
        RefPtr<PluginView> pluginView = PluginView::create(m_frame, pluginSize, element, url, paramNames, paramValues, mimeType, loadManually);
        return pluginView;
    }
    // If it's not the plugin type we support, try load directly from browser.
    if (m_frame->loader() && m_frame->loader()->subframeLoader() && !url.isNull())
        m_frame->loader()->subframeLoader()->requestFrame(element, url, WTF::String());

    return 0;
}

void FrameLoaderClientBlackBerry::redirectDataToPlugin(Widget* pluginWidget)
{
    ASSERT(!m_pluginView);
    m_pluginView = static_cast<PluginView*>(pluginWidget);
    m_hasSentResponseToPlugin = false;
}

void FrameLoaderClientBlackBerry::receivedData(const char* data, int length, const WTF::String& textEncoding)
{
    if (!m_frame)
        return;

    if (m_cancelLoadOnNextData) {
        m_frame->loader()->activeDocumentLoader()->stopLoading();
        m_frame->loader()->documentLoader()->writer()->end();
        m_cancelLoadOnNextData = false;
        return;
    }

    // Set the encoding. This only needs to be done once, but it's harmless to do it again later.
    WTF::String encoding = m_frame->loader()->documentLoader()->overrideEncoding();
    bool userChosen = !encoding.isNull();
    if (encoding.isNull())
        encoding = textEncoding;
    m_frame->loader()->documentLoader()->writer()->setEncoding(encoding, userChosen);
    m_frame->loader()->documentLoader()->writer()->addData(data, length);
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
    m_cancelLoadOnNextData = false;

    // In Frame::createView, Frame's FrameView object is set to 0 and  recreated.
    // This operation is not atomic, and an attempt to blit contents might happen
    // in the backing store from another thread (see BackingStorePrivate::blitContents method),
    // so we suspend and resume screen update to make sure we do not get a invalid FrameView
    // state.
    BackingStoreClient* backingStoreClientForFrame = m_webPage->d->backingStoreClientForFrame(m_frame);
    if (backingStoreClientForFrame)
        backingStoreClientForFrame->backingStore()->d->suspendScreenAndBackingStoreUpdates();

    // We are navigating away from this document, so clean up any footprint we might have.
    if (m_frame->document())
        m_webPage->d->clearDocumentData(m_frame->document());

    Color backgroundColor(m_webPage->settings()->backgroundColor());

    m_frame->createView(m_webPage->d->viewportSize(),     /*viewport*/
                        backgroundColor,                  /*background color*/
                        backgroundColor.hasAlpha(),       /*is transparent*/
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

    if (isMainFrame()) {
        // Since the mainframe has a tiled backingstore request to receive all update
        // rects instead of the default which just sends update rects for currently
        // visible viewport
        m_frame->view()->setPaintsEntireContents(true);
    }
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
    WTF::String mimeType = BlackBerry::WebKit::WebSettings::getNormalizedMIMEType(mimeTypeIn);

    // FIXME: Seems no other port checks empty MIME type in this function. Should we do that?
    return MIMETypeRegistry::isSupportedImageMIMEType(mimeType) || MIMETypeRegistry::isSupportedNonImageMIMEType(mimeType)
        || MIMETypeRegistry::isSupportedMediaMIMEType(mimeType) || BlackBerry::WebKit::WebSettings::isSupportedObjectMIMEType(mimeType)
        || (mimeType == "application/x-shockwave-flash");
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

void FrameLoaderClientBlackBerry::dispatchDidReceiveResponse(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::ResourceResponse& response)
{
    if (m_webPage->d->m_dumpRenderTree)
        m_webPage->d->m_dumpRenderTree->didReceiveResponseForFrame(m_frame, response);
}

void FrameLoaderClientBlackBerry::dispatchDidReceiveTitle(const StringWithDirection& title)
{
    if (isMainFrame())
        m_webPage->client()->setPageTitle(title.string().characters(), title.string().length());

    if (m_webPage->d->m_dumpRenderTree)
        m_webPage->d->m_dumpRenderTree->didReceiveTitleForFrame(title.string(), m_frame);
}

void FrameLoaderClientBlackBerry::setTitle(const StringWithDirection& /*title*/, const KURL& /*url*/)
{
    // Used by Apple WebKit to update the title of an existing history item.
    // QtWebKit doesn't accomodate this on history items. If it ever does,
    // it should be privateBrowsing-aware. For now, we are just passing
    // globalhistory layout tests.
    // FIXME: Use direction of title.
    notImplemented();
}

void FrameLoaderClientBlackBerry::dispatchDidCommitLoad()
{
//    m_frame->document()->setExtraLayoutDelay(250);

    SecurityOrigin* securityOrigin = m_frame->document()->securityOrigin();
    BlackBerry::WebKit::WebSettings* webSettings = m_webPage->settings();

    // whitelist all known protocols if BrowserField2 requires unrestricted requests
    if (webSettings->allowCrossSiteRequests() && !securityOrigin->protocol().isEmpty()) {
        SecurityPolicy::addOriginAccessWhitelistEntry(*securityOrigin, String("about"), "", true);
        SecurityPolicy::addOriginAccessWhitelistEntry(*securityOrigin, String("data"), String(""), true);
        SecurityPolicy::addOriginAccessWhitelistEntry(*securityOrigin, String("file"), String(""), true);
        SecurityPolicy::addOriginAccessWhitelistEntry(*securityOrigin, String("http"), String(""), true);
        SecurityPolicy::addOriginAccessWhitelistEntry(*securityOrigin, String("https"), String(""), true);
        SecurityPolicy::addOriginAccessWhitelistEntry(*securityOrigin, String("javascript"), String(""), true);
        SecurityPolicy::addOriginAccessWhitelistEntry(*securityOrigin, String("local"), String(""), true);
        SecurityPolicy::addOriginAccessWhitelistEntry(*securityOrigin, String("mailto"), String(""), true);
        SecurityPolicy::addOriginAccessWhitelistEntry(*securityOrigin, String("pattern"), String(""), true);
        SecurityPolicy::addOriginAccessWhitelistEntry(*securityOrigin, String("rtcp"), String(""), true);
        SecurityPolicy::addOriginAccessWhitelistEntry(*securityOrigin, String("rtp"), String(""), true);
        SecurityPolicy::addOriginAccessWhitelistEntry(*securityOrigin, String("rtsp"), String(""), true);
        SecurityPolicy::addOriginAccessWhitelistEntry(*securityOrigin, String("sms"), String(""), true);
        SecurityPolicy::addOriginAccessWhitelistEntry(*securityOrigin, String("tel"), String(""), true);

        // We also want to be able to handle unknown protocols in the client.
        securityOrigin->grantUniversalAccess();
    }

    if (isMainFrame()) {
        m_webPage->d->setLoadState(WebPagePrivate::Committed);

        WTF::String originalUrl = m_frame->loader()->documentLoader()->originalRequest().url().string();
        WTF::String url = m_frame->loader()->documentLoader()->request().url().string();
        WTF::String token = m_frame->loader()->documentLoader()->request().token();

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

    if (m_webPage->d->m_dumpRenderTree)
        m_webPage->d->m_dumpRenderTree->didCommitLoadForFrame(m_frame);
}

void FrameLoaderClientBlackBerry::dispatchDidHandleOnloadEvents()
{
    m_webPage->client()->notifyDocumentOnLoad();
    if (m_webPage->d->m_dumpRenderTree)
        m_webPage->d->m_dumpRenderTree->didHandleOnloadEventsForFrame(m_frame);
}

void FrameLoaderClientBlackBerry::dispatchDidFinishLoad()
{
    didFinishOrFailLoading(ResourceError());

    if (m_webPage->d->m_dumpRenderTree)
        m_webPage->d->m_dumpRenderTree->didFinishLoadForFrame(m_frame);

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
//   FIXME
//    m_frame->document()->setExtraLayoutDelay(0);

    // If we have finished loading a page through history navigation, any
    // attempt to go back to that page through an automatic redirect should be
    // denied to avoid redirect loops. So save the history navigation urls to
    // check later. (If this was not a history navigation,
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
                && (error.errorCode() == BlackBerry::Platform::FilterStream::StatusErrorAlreadyHandled)) {
            // error has already been displayed by client
            return;
        }

        if (error.domain().isEmpty() && !error.errorCode() && error.failingURL().isEmpty() && error.localizedDescription().isEmpty()) {
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
        // error data. This must be done within dispatchDidFailProvisionalLoad,
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

    if (m_childFrameCreationSuppressed)
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

void FrameLoaderClientBlackBerry::transferLoadingResourceFromPage(ResourceLoader*, const ResourceRequest&, Page*)
{
    notImplemented();
}

ObjectContentType FrameLoaderClientBlackBerry::objectContentType(const KURL& url, const WTF::String& mimeTypeIn, bool shouldPreferPlugInsForImages)
{
    WTF::String mimeType = mimeTypeIn;
    if (mimeType.isEmpty())
        mimeType = MIMETypeRegistry::getMIMETypeForPath(url.path());

    // Get mapped type.
    mimeType = BlackBerry::WebKit::WebSettings::getNormalizedMIMEType(mimeType);

    ObjectContentType defaultType = FrameLoader::defaultObjectContentType(url, mimeType, shouldPreferPlugInsForImages);
    if (defaultType != ObjectContentNone)
        return defaultType;

    if (BlackBerry::WebKit::WebSettings::isSupportedObjectMIMEType(mimeType))
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

    BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "dispatchDidFirstVisuallyNonEmptyLayout");

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

    // Notify plugins that are waiting for the page to fully load before starting that
    // the load has completed.
    m_webPage->d->notifyPageOnLoad();
}

void FrameLoaderClientBlackBerry::dispatchDidClearWindowObjectInWorld(DOMWrapperWorld* world)
{
    if (world != mainThreadNormalWorld())
        return;

    // Provide the extension object first in case the client or others want to use it
    // FIXME: Conditionally attach extension object based on some flag or whether or not we
    // are browser or something else.
    attachExtensionObjectToFrame(m_frame, m_webPage->client());

    m_webPage->client()->notifyWindowObjectCleared();

    if (m_webPage->d->m_dumpRenderTree) {
        JSGlobalContextRef context = toGlobalRef(m_frame->script()->globalObject(mainThreadNormalWorld())->globalExec());
        JSObjectRef windowObject = toRef(m_frame->script()->globalObject(mainThreadNormalWorld()));
        ASSERT(windowObject);
        m_webPage->d->m_dumpRenderTree->didClearWindowObjectInWorld(world, context, windowObject);
    }
}

bool FrameLoaderClientBlackBerry::shouldGoToHistoryItem(HistoryItem*) const
{
    return true;
}

bool FrameLoaderClientBlackBerry::shouldStopLoadingForHistoryItem(HistoryItem*) const
{
    return true;
}

void FrameLoaderClientBlackBerry::invalidateBackForwardList() const
{
    notifyBackForwardListChanged();
}

void FrameLoaderClientBlackBerry::notifyBackForwardListChanged() const
{
    BackForwardListImpl* backForwardList = static_cast<WebCore::BackForwardListImpl*>(m_webPage->d->m_page->backForward()->client());
    ASSERT(backForwardList);

    unsigned int listSize = backForwardList->entries().size();
    unsigned int currentIndex = backForwardList->backListCount();
    m_webPage->client()->resetBackForwardList(listSize, currentIndex);
}

Frame* FrameLoaderClientBlackBerry::dispatchCreatePage(const NavigationAction& navigation)
{
    BlackBerry::WebKit::WebPage* webPage = m_webPage->client()->createWindow(0, 0, -1, -1, BlackBerry::WebKit::WebPageClient::FlagWindowDefault, navigation.url().string(), WebString());
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

    if (m_frame->document())
        m_webPage->d->clearDocumentData(m_frame->document());

    m_webPage->d->m_inputHandler->frameUnloaded(m_frame);
    m_webPage->client()->notifyFrameDetached(m_frame);
}

void FrameLoaderClientBlackBerry::dispatchWillSendRequest(DocumentLoader* docLoader, long unsigned int, ResourceRequest& request, const ResourceResponse&)
{
    // If the request is being loaded by the provisional document loader, then
    // it is a new top level request which has not been commited.
    bool isMainResourceLoad = docLoader && docLoader == docLoader->frameLoader()->provisionalDocumentLoader();

    // Any processing which is done for all loads (both main and subresource) should go here.
    BlackBerry::Platform::NetworkRequest platformRequest;
    request.initializePlatformRequest(platformRequest, false /*isInitial*/);
    m_webPage->client()->populateCustomHeaders(platformRequest);
    const BlackBerry::Platform::NetworkRequest::HeaderList& headerLists = platformRequest.getHeaderListRef();
    for (BlackBerry::Platform::NetworkRequest::HeaderList::const_iterator it = headerLists.begin(); it != headerLists.end(); ++it) {
        std::string headerString = it->first;
        std::string headerValueString = it->second;
        request.setHTTPHeaderField(WTF::String(headerString.c_str()), WTF::String(headerValueString.c_str()));
    }
    if (cookiesEnabled()) {
        String cookiePairs = cookieManager().getCookie(request.url(), WithHttpOnlyCookies);
        if (!cookiePairs.isEmpty()) {
            // We only modify the WebCore request to make the cookies visible in inspector
            request.setHTTPHeaderField(WTF::String("Cookie"), cookiePairs);
        }
    }
    if (!isMainResourceLoad) {
        // Do nothing for now.
        // Any processing which is done only for subresources should go here.
        return;
    }

    // All processing beyond this point is done only for main resource loads.

    if (m_clientRedirectIsPending && isMainFrame()) {
        WTF::String originalUrl = m_frame->document()->url().string();
        WTF::String finalUrl = request.url().string();

        m_webPage->client()->notifyClientRedirect(originalUrl.characters(), originalUrl.length(),
            finalUrl.characters(), finalUrl.length());
    }

    // FIXME: Update the request type. See PR #119792.
    if (docLoader->frameLoader()->isLoadingMainFrame())
        request.setTargetType(ResourceRequest::TargetIsMainFrame);
    else
        request.setTargetType(ResourceRequest::TargetIsSubframe);

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
        viewState.isZoomToFitScale= m_webPage->d->currentScale() == m_webPage->zoomToFitScale();
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

    // We need to reset this variable after the view state has been restored.
    m_webPage->d->m_didRestoreFromPageCache = false;
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

    // when rotate happens, only zoom when previous page was zoomToFitScale, otherwise keep old scale
    if (orientationChanged && viewState.isZoomToFitScale)
        scale = BlackBerry::Platform::Graphics::Screen::width() * scale / static_cast<double>(BlackBerry::Platform::Graphics::Screen::height());
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
    if (!m_webPage->d->zoomAboutPoint(scale, m_frame->view()->scrollPosition(), true /* enforceScaleClamping */, true /*forceRendering*/, true /*isRestoringZoomLevel*/)) {
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
        BlackBerry::Platform::NetworkRequest platformRequest;
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
        // Executing the script will set deferred loading, which could trigger this timer again if a script is set. So clear the script first.
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

void FrameLoaderClientBlackBerry::startDownload(const ResourceRequest& request, const String& /*suggestedName*/)
{
    // FIXME: use the suggestedName?
    BlackBerry::Platform::NetworkRequest platformRequest;
    request.initializePlatformRequest(platformRequest, false /*isInitial*/);

    m_webPage->download(platformRequest);
}

void FrameLoaderClientBlackBerry::download(ResourceHandle* handle, const ResourceRequest&, const ResourceRequest&, const ResourceResponse& r)
{
    BlackBerry::Platform::FilterStream* stream = NetworkManager::instance()->streamForHandle(handle);
    ASSERT(stream);

    m_webPage->client()->downloadRequested(stream, r.suggestedFilename());
}

void FrameLoaderClientBlackBerry::dispatchDidReceiveIcon()
{
    WTF::String url = m_frame->document()->url().string();
    Image* img = WebCore::iconDatabase().synchronousIconForPageURL(url, IntSize(10, 10));
    WTF::String iconUrl = WebCore::iconDatabase().synchronousIconURLForPageURL(url);
    if (img && img->data()) {
        NativeImageSkia* bitmap= img->nativeImageForCurrentFrame();
        if (!bitmap)
            return;
        bitmap->lockPixels();
        m_webPage->client()->setFavicon(img->width(), img->height(), (unsigned char*)bitmap->getPixels(), iconUrl.utf8().data());
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

    // The multipart of "multipart/x-mixed-replace" only supports image, correct?
    // we may have a better place to handle this case?
    nodeList = m_frame->document()->getElementsByTagName("img");
    for (unsigned i = 0; i < nodeList.get()->length(); ++i) {
        HTMLImageElement* node = static_cast<HTMLImageElement*>(nodeList.get()->item(i));
        CachedImage* cachedimage = node ? node->cachedImage() : 0;
        if (cachedimage && cachedimage->response().isMultipartPayload())
            return false;
    }
    return true;
}

void FrameLoaderClientBlackBerry::provisionalLoadStarted()
{
    // We would like to hide the virtual keyboard before it navigates to another page
    // so that the scroll offset without keyboard shown will be saved in the history item.
    // and then when the user navigates back, it will scroll to the right position.
    if (isMainFrame())
        m_webPage->setVirtualKeyboardVisible(false);
}

// We don't need to provide the error message string, that will be handled in BrowserErrorPage according to the error code.
ResourceError FrameLoaderClientBlackBerry::cannotShowURLError(const ResourceRequest& request)
{
    // FIXME: Why are we not passing the domain to the ResourceError? See PR #119789.
    return ResourceError(String(), WebKitErrorCannotShowURL, request.url().string(), String());
}

void FrameLoaderClientBlackBerry::didRestoreFromPageCache()
{
    m_webPage->d->m_didRestoreFromPageCache = true;
}

void FrameLoaderClientBlackBerry::dispatchWillUpdateApplicationCache(const ResourceRequest&)
{
    ASSERT(isMainFrame());
    if (!isMainFrame())
        return;

    m_webPage->client()->notifyWillUpdateApplicationCache();
}

void FrameLoaderClientBlackBerry::dispatchDidLoadFromApplicationCache(const ResourceRequest&)
{
    ASSERT(isMainFrame());
    if (!isMainFrame())
        return;

    m_webPage->client()->notifyDidLoadFromApplicationCache();
}

} // WebCore
