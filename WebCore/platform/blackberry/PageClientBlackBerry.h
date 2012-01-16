/*
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 */

#ifndef PageClientOlympia_h
#define PageClientOlympia_h

#include "Cursor.h"
#include "WebPageClient.h"

namespace Olympia {
namespace Platform {
    class NetworkStreamFactory;
    class SocketStreamFactory;
namespace Graphics {
    class Window;
}
}
}

namespace WebCore {
    class FloatRect;
    class IntRect;
    class IntSize;
    class Element;
    class PluginView;
}

class PageClientBlackBerry {
public:
    virtual void setCursor(WebCore::PlatformCursorHandle handle) = 0;
    virtual Olympia::Platform::NetworkStreamFactory* networkStreamFactory() = 0;
    virtual Olympia::Platform::SocketStreamFactory* socketStreamFactory() = 0;
    virtual Olympia::Platform::Graphics::Window* platformWindow() const = 0;
    virtual void setPreventsScreenDimming(bool preventDimming) = 0;
    virtual void showVirtualKeyboard(bool showKeyboard) = 0;
    virtual void ensureContentVisible(bool centerInView = true) = 0;
    virtual void zoomToContentRect(const WebCore::IntRect& rect) = 0;
    virtual void registerPlugin(WebCore::PluginView*, bool) = 0;
    virtual void notifyPageOnLoad() = 0;
    virtual bool shouldPluginEnterFullScreen(WebCore::PluginView*, const char*) = 0;
    virtual void didPluginEnterFullScreen(WebCore::PluginView*, const char*) = 0;
    virtual void didPluginExitFullScreen(WebCore::PluginView*, const char*) = 0;
    virtual void onPluginStartBackgroundPlay(WebCore::PluginView*, const char*) = 0;
    virtual void onPluginStopBackgroundPlay(WebCore::PluginView*, const char*) = 0;
    virtual bool lockOrientation(bool landscape) = 0;
    virtual void unlockOrientation() = 0;
    virtual int orientation() const = 0;
    virtual double currentZoomFactor() const = 0;
    virtual WebCore::IntSize viewportSize() const = 0;
    virtual void showAlertDialog(Olympia::WebKit::WebPageClient::AlertType atype) = 0;
};

#endif
