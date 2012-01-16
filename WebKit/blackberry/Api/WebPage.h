/*
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 */

#ifndef WebPage_h
#define WebPage_h

#include "BlackBerryContext.h"
#include "BlackBerryGlobal.h"
#include "BlackBerryPlatformInputEvents.h"
#include "BlackBerryPlatformKeyboardEvent.h"
#include "BlackBerryPlatformPrimitives.h"
#if !defined(__QNXNTO__)
#include "OlympiaPlatformReplaceText.h"
#endif
#include "BlackBerryPlatformTouchEvents.h"
#include "WebString.h"
#include "SharedPointer.h"
#include "streams/NetworkRequest.h"

#include <stdint.h>

struct OpaqueJSContext;
typedef const struct OpaqueJSContext* JSContextRef;

struct OpaqueJSValue;
typedef const struct OpaqueJSValue* JSValueRef;

namespace WebCore {
    class ChromeClientBlackBerry;
    class EditorClientBlackBerry;
    class Element;
    class Frame;
    class FrameLoaderClientBlackBerry;
    class GeolocationControllerClientBlackBerry;
    class IconDatabaseClientBlackBerry;
    class InspectorClientBlackBerry;
    class JavaScriptDebuggerBlackBerry;
    class Node;
    class RenderObject;
}

#if !defined(__QNXNTO__)
class WebDOMDocument;
class WebDOMNode;
#endif

namespace Olympia {
    namespace WebKit {

        class BackingStore;
        class BackingStoreClient;
        class BackingStorePrivate;
        class DumpRenderTree; // FIXME: Temporary until DRT Uses new API and doesn't need friends
        class DumpRenderTreeClient;
        class RenderQueue;
        class ResourceHolder;
        class SwapGroup;
        class WebPageClient;
        class WebPageGroupLoadDeferrer;
        class WebPagePrivate;
        class WebPlugin;
        class WebSettings;

        enum MouseEventType { MouseEventMoved, MouseEventPressed, MouseEventReleased, MouseEventAborted };
        enum JavaScriptDataType { JSUndefined = 0, JSNull, JSBoolean, JSNumber, JSString, JSObject, JSException, JSDataTypeMax };

        enum ActivationStateType { ActivationActive, ActivationInactive, ActivationStandby };

        class OLYMPIA_EXPORT WebPage {
        public:
            WebPage(WebPageClient*, const WebString& pageGroupName, const Platform::IntRect&);
            void destroy();

            WebPageClient* client() const;

            void load(const char* url, const char* networkToken, bool isInitial = false);

            void loadExtended(const char* url, const char* networkToken, const char* method, Platform::NetworkRequest::CachePolicy cachePolicy = Platform::NetworkRequest::UseProtocolCachePolicy, const char* data = 0, size_t dataLength = 0, const char* const* headers = 0, size_t headersLength = 0, bool mustHandleInternally = false);

            void loadString(const char* string, const char* baseURL, const char* mimeType = "text/html");

            bool executeJavaScript(const char* script, JavaScriptDataType& returnType, WebString& returnValue);
            void initializeIconDataBase();

            void stopLoading();

            void reload();
            void reloadFromCache();

            WebSettings* settings() const;

            void setVisible(bool visible);
            bool isVisible() const;

            void setScreenOrientation(int orientation);

            Platform::IntSize viewportSize() const;
            void setViewportSize(const Platform::IntSize&);

            void resetVirtualViewportOnCommitted(bool reset);
            void setVirtualViewportSize(int width, int height);

            // Used for default layout size unless overridden by web content or by other APIs
            void setDefaultLayoutSize(int width, int height);

            void mouseEvent(MouseEventType, const Platform::IntPoint& pos, const Platform::IntPoint& globalPos);
            bool wheelEvent(int delta, const Platform::IntPoint& pos, const Platform::IntPoint& globalPos);

            // Handles native javascript touch events
            bool touchEvent(const Olympia::Platform::TouchEvent&);

            // For conversion to mouse events
            void touchEventCancel();
            void touchEventCancelAndClearFocusedNode();
            bool touchPointAsMouseEvent(const Olympia::Platform::TouchPoint&);

