/*
 * Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 */

#include "config.h"
#include "ChromeClientBlackBerry.h"

#include "BackingStore.h"
#include "BackingStore_p.h"
#include "BackingStoreClient.h"
#include "CString.h"
#include "DatabaseTracker.h"
#include "PageGroupLoadDeferrer.h"

#include "Document.h"
#include "DumpRenderTreeClient.h"
#include "FileChooser.h"
#include "Frame.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "Geolocation.h"
#include "GeolocationControllerClientBlackBerry.h"
#include "HitTestResult.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "InputHandler.h"
#include "KURL.h"
#include "Node.h"
#include "NotImplemented.h"
#include "NotificationPresenterImpl.h"
#include "BlackBerryPlatformMisc.h"
#include "BlackBerryPlatformSettings.h"
#include "WebString.h"
#include "Page.h"
#include "PageGroup.h"
#include "PlatformString.h"
#include "PopupMenuBlackBerry.h"
#include "RenderView.h"
#include "SearchPopupMenuBlackBerry.h"
#include "SecurityOrigin.h"
#include "SharedPointer.h"
#include "SVGZoomAndPan.h"
#include "ViewportArguments.h"
#include "WebPage.h"
#include "WebPageClient.h"
#include "WebPage_p.h"
#include "WebSettings.h"
#include "WindowFeatures.h"

#define DEBUG_OVERFLOW_DETECTION 0

using Olympia::WebKit::WebString;
using namespace Olympia::WebKit;

