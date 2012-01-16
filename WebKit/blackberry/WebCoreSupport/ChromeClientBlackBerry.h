/*
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 */

#ifndef ChromeClientOlympia_h
#define ChromeClientOlympia_h

#include "ChromeClient.h"

namespace Olympia {
    namespace WebKit {
        class WebPage;
        class WebPagePrivate;
    }
}

namespace WebCore {

class ChromeClientBlackBerry : public ChromeClient {
public:
    ChromeClientBlackBerry(Olympia::WebKit::WebPage* page);

    virtual void chromeDestroyed();
    virtual void setWindowRect(const FloatRect&);
    virtual FloatRect windowRect();
    virtual FloatRect pageRect();
    virtual float scaleFactor();
    virtual void focus();
    virtual void unfocus();
    virtual bool canTakeFocus(FocusDirection);
    virtual void takeFocus(FocusDirection);
    virtual void focusedNodeChanged(Node*);
    virtual bool shouldForceDocumentStyleSelectorUpdate();
    virtual Page* createWindow(Frame*, const FrameLoadRequest&, const WindowFeatures&);
    virtual void show();
    virtual bool canRunModal();
    virtual void runModal();
    virtual void setToolbarsVisible(bool);
    virtual bool toolbarsVisible();
    virtual void setStatusbarVisible(bool);
    virtual bool statusbarVisible();
    virtual void setScrollbarsVisible(bool);
    virtual bool scrollbarsVisible();
    virtual void setMenubarVisible(bool);
    virtual bool menubarVisible();
    virtual void setResizable(bool);
    virtual void addMessageToConsole(MessageSource, MessageType, MessageLevel, const String& message, unsigned int lineNumber, const String& sourceID);
    virtual bool canRunBeforeUnloadConfirmPanel();
    virtual bool runBeforeUnloadConfirmPanel(const String&, Frame*);
    virtual void closeWindowSoon();
    virtual void runJavaScriptAlert(Frame*, const String&);
    virtual bool runJavaScriptConfirm(Frame*, const String&);
    virtual bool runJavaScriptPrompt(Frame*, const String&, const String&, String&);
    virtual void setStatusbarText(const String&);
    virtual bool shouldInterruptJavaScript();
    virtual bool tabsToLinks() const;
    virtual IntRect windowResizerRect() const;
    virtual void invalidateWindow(const IntRect&, bool);
    virtual void invalidateContentsAndWindow(const IntRect&, bool);
    virtual void invalidateContentsForSlowScroll(const IntSize&, const IntRect&, bool, const ScrollView*);
    virtual void scroll(const IntSize&, const IntRect&, const IntRect&);
    virtual IntPoint screenToWindow(const IntPoint&) const;
    virtual IntRect windowToScreen(const IntRect&) const;
    virtual void contentsSizeChanged(Frame*, const IntSize&) const;
    virtual void scrollRectIntoView(const IntRect&, const ScrollView*) const;
    virtual void mouseDidMoveOverElement(const HitTestResult&, unsigned int);
    virtual void setToolTip(const String&, TextDirection);
#if ENABLE(EVENT_MODE_METATAGS)
    virtual void didReceiveCursorEventMode(Frame*, CursorEventMode) const;
    virtual void didReceiveTouchEventMode(Frame*, TouchEventMode) const;
#endif
    virtual void dispatchViewportDataDidChange(const ViewportArguments&) const;
    virtual void print(Frame*);
    virtual void exceededDatabaseQuota(Frame*, const String&);
    virtual void requestGeolocationPermissionForFrame(Frame*, Geolocation*);
    virtual void cancelGeolocationPermissionRequestForFrame(Frame*, Geolocation*);
    virtual void runOpenPanel(Frame*, WTF::PassRefPtr<FileChooser>);
    virtual void setCursor(const Cursor&);
    virtual void formStateDidChange(const Node*);
    virtual PassOwnPtr<HTMLParserQuirks> createHTMLParserQuirks();
    virtual void scrollbarsModeDidChange() const;
    virtual PlatformPageClient platformPageClient() const;

#if ENABLE(TOUCH_EVENTS)
    virtual void needTouchEvents(bool) { }
#endif
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    virtual void reachedApplicationCacheOriginQuota(WebCore::SecurityOrigin*);
#endif

    virtual void reachedMaxAppCacheSize(int64_t spaceNeeded);

    virtual void layoutFinished(Frame*) const;
    virtual void overflowExceedsContentsSize(Frame*) const;
    virtual void didDiscoverFrameSet(Frame*) const;

    virtual int reflowWidth() const;

    virtual void chooseIconForFiles(const Vector<String>&, FileChooser*);

    virtual bool supportsFullscreenForNode(const Node*);
    virtual void enterFullscreenForNode(Node*);
    virtual void exitFullscreenForNode(Node*);


#if ENABLE(NOTIFICATIONS)
    virtual WebCore::NotificationPresenter* notificationPresenter() const;
#endif

#if ENABLE(SVG)
    virtual void didSetSVGZoomAndPan(Frame*, unsigned short zoomAndPan);
#endif
    virtual bool selectItemWritingDirectionIsNatural();
    virtual PassRefPtr<WebCore::PopupMenu> createPopupMenu(WebCore::PopupMenuClient*) const;
    virtual PassRefPtr<WebCore::SearchPopupMenu> createSearchPopupMenu(WebCore::PopupMenuClient*) const;



#if USE(ACCELERATED_COMPOSITING)
    virtual void attachRootGraphicsLayer(Frame*, GraphicsLayer*);
    virtual void setNeedsOneShotDrawingSynchronization();
    virtual void scheduleCompositingLayerSync();
    virtual bool allowsAcceleratedCompositing() const;
#endif

    virtual void* platformWindow() const;
    virtual void* platformCompositingWindow() const;

    Olympia::WebKit::WebPage* webPage() const { return m_webPage; }

private:
    Olympia::WebKit::WebPage* m_webPage;
};

} // WebCore

#endif // ChromeClientOlympia_h