            // Returns true if the key stroke was handled by webkit.
            // TODO: Deprecate this function.
            bool keyEvent(Olympia::Platform::KeyboardEvent::Type type, const unsigned short character, bool shiftDown = false, bool altDown = false, bool ctrlDown = false);
            bool keyEvent(Olympia::Platform::KeyboardEvent::Type type, unsigned short character, unsigned modifiers = 0);

            void navigationMoveEvent(const unsigned short character, bool shiftDown, bool altDown);

            WebString title() const;
            WebString selectedText() const;
            WebString cutSelectedText();
            void insertText(const WebString&);
            void clearCurrentInputField();

            void cut();
            void copy();
            void paste();

            // Text encoding
            WebString textEncoding();
            WebString forcedTextEncoding();
            void setForcedTextEncoding(const char*);

            // scroll position returned is in transformed coordinates
            Platform::IntPoint scrollPosition() const;
            // scroll position provided should be in transformed coordinates
            void setScrollPosition(const Platform::IntPoint&);
            bool scrollBy(const Platform::IntSize&, bool scrollMainFrame = true);
            void notifySlowPathScrollingStatusChanged(bool status);
            void setScrollOriginPoint(const Olympia::Platform::IntPoint&);

            BackingStore* backingStore() const;

            bool zoomToFit();
            bool zoomToOneOne();
            void zoomToInitialScale();
            bool blockZoom(int x, int y);
            void blockZoomAnimationFinished();
            bool isAtInitialZoom() const;
            bool isMaxZoomed() const;
            bool isMinZoomed() const;
            bool pinchZoomAboutPoint(double scale, int x, int y);

            bool isUserScalable() const;
            double currentScale() const;
            double initialScale() const;
            void setInitialScale(double);
            double minimumScale() const;
            void setMinimumScale(double);
            double maximumScale() const;
            void setMaximumScale(double);

            void assignFocus(Olympia::Platform::FocusDirection direction);

            bool moveToNextField(Olympia::Platform::ScrollDirection, int desiredScrollAmount);
            Olympia::Platform::IntRect focusNodeRect();
            // FIXME: setFocused and focusField seem to be be doing similar stuff.
            //        They should be redesigned and possibly unified.
            void setFocused(bool focused);
            bool focusField(bool focus);
            bool linkToLinkOnClick();

            void clearBrowsingData();
            void clearHistory();
            void clearCookies();
            void clearCache();
            void clearLocalStorage();

            void runLayoutTests();
            /**
             * Finds and selects the next utf8 string that is a case sensitive
             * match in the web page. It will wrap the web page if it reaches
             * the end. An empty string will result in no match and no selection.
             *
             * Returns true if the string matched and false if not.
             */
            bool findNextString(const char*, bool forward = true);

            /**
             * Finds and selects the next unicode string. This is a case
             * sensitive search that will wrap if it reaches the end. An empty
             * string will result in no match and no selection.
             *
             * Returns true if the string matched and false if not.
             */
            bool findNextUnicodeString(const unsigned short*, bool forward = true);

            /* JavaScriptDebugger interface */
            bool enableScriptDebugger();
            bool disableScriptDebugger();

            JSContextRef scriptContext() const;
            JSValueRef windowObject() const;

            /* Media Plugin interface */
            void cancelLoadingPlugin(int id);
            void removePluginsFromList();
            void cleanPluginListFromFrame(WebCore::Frame* frame);
            void mediaReadyStateChanged(int id, int state);
            void mediaVolumeChanged(int id, int volume);
            void mediaDurationChanged(int id, float duration);

#if !defined(__QNXNTO__)
            WebDOMDocument document() const;
#endif

            void addBreakpoint(const unsigned short* url, unsigned urlLength, unsigned lineNumber, const unsigned short* condition, unsigned conditionLength);
            void updateBreakpoint(const unsigned short* url, unsigned urlLength, unsigned lineNumber, const unsigned short* condition, unsigned conditionLength);
            void removeBreakpoint(const unsigned short* url, unsigned urlLength, unsigned lineNumber);

            bool pauseOnExceptions();
            void setPauseOnExceptions(bool pause);

            void pauseInDebugger();
            void resumeDebugger();

            void stepOverStatementInDebugger();
            void stepIntoStatementInDebugger();
            void stepOutOfFunctionInDebugger();

            unsigned timeoutForJavaScriptExecution() const;
            void setTimeoutForJavaScriptExecution(unsigned ms);

