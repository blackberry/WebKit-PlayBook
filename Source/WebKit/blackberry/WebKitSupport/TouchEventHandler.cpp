/*
 * Copyright (C) 2010, 2011 Research In Motion Limited. All rights reserved.
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
#include "TouchEventHandler.h"

#include "DOMSupport.h"
#include "Document.h"
#include "DocumentMarkerController.h"
#include "FatFingers.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameView.h"
#include "HTMLAnchorElement.h"
#include "HTMLAreaElement.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLPlugInElement.h"
#include "InputHandler.h"
#include "IntRect.h"
#include "IntSize.h"
#include "Node.h"
#include "Page.h"
#include "PlatformMouseEvent.h"
#include "PlatformTouchEvent.h"
#include "RenderLayer.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "RenderedDocumentMarker.h"
#include "SelectionHandler.h"
#include "WebPage_p.h"
#include "WebSettings.h"

#include <wtf/MathExtras.h>

using namespace BlackBerry::Platform;
using namespace WebCore;
using namespace WTF;

namespace BlackBerry {
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

    // Check if the focus element has a mouse listener and no touch listener. If so the field
    // will require touch events be converted to mouse events to function properly.
    // Range element are a special case that require natural mouse events in order to allow
    // dragging of the slider handle.
    if (hasMouseMoveListener(element) && !hasTouchListener(element))
        return true;

    // Check for range slider
    if (element->hasTagName(HTMLNames::inputTag)) {
        HTMLInputElement* inputElement = static_cast<HTMLInputElement*>(element);
        if (inputElement->isRangeControl())
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
    , m_existingTouchMode(WebCore::ProcessedTouchEvents)
{
}

TouchEventHandler::~TouchEventHandler()
{
}

bool TouchEventHandler::shouldSuppressMouseDownOnTouchDown() const
{
    return m_lastFatFingersResult.isTextInput() || m_webPage->m_inputHandler->isInputMode() || m_webPage->m_selectionHandler->isSelectionActive();
}

void TouchEventHandler::touchEventCancel()
{
    if (!shouldSuppressMouseDownOnTouchDown()) {
        // Input elements delay mouse down and do not need to be released on touch cancel.
        m_webPage->m_page->focusController()->focusedOrMainFrame()->eventHandler()->setMousePressed(false);
    }
    m_convertTouchToMouse = false;
    m_didCancelTouch = true;

    // If we cancel a single touch event, we need to also clean up any hover
    // state we get into by synthetically moving the mouse to the m_fingerPoint.
    Element* elementUnderFatFinger = m_lastFatFingersResult.nodeAsElementIfApplicable();
    if (elementUnderFatFinger && elementUnderFatFinger->renderer()) {

        HitTestRequest request(HitTestRequest::FingerUp);
        // The HitTestResult point is not actually needed.
        HitTestResult result(WebCore::IntPoint::zero());
        result.setInnerNode(elementUnderFatFinger);

        Document* document = elementUnderFatFinger->document();
        ASSERT(document);
        document->renderView()->layer()->updateHoverActiveState(request, result);
        document->updateStyleIfNeeded();
        // Updating the document style may destroy the renderer.
        if (elementUnderFatFinger->renderer())
            elementUnderFatFinger->renderer()->repaint();
        ASSERT(!elementUnderFatFinger->hovered());
    }

    m_lastFatFingersResult.reset();
    hideTapHighlight();
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
    if (shouldSuppressMouseDownOnTouchDown())
        handleFatFingerPressed();

    // Clear the focus ring indication if tap-and-hold'ing on a link.
    if (m_lastFatFingersResult.validNode() && m_lastFatFingersResult.validNode()->isLink())
        m_webPage->clearFocusNode();
}

bool TouchEventHandler::handleTouchPoint(BlackBerry::Platform::TouchPoint& point)
{
    switch (point.m_state) {
    case BlackBerry::Platform::TouchPoint::TouchPressed:
        {
            m_lastFatFingersResult.reset(); // Theoretically this shouldn't be required. Keep it just in case states get mangled.
            m_didCancelTouch = false;
            m_lastScreenPoint = point.m_screenPos;

            WebCore::IntPoint contentPos(m_webPage->mapFromViewportToContents(point.m_pos));

            m_lastFatFingersResult = FatFingers(m_webPage, contentPos, FatFingers::ClickableElement).findBestPoint();

            Element* elementUnderFatFinger = 0;
            if (m_lastFatFingersResult.positionWasAdjusted() && m_lastFatFingersResult.validNode()) {
                ASSERT(m_lastFatFingersResult.validNode()->isElementNode());
                elementUnderFatFinger = m_lastFatFingersResult.nodeAsElementIfApplicable();
            }

            // Set or reset the touch mode.
            Element* possibleTargetNodeForMouseMoveEvents = static_cast<Element*>(m_lastFatFingersResult.positionWasAdjusted() ? elementUnderFatFinger : m_lastFatFingersResult.validNode());
            m_convertTouchToMouse = shouldConvertTouchToMouse(possibleTargetNodeForMouseMoveEvents);

            if (elementUnderFatFinger)
                drawTapHighlight();

            // TODO should a text input element with mouse listener get the raw events which will
            // trigger the VKB?
            if (!shouldSuppressMouseDownOnTouchDown())
                 handleFatFingerPressed();

            return true;
        }
    case BlackBerry::Platform::TouchPoint::TouchReleased:
        {
            unsigned int spellLength = spellCheck(point);

            hideTapHighlight();

            if (shouldSuppressMouseDownOnTouchDown())
                handleFatFingerPressed();

            // The rebase has eliminated a necessary event when the mouse does not
            // trigger an actual selection change preventing re-showing of the
            // keyboard. If input mode is active, call setNavigationMode which
            // will update the state and display keyboard if needed.
            if (m_webPage->m_inputHandler->isInputMode())
                m_webPage->m_inputHandler->setNavigationMode(true);

            WebCore::IntPoint adjustedPoint;
            if (m_convertTouchToMouse) {
                adjustedPoint = point.m_pos;
                m_convertTouchToMouse = false;
            } else // Fat finger point in viewport coordinates.
                adjustedPoint = m_webPage->mapFromContentsToViewport(m_lastFatFingersResult.adjustedPosition());

            // Create  MouseReleased Event.
            WebCore::PlatformMouseEvent mouseEvent(adjustedPoint, m_lastScreenPoint, WebCore::MouseEventReleased, 1, LeftButton, WebCore::TouchScreen);
            m_webPage->handleMouseEvent(mouseEvent);
            m_lastFatFingersResult.reset(); // reset the fat finger result as its no longer valid when a user's finger is not on the screen.
            if (spellLength) {
                unsigned int end = m_webPage->m_inputHandler->caretPosition();
                unsigned int start = end - spellLength;
                m_webPage->m_client->requestSpellingSuggestionsForString(start, end);
            }
            return true;
        }
    case BlackBerry::Platform::TouchPoint::TouchMoved:
        if (m_convertTouchToMouse) {
            WebCore::PlatformMouseEvent mouseEvent(point.m_pos, m_lastScreenPoint, WebCore::MouseEventMoved, 1, LeftButton, WebCore::TouchScreen);
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

unsigned TouchEventHandler::spellCheck(BlackBerry::Platform::TouchPoint& touchPoint)
{
    Element* elementUnderFatFinger = m_lastFatFingersResult.nodeAsElementIfApplicable();
    if (!m_lastFatFingersResult.isTextInput() || !elementUnderFatFinger)
        return 0;

    WebCore::IntPoint contentPos(m_webPage->mapFromViewportToContents(touchPoint.m_pos));
    contentPos = DOMSupport::convertPointToFrame(m_webPage->mainFrame(), m_webPage->focusedOrMainFrame(), contentPos);

    Document* document = elementUnderFatFinger->document();
    ASSERT(document);
    RenderedDocumentMarker* marker = document->markers()->renderedMarkerContainingPoint(contentPos, DocumentMarker::Spelling);
    if (!marker)
        return 0;

    WebCore::IntRect rect = marker->renderedRect();
    WebCore::IntPoint newContentPos = WebCore::IntPoint(rect.x() + rect.width(), rect.y() + rect.height() / 2); // midway of right edge
    Frame* frame = m_webPage->focusedOrMainFrame();
    if (frame != m_webPage->mainFrame())
        newContentPos = m_webPage->mainFrame()->view()->windowToContents(frame->view()->contentsToWindow(newContentPos));
    m_lastFatFingersResult.m_adjustedPosition = newContentPos;
    m_lastFatFingersResult.m_positionWasAdjusted = true;
    return marker->endOffset() - marker->startOffset();
}

void TouchEventHandler::handleFatFingerPressed()
{
    if (!m_didCancelTouch) {

        // Convert touch event to a mouse event
        // First update the mouse position with a MouseMoved event
        // Send the mouse move event
        WebCore::PlatformMouseEvent mouseMoveEvent(m_webPage->mapFromContentsToViewport(m_lastFatFingersResult.adjustedPosition()), m_lastScreenPoint, WebCore::MouseEventMoved, 0, LeftButton, WebCore::TouchScreen);
        m_webPage->handleMouseEvent(mouseMoveEvent);

        // Then send the MousePressed event
        WebCore::PlatformMouseEvent mousePressedEvent(m_webPage->mapFromContentsToViewport(m_lastFatFingersResult.adjustedPosition()), m_lastScreenPoint, WebCore::MouseEventPressed, 1, LeftButton, WebCore::TouchScreen);
        m_webPage->handleMouseEvent(mousePressedEvent);
    }
}

// This method filters what element will get tap-highlight'ed or not. To start with,
// we are going to highlight links (anchors with a valid href element), and elements
// whose tap highlight color value is different than the default value.
static Element* elementForTapHighlight(WebCore::Element* elementUnderFatFinger)
{
    bool isArea = elementUnderFatFinger->hasTagName(HTMLNames::areaTag);

    // Do not bail out right way here if there element does not have a renderer. It is the case
    // for <map> (descendent of <area>) elements. The associated <image> element actually has the
    // renderer.
    if (elementUnderFatFinger->renderer()) {
        WebCore::Color tapHighlightColor = elementUnderFatFinger->renderStyle()->tapHighlightColor();
        if (tapHighlightColor != RenderTheme::defaultTheme()->platformTapHighlightColor())
            return elementUnderFatFinger;
    }

    Node* linkNode = elementUnderFatFinger->enclosingLinkEventParentOrSelf();
    if (!linkNode || !linkNode->isHTMLElement() || (!linkNode->renderer() && !isArea))
        return 0;

    ASSERT(linkNode->isLink());

    // FatFingers class selector ensure only anchor with valid href attr value get here.
    // It includes empty hrefs.
    WebCore::Element* highlightCandidateElement = static_cast<WebCore::Element*>(linkNode);

    if (!isArea)
        return highlightCandidateElement;

    HTMLAreaElement* area = static_cast<HTMLAreaElement*>(highlightCandidateElement);
    HTMLImageElement* image = area->imageElement();
    if (image && image->renderer())
        return image;

    return 0;
}

void TouchEventHandler::drawTapHighlight()
{
    Element* elementUnderFatFinger = m_lastFatFingersResult.nodeAsElementIfApplicable();
    if (!elementUnderFatFinger)
        return;

    WebCore::Element* element = elementForTapHighlight(elementUnderFatFinger);
    if (!element)
        return;

    // Get the element bounding rect in transformed coordinates so we can extract
    // the focus ring relative position each rect.
    RenderObject* renderer = element->renderer();
    ASSERT(renderer);

    Frame* elementFrame = element->document()->frame();
    ASSERT(elementFrame);

    FrameView* elementFrameView = elementFrame->view();
    if (!elementFrameView)
        return;

    // Tell the client if the element is either in a scrollable container or in a fixed positioned container.
    // On the client side, this info is being used to hide the tap highlight window on scroll.
    RenderLayer* layer = m_webPage->enclosingFixedPositionedAncestorOrSelfIfFixedPositioned(renderer->enclosingLayer());
    bool shouldHideTapHighlightRightAfterScrolling = !layer->renderer()->isRenderView();
    shouldHideTapHighlightRightAfterScrolling |= !!m_webPage->m_inRegionScrollStartingNode.get();

    WebCore::IntPoint framePos(m_webPage->frameOffset(elementFrame));

    // FIXME: We can get more precise on the MAP case by calculating the rect with HTMLAreaElement::computeRect().
    WebCore::IntRect absoluteRect = renderer->absoluteClippedOverflowRect();
    absoluteRect.move(framePos.x(), framePos.y());

    WebCore::IntRect clippingRect;
    if (elementFrame == m_webPage->mainFrame())
        clippingRect = WebCore::IntRect(WebCore::IntPoint(0, 0), elementFrameView->contentsSize());
    else
        clippingRect = m_webPage->mainFrame()->view()->windowToContents(m_webPage->getRecursiveVisibleWindowRect(elementFrameView, true /*noClipToMainFrame*/));
    clippingRect = intersection(absoluteRect, clippingRect);

    Vector<WebCore::FloatQuad> focusRingQuads;
    renderer->absoluteFocusRingQuads(focusRingQuads);

    Platform::IntRectRegion region;
    for (size_t i = 0; i < focusRingQuads.size(); ++i) {
        WebCore::IntRect rect = focusRingQuads[i].enclosingBoundingBox();
        rect.move(framePos.x(), framePos.y());
        WebCore::IntRect clippedRect = intersection(clippingRect, rect);
        clippedRect.inflate(2);
        region = unionRegions(region, Platform::IntRect(clippedRect));
    }

    WebCore::Color highlightColor = element->renderStyle()->tapHighlightColor();

    m_webPage->m_client->drawTapHighlight(region,
                                          highlightColor.red(),
                                          highlightColor.green(),
                                          highlightColor.blue(),
                                          highlightColor.alpha(),
                                          shouldHideTapHighlightRightAfterScrolling);
}

void TouchEventHandler::hideTapHighlight()
{
    m_webPage->m_client->hideTapHighlight();
}

}
}
