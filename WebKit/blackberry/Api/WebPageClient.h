/*
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 */

#ifndef WebPageClient_h
#define WebPageClient_h

#include "BlackBerryGlobal.h"

#include "BlackBerryPlatformGeoTrackerListener.h"
#include "BlackBerryPlatformGraphics.h"
#include "BlackBerryPlatformInputEvents.h"
#include "BlackBerryPlatformIntRectRegion.h"
#include "BlackBerryPlatformMisc.h"
#include <BlackBerryPlatformNavigationType.h>
#include "BlackBerryPlatformPrimitives.h"
#include <BlackBerryPlatformWindow.h>
#include "WebString.h"
#if !defined(__QNXNTO__)
#include "WebDOMDocument.h"

#include "JavaScriptCore/JSValueRef.h"
#endif

#include <pthread.h>

template<typename T> class ScopeArray;
template<typename T> class SharedArray;

typedef void* WebFrame;

namespace WTF {
class String;
}

namespace Olympia {

    namespace Platform {
        class FilterStream;
        class GeoTracker;
        class IntPoint;
        class IntRect;
        class NetworkRequest;
        class NetworkStreamFactory;
        class SocketStreamFactory;
    }

    namespace WebKit {
        class WebPage;

        class OLYMPIA_EXPORT WebPageClient {
        public:
            virtual ~WebPageClient() {}
            enum WindowStyleFlag {
                FlagWindowHasMenuBar = 0x00000001,
                FlagWindowHasToolBar = 0x00000002,
                FlagWindowHasLocationBar = 0x00000004,
                FlagWindowHasStatusBar = 0x00000008,
                FlagWindowHasScrollBar = 0x00000010,
                FlagWindowIsResizable = 0x00000020,
                FlagWindowIsFullScreen = 0x00000040,
                FlagWindowIsDialog = 0x00000080,
                FlagWindowDefault = 0xFFFFFFFF,
            };

            enum FocusType {
                FocusUnknown = 0,
                FocusNone,
                FocusCanvas,
                FocusImage,
                FocusInputButton,
                FocusInputCheckBox,
                FocusInputColor,
                FocusInputDate,
                FocusInputDateTime,
                FocusInputDateTimeLocal,
                FocusInputEmail,
                FocusInputFile,
                FocusInputImage,
                FocusInputMonth,
                FocusInputNumber,
                FocusInputPassword,
                FocusInputRadio,
                FocusInputRange,
                FocusInputReset,
                FocusInputSearch,
                FocusInputSubmit,
                FocusInputTelephone,
                FocusInputText,
                FocusInputTime,
                FocusInputURL,
                FocusInputWeek,
                FocusInputUnknown,
                FocusLink,
                FocusObject,
                FocusSelect,
                FocusSVGElement,
                FocusTextArea,
                FocusVideo,
            };

            enum AlertType {
                MediaOK = 0,
                MediaDecodeError,
                MediaMetaDataError,
                MediaMetaDataTimeoutError,
                MediaNoMetaDataError,
                MediaVideoReceiveError,
                MediaAudioReceiveError,
                MediaInvalidError,
            };

            virtual int getInstanceId() const = 0;

            virtual void notifyLoadStarted() = 0;
            virtual void notifyLoadCommitted(const unsigned short* originalUrl, unsigned int originalUrlLength, const unsigned short* finalUrl, unsigned int finalUrlLength, const unsigned short* networkToken, unsigned int networkTokenLength) = 0;
            virtual void notifyLoadFailedBeforeCommit(const unsigned short* originalUrl, unsigned int originalUrlLength, const unsigned short* finalUrl, unsigned int finalUrlLength, const unsigned short* networkToken, unsigned int networkTokenLength) = 0;
            virtual void notifyLoadToAnchor(const unsigned short* url, unsigned int urlLength, const unsigned short* networkToken, unsigned int networkTokenLength) = 0;
            virtual void notifyLoadProgress(int percentage) = 0;
            virtual void notifyLoadReadyToRender(bool pageIsVisuallyNonEmpty) = 0;
            virtual void notifyFirstVisuallyNonEmptyLayout() = 0;
            virtual void notifyLoadFinished(int status) = 0;
            virtual void notifyClientRedirect(const unsigned short* originalUrl, unsigned int originalUrlLength, const unsigned short* finalUrl, unsigned int finalUrlLength) = 0;

#if !defined(__QNXNTO__)
            virtual void notifyDocumentCreatedForFrame(const WebFrame frame, const bool isMainFrame, const WebDOMDocument& document, const JSContextRef context, const JSValueRef window) = 0;
            virtual void notifyDocumentCreatedForFrameAsync(const WebFrame frame, const bool isMainFrame, const WebDOMDocument& document, const JSContextRef context, const JSValueRef window) = 0;
#endif
            virtual void notifyFrameDetached(const WebFrame frame) = 0;

            virtual void notifyRunLayoutTestsFinished() = 0;

