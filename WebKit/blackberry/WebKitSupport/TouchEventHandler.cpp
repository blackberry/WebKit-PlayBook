/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 */

#include "config.h"
#include "TouchEventHandler.h"

#include "HTMLNames.h"
#include "Document.h"
#include "FatFingers.h"
#include "FocusController.h"
#include "Frame.h"
#include "HTMLInputElement.h"
#include "HTMLPlugInElement.h"
#include "InputHandler.h"
#include "IntRect.h"
#include "IntSize.h"
#include "Node.h"
#include "Page.h"
#include "PlatformMouseEvent.h"
#include "PlatformTouchEvent.h"
#include "WebPage_p.h"
#include "WebSettings.h"

#include <wtf/MathExtras.h>

using namespace Olympia::Platform;
using namespace WebCore;

namespace Olympia {
namespace WebKit {

static bool hasMouseMoveListener(WebCore::Element* element)
{
    ASSERT(element);
    return element->hasEventListeners(eventNames().mousemoveEvent) || element->document()->hasEventListeners(eventNames().mousemoveEvent);
}

static bool hasTouchListener(WebCore::Element* element)
{
    ASSERT(element);
    return element->hasEventListeners(eventNames().touchstartEvent)
        || element->hasEventListeners(eventNames().touchmoveEvent)
        || element->hasEventListeners(eventNames().touchcancelEvent)
        || element->hasEventListeners(eventNames().touchendEvent);
}

static bool shouldConvertTouchToMouse(WebCore::Element* element)
{
    if (!element)
        return false;

    // Check if the focus element has a mouse listener and no touch listener.  If so the field
    // will require touch events be converted to mouse events to function properly.
    // Range element are a special case that require natural mouse events in order to allow
    // dragging of the slider handle.
    if (hasMouseMoveListener(element) && !hasTouchListener(element))
        return true;

    // Check for range slider
    if( element->hasTagName(HTMLNames::inputTag)) {
        HTMLInputElement* inputElement = static_cast<HTMLInputElement*>(element);
        if (inputElement->deprecatedInputType() == HTMLInputElement::RANGE)
            return true;
    }

    // Check for plugin
    if ((element->hasTagName(HTMLNames::objectTag) || element->hasTagName(HTMLNames::embedTag)) && static_cast<HTMLPlugInElement*>(element))
        return true;

    return false;
}

TouchEventHandler::TouchEventHandler(WebPagePrivate* webpage)
    : m_webPage(webpage)
    , m_didCancelTouch(false)
    , m_convertTouchToMouse(false)
    , m_elementUnderFatFinger(0)
    , m_elementUnderFatFingerIsTextInput(false)
    , m_existingTouchMode(WebCore::ProcessedTouchEvents)
{
}

TouchEventHandler::~TouchEventHandler()
{
}

void TouchEventHandler::touchEventCancel()
{
    if (!m_elementUnderFatFingerIsTextInput) {
        // Input elements delay mouse down and do not need to be released on touch cancel.
        m_webPage->m_page->focusController()->focusedOrMainFrame()->eventHandler()->setMousePressed(false);
    }
    m_convertTouchToMouse = false;
    m_didCancelTouch = true;
    m_elementUnderFatFinger = 0;
}

void TouchEventHandler::touchEventCancelAndClearFocusedNode()
{
    touchEventCancel();
    m_webPage->clearFocusNode();
}

void TouchEventHandler::touchHoldEvent()
{
    // This is a hack for our hack that converts the touch pressed event that we've delayed because the user has focused a input field
    // to the page as a mouse pressed event.
    if (m_elementUnderFatFingerIsTextInput)
        handleFatFingerPressed();
}

bool TouchEventHandler::handleTouchPoint(Olympia::Platform::TouchPoint& point)
{
    switch (point.m_state) {
    case Olympia::Platform::TouchPoint::TouchPressed:
        {
            m_didCancelTouch = false;
            m_elementUnderFatFinger = 0;
            m_lastScreenPoint = point.m_screenPos;

            WebCore::IntPoint contentPos(m_webPage->mapFromViewportToContents(point.m_pos));
            Node* nodeUnderFatFinger;
            m_fatFingerPoint = FatFingers(m_webPage).findBestPoint(contentPos, FatFingers::ClickableElement, nodeUnderFatFinger);

            if (nodeUnderFatFinger && nodeUnderFatFinger->isElementNode())
                m_elementUnderFatFinger = toElement(nodeUnderFatFinger);
            else
                m_elementUnderFatFinger = 0;
            m_elementUnderFatFingerIsTextInput = false;

            // Set or reset the touch mode.
            m_convertTouchToMouse = shouldConvertTouchToMouse(m_elementUnderFatFinger);
            if (m_elementUnderFatFinger) {
                // Check if the element under the touch point will require a VKB be displayed so that
                // the touch down can be suppressed.
                if (m_elementUnderFatFinger->isTextFormControl() ||
                    m_elementUnderFatFinger->hasTagName(HTMLNames::textareaTag) ||
                    m_elementUnderFatFinger->isContentEditable())
                        m_elementUnderFatFingerIsTextInput = true;
            }

            if (!m_elementUnderFatFingerIsTextInput && m_webPage->m_inputHandler->isInputMode())
                m_elementUnderFatFingerIsTextInput = true;

            // TODO should a text input element with mouse listener get the raw events which will
            // trigger the VKB?
            if (!m_elementUnderFatFingerIsTextInput)
                 handleFatFingerPressed();

            return true;
        }
    case Olympia::Platform::TouchPoint::TouchReleased:
        {
            if (m_elementUnderFatFingerIsTextInput)
                handleFatFingerPressed();

            WebCore::IntPoint adjustedPoint;
            if (m_convertTouchToMouse) {
                adjustedPoint = point.m_pos;
                m_convertTouchToMouse = false;
            } else // Fat finger point in viewport coordinates.
                adjustedPoint = m_webPage->mapFromContentsToViewport(m_fatFingerPoint);

            // Create  MouseReleased Event.
            WebCore::PlatformMouseEvent mouseEvent(adjustedPoint, m_lastScreenPoint, WebCore::MouseEventReleased, 1, WebCore::TouchScreen);
            m_webPage->handleMouseEvent(mouseEvent);
            return true;
        }
    case Olympia::Platform::TouchPoint::TouchMoved:
        if (m_convertTouchToMouse) {
            WebCore::PlatformMouseEvent mouseEvent(point.m_pos, m_lastScreenPoint, WebCore::MouseEventMoved, 1, WebCore::TouchScreen);
            m_lastScreenPoint = point.m_screenPos;
            if (!m_webPage->handleMouseEvent(mouseEvent)) {
                m_convertTouchToMouse = false;
                return false;
            }
            return true;
        }
        break;
    default:
        break;
    }
    return false;
}

void TouchEventHandler::handleFatFingerPressed()
{
    if (!m_didCancelTouch) {

        // Convert touch event to a mouse event
        // First update the mouse position with a MouseMoved event
        // Send the mouse move event
        WebCore::PlatformMouseEvent mouseMoveEvent(m_webPage->mapFromContentsToViewport(m_fatFingerPoint), m_lastScreenPoint, WebCore::MouseEventMoved, 0, WebCore::TouchScreen);
        m_webPage->handleMouseEvent(mouseMoveEvent);

        // Then send the MousePressed event
        WebCore::PlatformMouseEvent mousePressedEvent(m_webPage->mapFromContentsToViewport(m_fatFingerPoint), m_lastScreenPoint, WebCore::MouseEventPressed, 1, WebCore::TouchScreen);
        m_webPage->handleMouseEvent(mousePressedEvent);
    }
}

}
}
