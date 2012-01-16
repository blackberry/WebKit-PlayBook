/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 */

#ifndef TouchEventHandler_h
#define TouchEventHandler_h

#include "ChromeClient.h"
#include "IntPoint.h"
#include "BlackBerryPlatformTouchEvents.h"

namespace WebCore {
class IntPoint;
class IntRect;
class Node;
class Element;
class Document;
}

namespace Olympia {
namespace WebKit {

class WebPagePrivate;

class TouchEventHandler {
public:
    TouchEventHandler(WebPagePrivate* webpage);
    ~TouchEventHandler();

    bool handleTouchPoint(Olympia::Platform::TouchPoint&);
    void touchEventCancel();
    void touchEventCancelAndClearFocusedNode();
    void touchHoldEvent();

    bool elementUnderFatFingerIsTextInput() { return m_elementUnderFatFingerIsTextInput; }
    WebCore::Element* elementUnderFatFinger() { return m_elementUnderFatFinger; }
    WebCore::IntPoint fatFingerPoint() { return m_fatFingerPoint; }

private:
    void handleFatFingerPressed();

    WebPagePrivate* m_webPage;

    bool m_didCancelTouch;
    bool m_convertTouchToMouse;
    WebCore::Element* m_elementUnderFatFinger;
    bool m_elementUnderFatFingerIsTextInput;
    WebCore::TouchEventMode m_existingTouchMode;

    WebCore::IntPoint m_fatFingerPoint; // Content Position
    WebCore::IntPoint m_lastScreenPoint; // Screen Position
};

}
}

#endif // TouchEventHandler_h