            virtual void notifySlowPathScrollingRequired(bool) = 0;

            // document.onload
            virtual void notifyDocumentOnLoad() = 0;

            virtual void notifyWindowObjectCleared() = 0;
            virtual WebString invokeClientJavaScriptCallback(const char* const* args, unsigned int numArgs) = 0;

            virtual void addMessageToConsole(const unsigned short* message, unsigned messageLength, const unsigned short* source, unsigned sourceLength, unsigned lineNumber) = 0;
            virtual void showAlertDialog(AlertType atype) = 0;

            virtual void runJavaScriptAlert(const unsigned short* message, unsigned messageLength, const char* origin, unsigned originLength) = 0;
            virtual bool runJavaScriptConfirm(const unsigned short* message, unsigned messageLength, const char* origin, unsigned originLength) = 0;
            virtual bool runJavaScriptPrompt(const unsigned short* message, unsigned messageLength, const unsigned short* defaultValue, unsigned defaultValueLength, const char* origin, unsigned originLength, WebString& result) = 0;

            virtual bool shouldInterruptJavaScript() = 0;

            virtual void javascriptSourceParsed(const unsigned short* url, unsigned urlLength, const unsigned short* script, unsigned scriptLength) = 0;
            virtual void javascriptParsingFailed(const unsigned short* url, unsigned urlLength, const unsigned short* error, unsigned errorLength, int lineNumber) = 0;
            virtual void javascriptPaused(const unsigned short* stack, unsigned stackLength) = 0;
            virtual void javascriptContinued() = 0;

            // All of these methods use transformed coordinates
            virtual void contentsSizeChanged(const Olympia::Platform::IntSize&) const = 0;
            virtual void scrollChanged(const Olympia::Platform::IntPoint&) const = 0;
            virtual void zoomChanged(const bool isMinZoomed, const bool isMaxZoomed, const bool isAtInitialZoom, const double newZoom) const = 0;

            virtual void setPageTitle(const unsigned short* title, unsigned titleLength) = 0;

            virtual Olympia::Platform::Graphics::Window* window() const = 0;
            virtual Olympia::Platform::Graphics::Window* compositingWindow() const = 0;
            virtual Olympia::Platform::Graphics::Window* applicationWindow() const = 0;

            virtual void notifyContentRendered(const Olympia::Platform::IntRect&) = 0;
            virtual void syncToUserInterfaceThread() = 0;
            virtual void notifyScreenRotated() = 0;

            virtual void focusChanged(FocusType type, int elementId) = 0;
            virtual void inputFocusGained(unsigned int frameId, unsigned int inputFieldId, Olympia::Platform::OlympiaInputType type, unsigned int characterCount, unsigned int selectionStart, unsigned int selectionEnd) = 0;
            virtual void inputFocusLost(unsigned int frameId, unsigned int inputFieldId) = 0;
            virtual void inputTextChanged(unsigned int frameId, unsigned int inputFieldId, const unsigned short* text, unsigned int textLength, unsigned int selectionStart, unsigned int selectionEnd) = 0;
            virtual void inputTextForElement(unsigned int requestedFrameId, unsigned int requestedElementId, unsigned int offset, int length, int selectionStart, int selectionEnd, const unsigned short* text, unsigned int textLength) = 0;
            virtual void inputFrameCleared(unsigned int frameId) = 0;
            virtual void inputSelectionChanged(unsigned int frameId, unsigned int inputFieldId, unsigned int selectionStart, unsigned int selectionEnd) = 0;
            virtual void inputSetNavigationMode(bool) = 0;

            virtual void checkSpellingOfString(const unsigned short* text, int length, int& misspellingLocation, int& misspellingLength) = 0;

            virtual void notifySelectionDetailsChanged(const Olympia::Platform::IntRect& start, const Olympia::Platform::IntRect& end, const Olympia::Platform::IntRectRegion& region, bool invalidateSelectionPoints) = 0;
            virtual void notifySelectionHandlesReversed() = 0;

            virtual void cursorChanged(Olympia::Platform::CursorType cursorType, const char* url, const int x, const int y) = 0;

            virtual void requestGeolocationPermission(Olympia::Platform::GeoTrackerListener*, void*, const char*, unsigned int) = 0;
            virtual void cancelGeolocationPermission(Olympia::Platform::GeoTrackerListener*, void*) = 0;
            virtual Olympia::Platform::NetworkStreamFactory* networkStreamFactory() = 0;
            virtual Olympia::Platform::SocketStreamFactory* socketStreamFactory() = 0;

            virtual void handleStringPattern(const unsigned short* pattern, unsigned int length) = 0;
            virtual void handleExternalLink(const Platform::NetworkRequest&, const unsigned short* context, unsigned int contextLength, bool isClientRedirect) = 0;

            virtual void resetBackForwardList(unsigned int listSize, unsigned int currentIndex) = 0;