namespace WebCore {

static CString frameOrigin(Frame* frame) {
    DOMWindow* window = frame->domWindow();
    SecurityOrigin* origin = window->securityOrigin();
    CString latinOrigin = origin->toString().latin1();
    return latinOrigin;
}

ChromeClientBlackBerry::ChromeClientBlackBerry(Olympia::WebKit::WebPage* page)
    : m_webPage(page)
{
}

void ChromeClientBlackBerry::addMessageToConsole(MessageSource, MessageType, MessageLevel, const WTF::String& message, unsigned int lineNumber, const WTF::String& sourceID)
{
    if (m_webPage->d->m_dumpRenderTree)
        m_webPage->d->m_dumpRenderTree->addMessageToConsole(message, lineNumber, sourceID);
    m_webPage->client()->addMessageToConsole(message.characters(), message.length(), sourceID.characters(), sourceID.length(), lineNumber);
}

void ChromeClientBlackBerry::runJavaScriptAlert(Frame* frame, const WTF::String& message)
{
    if (m_webPage->d->m_dumpRenderTree) {
        m_webPage->d->m_dumpRenderTree->runJavaScriptAlert(message);
        return;
    }
    TimerBase::fireTimersInNestedEventLoop();

    CString latinOrigin = frameOrigin(frame);
    m_webPage->client()->runJavaScriptAlert(message.characters(), message.length(), latinOrigin.data(), latinOrigin.length());
}

bool ChromeClientBlackBerry::runJavaScriptConfirm(Frame* frame, const WTF::String& message)
{
    if (m_webPage->d->m_dumpRenderTree)
        return m_webPage->d->m_dumpRenderTree->runJavaScriptConfirm(message);
    TimerBase::fireTimersInNestedEventLoop();

    CString latinOrigin = frameOrigin(frame);
    return m_webPage->client()->runJavaScriptConfirm(message.characters(), message.length(), latinOrigin.data(), latinOrigin.length());

}

bool ChromeClientBlackBerry::runJavaScriptPrompt(Frame* frame, const WTF::String& message, const WTF::String& defaultValue, WTF::String& result)
{
    WebString clientResult;

    if (m_webPage->d->m_dumpRenderTree) {
        result = m_webPage->d->m_dumpRenderTree->runJavaScriptPrompt(message, defaultValue);
        return true;
    }

    TimerBase::fireTimersInNestedEventLoop();

    CString latinOrigin = frameOrigin(frame);
    if (m_webPage->client()->runJavaScriptPrompt(message.characters(), message.length(), defaultValue.characters(), defaultValue.length(), latinOrigin.data(), latinOrigin.length(), clientResult)) {
        result = clientResult;
        return true;
    }
    return false;
}

void ChromeClientBlackBerry::chromeDestroyed()
{
    delete this;
}

void ChromeClientBlackBerry::setWindowRect(const FloatRect&)
{
    return; // The window dimensions are fixed in the RIM port.
}

FloatRect ChromeClientBlackBerry::windowRect()
{
    const IntSize windowSize = m_webPage->client()->applicationWindow()->windowSize();
    return FloatRect(0, 0, windowSize.width(), windowSize.height());;
}

FloatRect ChromeClientBlackBerry::pageRect()
{
    notImplemented();
    return FloatRect();
}

float ChromeClientBlackBerry::scaleFactor()
{
    return 1.0f;
}

void ChromeClientBlackBerry::focus()
{
    notImplemented();
}

void ChromeClientBlackBerry::unfocus()
{
    notImplemented();
}

bool ChromeClientBlackBerry::canTakeFocus(FocusDirection)
{
    notImplemented();
    return false;
}

void ChromeClientBlackBerry::takeFocus(FocusDirection)
{
    notImplemented();
}

void ChromeClientBlackBerry::focusedNodeChanged(Node* node)
{
    Olympia::WebKit::WebPageClient::FocusType type = Olympia::WebKit::WebPageClient::FocusUnknown;


    if (!node)
        type = Olympia::WebKit::WebPageClient::FocusNone;
    else if (node->isSVGElement()) {
        type = Olympia::WebKit::WebPageClient::FocusSVGElement;
    } else if (node->isHTMLElement()) {
        if (node->hasTagName(HTMLNames::aTag)) {
            type = Olympia::WebKit::WebPageClient::FocusLink;
        } else if (node->hasTagName(HTMLNames::inputTag)) {
            HTMLInputElement::DeprecatedInputType intype = reinterpret_cast<HTMLInputElement*>(node)->deprecatedInputType();
            switch (intype) {
            case HTMLInputElement::BUTTON:
                type = Olympia::WebKit::WebPageClient::FocusInputButton;
                break;
             case HTMLInputElement::CHECKBOX:
                type = Olympia::WebKit::WebPageClient::FocusInputCheckBox;
                break;
            case HTMLInputElement::COLOR:
                type = Olympia::WebKit::WebPageClient::FocusInputColor;
                break;
            case HTMLInputElement::DATE:
                type = Olympia::WebKit::WebPageClient::FocusInputDate;
                break;
            case HTMLInputElement::DATETIME:
                type = Olympia::WebKit::WebPageClient::FocusInputDateTime;
                break;
            case HTMLInputElement::DATETIMELOCAL:
                type = Olympia::WebKit::WebPageClient::FocusInputDateTimeLocal;
                break;
            case HTMLInputElement::EMAIL:
                type = Olympia::WebKit::WebPageClient::FocusInputEmail;
                break;
            case HTMLInputElement::FILE:
                type = Olympia::WebKit::WebPageClient::FocusInputFile;
                break;
            case HTMLInputElement::IMAGE:
                type = Olympia::WebKit::WebPageClient::FocusInputImage;
                break;
            case HTMLInputElement::MONTH:
                type = Olympia::WebKit::WebPageClient::FocusInputMonth;
                break;
            case HTMLInputElement::NUMBER:
                type = Olympia::WebKit::WebPageClient::FocusInputNumber;
                break;
            case HTMLInputElement::PASSWORD:
                type = Olympia::WebKit::WebPageClient::FocusInputPassword;
                break;
            case HTMLInputElement::RADIO:
                type = Olympia::WebKit::WebPageClient::FocusInputRadio;
                break;
            case HTMLInputElement::RANGE:
                type = Olympia::WebKit::WebPageClient::FocusInputRange;
                break;
            case HTMLInputElement::RESET:
                type = Olympia::WebKit::WebPageClient::FocusInputReset;
                break;
            case HTMLInputElement::SEARCH:
                type = Olympia::WebKit::WebPageClient::FocusInputSearch;
                break;
            case HTMLInputElement::SUBMIT:
                type = Olympia::WebKit::WebPageClient::FocusInputSubmit;
                break;
            case HTMLInputElement::TELEPHONE:
                type = Olympia::WebKit::WebPageClient::FocusInputTelephone;
                break;
            case HTMLInputElement::TEXT:
                type = Olympia::WebKit::WebPageClient::FocusInputText;
                break;
            case HTMLInputElement::TIME:
                type = Olympia::WebKit::WebPageClient::FocusInputTime;
                break;
            case HTMLInputElement::URL:
                type = Olympia::WebKit::WebPageClient::FocusInputURL;
                break;
            case HTMLInputElement::WEEK:
                type = Olympia::WebKit::WebPageClient::FocusInputWeek;
                break;
            case HTMLInputElement::HIDDEN:
                // This should never happen, HIDDEN should never be focused!
            case HTMLInputElement::ISINDEX:
            default:
                type = Olympia::WebKit::WebPageClient::FocusInputUnknown;
                break;
            }
        } else if (node->hasTagName(HTMLNames::imgTag))
            type = Olympia::WebKit::WebPageClient::FocusImage;
        else if (node->hasTagName(HTMLNames::objectTag))
            type = Olympia::WebKit::WebPageClient::FocusObject;
        else if (node->hasTagName(HTMLNames::videoTag))
            type = Olympia::WebKit::WebPageClient::FocusVideo;
        else if (node->hasTagName(HTMLNames::selectTag))
            type = Olympia::WebKit::WebPageClient::FocusSelect;
        else if (node->hasTagName(HTMLNames::textareaTag))
            type = Olympia::WebKit::WebPageClient::FocusTextArea;
        else if (node->hasTagName(HTMLNames::canvasTag))
            type = Olympia::WebKit::WebPageClient::FocusCanvas;
    }

    m_webPage->d->m_inputHandler->nodeFocused(node);

    m_webPage->client()->focusChanged(type, reinterpret_cast<int>(node));
}

bool ChromeClientBlackBerry::shouldForceDocumentStyleSelectorUpdate()
{
    return !m_webPage->settings()->isJavaScriptEnabled() && !m_webPage->d->m_inputHandler->processingChange();
}

Page* ChromeClientBlackBerry::createWindow(Frame*, const FrameLoadRequest& request, const WindowFeatures& features)
{
    PageGroupLoadDeferrer deferrer(m_webPage->d->m_page, true);
    TimerBase::fireTimersInNestedEventLoop();

    int x = features.xSet ? features.x : 0;
    int y = features.ySet ? features.y : 0;
    int width = features.widthSet? features.width : -1;
    int height = features.heightSet ? features.height : -1;
    unsigned flags = 0;

    if (features.menuBarVisible)
        flags |= Olympia::WebKit::WebPageClient::FlagWindowHasMenuBar;
    if (features.statusBarVisible)
        flags |= Olympia::WebKit::WebPageClient::FlagWindowHasStatusBar;
    if (features.toolBarVisible)
        flags |= Olympia::WebKit::WebPageClient::FlagWindowHasToolBar;
    if (features.locationBarVisible)
        flags |= Olympia::WebKit::WebPageClient::FlagWindowHasLocationBar;
    if (features.scrollbarsVisible)
        flags |= Olympia::WebKit::WebPageClient::FlagWindowHasScrollBar;
    if (features.resizable)
        flags |= Olympia::WebKit::WebPageClient::FlagWindowIsResizable;
    if (features.fullscreen)
        flags |= Olympia::WebKit::WebPageClient::FlagWindowIsFullScreen;
    if (features.dialog)
        flags |= Olympia::WebKit::WebPageClient::FlagWindowIsDialog;

    Olympia::WebKit::WebPage* webPage = m_webPage->client()->createWindow(x, y, width, height, flags, WebString(request.resourceRequest().url().string()), WebString(request.frameName()));
    if (!webPage)
        return 0;

    Page* newPage = webPage->d->m_page;
    if (!request.resourceRequest().isNull())
        newPage->mainFrame()->loader()->load(request.resourceRequest(), false);
    return newPage;
}

void ChromeClientBlackBerry::show()
{
    notImplemented();
}

bool ChromeClientBlackBerry::canRunModal()
{
    notImplemented();
    return false;
}

void ChromeClientBlackBerry::reachedApplicationCacheOriginQuota(SecurityOrigin*)
{
    notImplemented();
}

void ChromeClientBlackBerry::runModal()
{
    notImplemented();
}

bool ChromeClientBlackBerry::selectItemWritingDirectionIsNatural()
{
    return false;
}

PassRefPtr<PopupMenu> ChromeClientBlackBerry::createPopupMenu(PopupMenuClient* client) const
{
    return adoptRef(new PopupMenuBlackBerry(client));
}

PassRefPtr<SearchPopupMenu> ChromeClientBlackBerry::createSearchPopupMenu(PopupMenuClient* client) const
{
    return adoptRef(new SearchPopupMenuBlackBerry(client));
}


void ChromeClientBlackBerry::setToolbarsVisible(bool)
{
    notImplemented();
}

bool ChromeClientBlackBerry::toolbarsVisible()
{
    notImplemented();
    return false;
}

void ChromeClientBlackBerry::setStatusbarVisible(bool)
{
    notImplemented();
}

bool ChromeClientBlackBerry::statusbarVisible()
{
    notImplemented();
    return false;
}

void ChromeClientBlackBerry::setScrollbarsVisible(bool)
{
    notImplemented();
}

bool ChromeClientBlackBerry::scrollbarsVisible()
{
    notImplemented();
    return false;
}

void ChromeClientBlackBerry::setMenubarVisible(bool)
{
    notImplemented();
}

bool ChromeClientBlackBerry::menubarVisible()
{
    notImplemented();
    return false;
}

void ChromeClientBlackBerry::setResizable(bool)
{
    notImplemented();
}

bool ChromeClientBlackBerry::canRunBeforeUnloadConfirmPanel()
{
    notImplemented();
    return false;
}

bool ChromeClientBlackBerry::runBeforeUnloadConfirmPanel(const WTF::String& message, Frame*)
{
    if (m_webPage->d->m_dumpRenderTree)
        return m_webPage->d->m_dumpRenderTree->runBeforeUnloadConfirmPanel(message);

    notImplemented();
    return false;
}

void ChromeClientBlackBerry::closeWindowSoon()
{
    m_webPage->client()->scheduleCloseWindow();
}

void ChromeClientBlackBerry::setStatusbarText(const WTF::String& status)
{
#if OS(QNX)
    m_webPage->client()->setStatus(status);
#else
    notImplemented();
#endif
    if (m_webPage->d->m_dumpRenderTree)
        m_webPage->d->m_dumpRenderTree->setStatusText(status);
}

bool ChromeClientBlackBerry::tabsToLinks() const
{
    // FIXME: We should probably make it configurable
    return true;
}

IntRect ChromeClientBlackBerry::windowResizerRect() const
{
    notImplemented();
    return IntRect();
}

IntPoint ChromeClientBlackBerry::screenToWindow(const IntPoint& screenPos) const
{
    IntPoint windowPoint = m_webPage->client()->window()->windowLocation();
    windowPoint.move(-screenPos.x(), -screenPos.y());
    return windowPoint;
}

IntRect ChromeClientBlackBerry::windowToScreen(const IntRect& windowRect) const
{
    IntRect windowPoint(windowRect);
    IntPoint location(m_webPage->client()->window()->windowLocation());
    windowPoint.move(location.x(), location.y());
    return windowPoint;
}

void ChromeClientBlackBerry::mouseDidMoveOverElement(const HitTestResult& result, unsigned int modifierFlags)
{
    notImplemented();
}

void ChromeClientBlackBerry::setToolTip(const WTF::String& tooltip, TextDirection)
{
#if OS(QNX)
    m_webPage->client()->setToolTip(tooltip);
#else
    notImplemented();
#endif
}

#if ENABLE(EVENT_MODE_METATAGS)
void ChromeClientBlackBerry::didReceiveCursorEventMode(Frame* frame, CursorEventMode mode) const
{
    if (m_webPage->d->m_mainFrame != frame)
        return;

    m_webPage->d->didReceiveCursorEventMode(mode);
}

void ChromeClientBlackBerry::didReceiveTouchEventMode(Frame* frame, TouchEventMode mode) const
{
    if (m_webPage->d->m_mainFrame != frame)
        return;

    m_webPage->d->didReceiveTouchEventMode(mode);
}
#endif

void ChromeClientBlackBerry::dispatchViewportDataDidChange(const ViewportArguments& arguments) const
{
    m_webPage->d->dispatchViewportDataDidChange(arguments);
}

void ChromeClientBlackBerry::print(Frame*)
{
    notImplemented();
}

void ChromeClientBlackBerry::exceededDatabaseQuota(Frame* frame, const WTF::String& name)
{
#if ENABLE(DATABASE)
    Document* document = frame->document();
    if (!document)
        return;

    SecurityOrigin* origin = document->securityOrigin();
    if (m_webPage->d->m_dumpRenderTree) {
        m_webPage->d->m_dumpRenderTree->exceededDatabaseQuota(origin, name);
        return;
    }

    DatabaseTracker& tracker = DatabaseTracker::tracker(document->groupName());

    unsigned long long totalUsage = tracker.totalDatabaseUsage();
    unsigned long long originUsage = tracker.usageForOrigin(origin);

    DatabaseDetails details = tracker.detailsForNameAndOrigin(name, origin);
    unsigned long long estimatedSize = details.expectedUsage();
    const WTF::String& nameStr = details.displayName();

    WTF::String originStr = origin->databaseIdentifier();

    unsigned long long quota = m_webPage->client()->databaseQuota(originStr.characters(), originStr.length(),
        nameStr.characters(), nameStr.length(), totalUsage, originUsage, estimatedSize);

    tracker.setQuota(origin, quota);
#endif
}

void ChromeClientBlackBerry::requestGeolocationPermissionForFrame(Frame* frame, Geolocation* geolocation)
{
    if (!m_webPage->settings()->isGeolocationEnabled()) {
        geolocation->setIsAllowed(false);
        return;
    }
    DOMWindow* window = frame->domWindow();
    if (!window)
        return;

    CString latinOrigin = frameOrigin(frame);

    m_webPage->client()->requestGeolocationPermission(m_webPage->d->m_geolocationClient, geolocation, latinOrigin.data(), latinOrigin.length());
}

void ChromeClientBlackBerry::cancelGeolocationPermissionRequestForFrame(Frame*, Geolocation* geolocation)
{
    m_webPage->client()->cancelGeolocationPermission(m_webPage->d->m_geolocationClient, geolocation);
}

void ChromeClientBlackBerry::runOpenPanel(WebCore::Frame*, WTF::PassRefPtr<WebCore::FileChooser> chooser)
{
    SharedArray<WebString> initialFiles;
    unsigned int initialFileSize = chooser->filenames().size();
    if (initialFileSize > 0)
        initialFiles.reset(new WebString[initialFileSize]);
    for (unsigned i = 0; i < initialFileSize; ++i)
        initialFiles[i] = chooser->filenames()[i];

    SharedArray<WebString> chosenFiles;
    unsigned int chosenFileSize;

    {
        PageGroupLoadDeferrer deferrer(m_webPage->d->m_page, true);
        TimerBase::fireTimersInNestedEventLoop();

        if (!m_webPage->client()->chooseFilenames(chooser->allowsMultipleFiles(), chooser->acceptTypes(), initialFiles, initialFileSize, chosenFiles, chosenFileSize))
            return;
    }

    Vector<WTF::String> files(chosenFileSize);
    for (unsigned i = 0; i < chosenFileSize; ++i)
        files[i] = chosenFiles[i];
    chooser->chooseFiles(files);
}

void ChromeClientBlackBerry::setCursor(const Cursor&)
{
    notImplemented();
}

void ChromeClientBlackBerry::formStateDidChange(const Node* node)
{
    m_webPage->d->m_inputHandler->nodeTextChanged(node);
}

PassOwnPtr<HTMLParserQuirks> ChromeClientBlackBerry::createHTMLParserQuirks()
{
    notImplemented();
    return 0;
}

void ChromeClientBlackBerry::scrollbarsModeDidChange() const
{
    notImplemented();
}

void ChromeClientBlackBerry::contentsSizeChanged(WebCore::Frame* frame, const WebCore::IntSize& size) const
{
    if (frame != m_webPage->d->m_mainFrame)
        return;

    m_webPage->d->contentsSizeChanged(size);
}

void ChromeClientBlackBerry::invalidateWindow(const IntRect& updateRect, bool immediate)
{
    m_webPage->backingStore()->d->repaint(updateRect, false /*contentChanged*/, immediate);
}

void ChromeClientBlackBerry::invalidateContentsAndWindow(const IntRect& updateRect, bool immediate)
{
    m_webPage->backingStore()->d->repaint(updateRect, true /*contentChanged*/, immediate);
}

void ChromeClientBlackBerry::invalidateContentsForSlowScroll(const IntSize& delta, const IntRect& updateRect, bool immediate, const ScrollView* scrollView)
{
    if (scrollView != m_webPage->d->m_mainFrame->view())
        m_webPage->backingStore()->d->repaint(updateRect, true /*contentChanged*/, true /*immediate*/);
    else {
        BackingStoreClient* backingStoreClientForFrame = m_webPage->d->backingStoreClientForFrame(m_webPage->d->m_mainFrame);
        ASSERT(backingStoreClientForFrame);
        backingStoreClientForFrame->checkOriginOfCurrentScrollOperation();

        m_webPage->backingStore()->d->slowScroll(delta, updateRect, immediate);
    }
}

void ChromeClientBlackBerry::scroll(const IntSize& delta, const IntRect& scrollViewRect, const IntRect& clipRect)
{
    // FIXME: There's a chance the function is called indirectly by FrameView's dtor
    // when the Frame's view() is null. We probably want to fix it in another way, but
    // at this moment let's do a quick fix.
    if (!m_webPage->d->m_mainFrame->view())
        return;

    BackingStoreClient* backingStoreClientForFrame = m_webPage->d->backingStoreClientForFrame(m_webPage->d->m_mainFrame);
    ASSERT(backingStoreClientForFrame);
    backingStoreClientForFrame->checkOriginOfCurrentScrollOperation();

    m_webPage->backingStore()->d->scroll(delta, scrollViewRect, clipRect);
}

void ChromeClientBlackBerry::scrollRectIntoView(const IntRect&, const ScrollView*) const
{
    m_webPage->d->notifyTransformedScrollChanged();
}

bool ChromeClientBlackBerry::shouldInterruptJavaScript()
{
    TimerBase::fireTimersInNestedEventLoop();
    return m_webPage->client()->shouldInterruptJavaScript();
}

PlatformPageClient ChromeClientBlackBerry::platformPageClient() const
{
    return m_webPage->d;
}

void ChromeClientBlackBerry::reachedMaxAppCacheSize(int64_t spaceNeeded)
{
    notImplemented();
}

void ChromeClientBlackBerry::layoutFinished(WebCore::Frame* frame) const
{
    if (frame != m_webPage->d->m_mainFrame)
        return;

    m_webPage->d->layoutFinished();
}

void ChromeClientBlackBerry::overflowExceedsContentsSize(WebCore::Frame* frame) const
{
    if (frame != m_webPage->d->m_mainFrame)
        return;

#if DEBUG_OVERFLOW_DETECTION
    Olympia::Platform::log(Olympia::Platform::LogLevelInfo, "ChromeClientBlackBerry::overflowExceedsContentsSize contents=%dx%d overflow=%dx%d",
                           frame->contentRenderer()->rightLayoutOverflow(),
                           frame->contentRenderer()->bottomLayoutOverflow(),
                           frame->contentRenderer()->rightAbsoluteVisibleOverflow(),
                           frame->contentRenderer()->bottomAbsoluteVisibleOverflow());
#endif
    m_webPage->d->overflowExceedsContentsSize();
}

void ChromeClientBlackBerry::didDiscoverFrameSet(WebCore::Frame* frame) const
{
    if (frame != m_webPage->d->m_mainFrame)
        return;

    Olympia::Platform::log(Olympia::Platform::LogLevelInfo, "ChromeClientBlackBerry::didDiscoverFrameSet");
    if (m_webPage->d->loadState() == Olympia::WebKit::WebPagePrivate::Committed) {
        m_webPage->d->setShouldUseFixedDesktopMode(true);
        m_webPage->d->zoomToInitialScaleOnLoad();
    }
}

int ChromeClientBlackBerry::reflowWidth() const
{
    return m_webPage->d->reflowWidth();
}

void ChromeClientBlackBerry::chooseIconForFiles(const Vector<WTF::String>&, FileChooser*)
{
    notImplemented();
}

bool ChromeClientBlackBerry::supportsFullscreenForNode(const Node* node)
{
    return node->hasTagName(HTMLNames::videoTag);
}

void ChromeClientBlackBerry::enterFullscreenForNode(Node* node)
{
    if (!supportsFullscreenForNode(node))
        return;

    m_webPage->d->enterFullscreenForNode(node);
}

void ChromeClientBlackBerry::exitFullscreenForNode(Node* node)
{
    m_webPage->d->exitFullscreenForNode(node);
}

#if ENABLE(SVG)
void ChromeClientBlackBerry::didSetSVGZoomAndPan(Frame* frame, unsigned short zoomAndPan)
{
    // For top-level SVG document, there is no viewport tag, we use viewport's user-scalable
    // to enable/disable zoom when top-level SVG document's zoomAndPan changed. Because there is no viewport
    // tag, other fields with default value in ViewportArguments are ok.
    if (frame == m_webPage->d->m_page->mainFrame()) {
        ViewportArguments arguments;
        switch (zoomAndPan) {
        case SVGZoomAndPan::SVG_ZOOMANDPAN_DISABLE:
            arguments.userScalable = 0;
            break;
        case SVGZoomAndPan::SVG_ZOOMANDPAN_MAGNIFY:
            arguments.userScalable = 1;
            break;
        default:
            return;
        }
        didReceiveViewportArguments(frame, arguments);
    }
}
#endif

#if ENABLE(NOTIFICATIONS)
WebCore::NotificationPresenter* ChromeClientBlackBerry::notificationPresenter() const
{
    return WebKit::NotificationPresenterImpl::getInstance();
}
#endif

#if USE(ACCELERATED_COMPOSITING)
void ChromeClientBlackBerry::attachRootGraphicsLayer(Frame* frame, GraphicsLayer* graphicsLayer)
{
    // If the graphicsLayer parameter is 0, WebCore is actually
    // trying to remove a previously attached layer.
    m_webPage->d->setRootLayerProxy(frame, graphicsLayer ? graphicsLayer->platformLayer() : 0);
}

void ChromeClientBlackBerry::setNeedsOneShotDrawingSynchronization()
{
    m_webPage->d->setNeedsOneShotDrawingSynchronization();
}

void ChromeClientBlackBerry::scheduleCompositingLayerSync()
{
    m_webPage->d->scheduleRootLayerCommit();
}

bool ChromeClientBlackBerry::allowsAcceleratedCompositing() const
{
    return true;
}
#endif

void* ChromeClientBlackBerry::platformWindow() const
{
    return m_webPage->d->m_client->window();
}

void* ChromeClientBlackBerry::platformCompositingWindow() const
{
    return m_webPage->d->m_client->compositingWindow();
}

} // namespace WebCore