            void requestElementText(int requestedFrameId, int requestedElementId, int offset, int length);
            void selectionChanged();
            void setCaretPosition(int requestedFrameId, int requestedElementId, int caretPosition);
            void setCaretHighlightStyle(Platform::CaretHighlightStyle);
            Olympia::Platform::IntRect rectForCaret(int index);
#if !defined(__QNXNTO__)
            Olympia::Platform::ReplaceTextErrorCode replaceText(const Olympia::Platform::ReplaceArguments&, const Olympia::Platform::AttributedText&);
#endif

            void setSelection(const Platform::IntPoint& startPoint, const Platform::IntPoint& endPoint);
            void selectAtPoint(const Platform::IntPoint&);
            void selectionCancelled();
            bool selectionContains(const Platform::IntPoint&);

            void popupListClosed(const int size, bool* selecteds);
            void popupListClosed(const int index);
            void setDateTimeInput(const WebString& value);
            void setColorInput(const WebString& value);

            void onInputLocaleChanged(bool isRTL);
            static void onNetworkAvailabilityChanged(bool available);
            static void onCertificateStoreLocationSet(const WebString& caPath);

            Olympia::WebKit::Context getContext() const;

            typedef intptr_t BackForwardId;
            struct BackForwardEntry {
                WebString url;
                WebString originalUrl;
                WebString title;
                WebString networkToken;
                BackForwardId id;
                bool lastVisitWasHTTPNonGet;
            };

            bool canGoBackOrForward(int delta) const;
            // Returns false if there is no page for the given delta (eg.
            // attempt to go back with -1 when on the first page)
            bool goBackOrForward(int delta);
            void goToBackForwardEntry(BackForwardId);

            int backForwardListLength() const;
            void getBackForwardList(SharedArray<BackForwardEntry>& result, unsigned int& resultLength) const;
            void releaseBackForwardEntry(BackForwardId) const;
            void clearBackForwardList(bool keepCurrentPage) const;

            ResourceHolder* getImageFromContext();

            void addVisitedLink(const unsigned short* url, unsigned int length);

#if !defined(__QNXNTO__)
            WebDOMNode nodeAtPoint(int x, int y);
            bool getNodeRect(const WebDOMNode&, Platform::IntRect& result);
            bool setNodeFocus(const WebDOMNode&, bool on);
            bool setNodeHovered(const WebDOMNode&, bool on);
            bool nodeHasHover(const WebDOMNode&);
#endif

            bool defersLoading() const;

            bool willFireTimer();

            void clearStorage();
            
            void enableWebInspector();
            void disableWebInspector();
            void dispatchInspectorMessage(const char* message, int length);

            /* FIXME: Needs API review on this header */
            void notifyPagePause();
            void notifyPageResume();
            void notifyPageBackground();
            void notifyPageForeground();
            void notifyPageFullScreenAllowed();
            void notifyPageFullScreenExit();
            void notifyDeviceIdleStateChange(bool enterIdle);
            void notifyAppActivationStateChange(ActivationStateType activationState);
            void notifySwipeEvent();
            void notifyScreenPowerStateChanged(bool powered);
            void notifyFullScreenVideoExited(bool done);
            void clearPluginSiteData();
            void setVirtualKeyboardVisible(bool visible);

        private:
            ~WebPage();

            friend class Olympia::WebKit::BackingStore;
            friend class Olympia::WebKit::BackingStoreClient;
            friend class Olympia::WebKit::BackingStorePrivate;
            friend class Olympia::WebKit::DumpRenderTree; // FIXME: Temporary until DRT Uses new API and doesn't need friends
            friend class Olympia::WebKit::RenderQueue;
            friend class Olympia::WebKit::WebPageGroupLoadDeferrer;
            friend class Olympia::WebKit::WebPlugin;
            friend class Olympia::WebKit::SwapGroup;
            friend class WebCore::ChromeClientBlackBerry;
            friend class WebCore::EditorClientBlackBerry;
            friend class WebCore::FrameLoaderClientBlackBerry;
            friend class WebCore::IconDatabaseClientBlackBerry;
            friend class WebCore::InspectorClientBlackBerry;
            friend class WebCore::JavaScriptDebuggerBlackBerry;
            friend class WebCore::GeolocationControllerClientBlackBerry;
            WebPagePrivate* d;
        };
    }
}

#endif // WebPage_h