            virtual void openPopupList(bool multiple, const int size, const ScopeArray<WebString>& labels, bool* enableds, const int* itemType, bool* selecteds) = 0;
            virtual void openDateTimePopup(const int type, const WebString& value, const WebString& min, const WebString& max, const double step) = 0;
            virtual void openColorPopup(const WebString& value) = 0;

            virtual bool chooseFilenames(bool allowMultiple, const WebString& acceptTypes, const SharedArray<WebString>& initialFiles, unsigned int initialFileSize, SharedArray<WebString>& chosenFiles, unsigned int& chosenFileSize) = 0;

            virtual void loadPluginForMimetype(int, int width, int height, const SharedArray<Olympia::WebKit::WebString>& paramNames, const SharedArray<Olympia::WebKit::WebString>& paramValues, int size, const char* url) = 0;
            virtual void notifyPluginRectChanged(int, Platform::IntRect rectChanged) = 0;
            virtual void destroyPlugin(int) = 0;
            virtual void playMedia(int) = 0;
            virtual void pauseMedia(int) = 0;
            virtual float getTime(int) = 0;
            virtual void setTime(int, float) = 0;
            virtual void setVolume(int, float) = 0;
            virtual void setMuted(int, bool) = 0;

            virtual WebPage* createWindow(int x, int y, int width, int height, unsigned flags, const WebString& url, const WebString& windowName) = 0;

            virtual void scheduleCloseWindow() = 0;

            // Database interface.
            virtual unsigned long long databaseQuota(const unsigned short* origin, unsigned originLength,
                                                     const unsigned short* databaseName, unsigned databaseNameLength,
                                                     unsigned long long totalUsage, unsigned long long originUsage,
                                                     unsigned long long estimatedSize) = 0;

            virtual void setIconForUrl(const char* originalPageUrl, const char* finalPageUrl, const char* iconUrl) = 0;
            virtual void setFavicon(int width, int height, unsigned char* iconData) = 0;
            virtual void setLargeIcon(const char* iconUrl) = 0;
            virtual void setWebAppCapable() = 0;
            virtual void setSearchProviderDetails(const char* title, const char* documentUrl) = 0;
            virtual void setAlternateFeedDetails(const char* title, const char* feedUrl) = 0;

            virtual WebString getErrorPage(int errorCode, const char* errorMessage, const char* url) = 0;

            virtual void willDeferLoading() = 0;
            virtual void didResumeLoading() = 0;

            // headers is a list of alternating key and value
            virtual void setMetaHeaders(const ScopeArray<WebString>& headers, unsigned int headersSize) = 0;

            virtual void needMoreData() = 0;
            virtual void handleWebInspectorMessageToFrontend(int id, const char* message, int length) = 0;

            virtual bool hasPendingScrollOrZoomEvent() const = 0;
            virtual Olympia::Platform::IntRect userInterfaceBlittedDestinationRect() const = 0;
            virtual Olympia::Platform::IntRect userInterfaceBlittedVisibleContentsRect() const = 0;

            virtual void resetBitmapZoomScale(double scale) = 0;
            virtual void animateBlockZoom(const Olympia::Platform::FloatPoint& finalPoint, double finalScale) = 0;

            virtual void setPreventsScreenIdleDimming(bool noDimming) = 0;
            virtual void authenticationChallenge(const unsigned short* realm, unsigned int realmLength, WebString& username, WebString& password) = 0;

            virtual bool shouldPluginEnterFullScreen() = 0;
            virtual void didPluginEnterFullScreen() = 0;
            virtual void didPluginExitFullScreen() = 0;
            virtual void onPluginStartBackgroundPlay() = 0;
            virtual void onPluginStopBackgroundPlay() = 0;
            virtual bool lockOrientation(bool landscape) = 0;
            virtual void unlockOrientation() = 0;

#if defined(__QNXNTO__)
            virtual void setToolTip(WebString) = 0;
            virtual void setStatus(WebString) = 0;
#endif
            virtual bool acceptNavigationRequest(const Olympia::Platform::NetworkRequest&,
                                                 Olympia::Platform::NavigationType) = 0;
            virtual void cursorEventModeChanged(Olympia::Platform::CursorEventMode) = 0;
            virtual void touchEventModeChanged(Olympia::Platform::TouchEventMode) = 0;

            virtual bool downloadAllowed(const char* url) = 0;
            virtual void downloadRequested(const Olympia::Platform::NetworkRequest&) = 0;
            virtual void downloadRequested(Olympia::Platform::FilterStream* stream, const WebString& suggestedFilename) = 0;

            virtual int fullscreenStart(const char* contextName,
                Olympia::Platform::Graphics::Window* window,
                unsigned x, unsigned y,
                unsigned width, unsigned height) = 0;

            virtual int fullscreenStop() = 0;

            virtual int fullscreenWindowSet(unsigned x, unsigned y, unsigned width, unsigned height) = 0;

            virtual void drawVerticalScrollbar() = 0;
            virtual void drawHorizontalScrollbar() = 0;
        };
    }
}

#endif // WebPageClient_h
