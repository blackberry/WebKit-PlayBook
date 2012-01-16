/*
 * Copyright (C) 2010, 2011 Research In Motion Limited. All rights reserved.
 */

#include "config.h"
#include "FatFingers.h"

#include "BlackBerryPlatformLog.h"
#include "BlackBerryPlatformScreen.h"
#include "BlackBerryPlatformSettings.h"
#include "CSSComputedStyleDeclaration.h"
#include "CSSParser.h"
#include "Document.h"
#include "Element.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "FloatQuad.h"
#include "Frame.h"
#include "FrameView.h"
#include "HitTestResult.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLNames.h"
#include "HTMLTextAreaElement.h"
#include "InputElement.h"
#include "NodeList.h"
#include "Range.h"
#include "RenderObject.h"
#include "Text.h"
#include "TextBreakIterator.h"
#include "WebPage_p.h"

#if DEBUG_FAT_FINGERS
#include "BackingStore.h"
#endif

using Olympia::Platform::LogLevelInfo;
using Olympia::Platform::log;
using WTF::RefPtr;
using WTF::String;
using WTF::Vector;

using namespace WebCore;

// Lets make the top padding bigger than other directions, since it gets us more
// accurate clicking results.

namespace Olympia {
namespace WebKit {

#if DEBUG_FAT_FINGERS
IntRect FatFingers::m_debugFatFingerRect;
IntPoint FatFingers::m_debugFatFingerClickPosition;
IntPoint FatFingers::m_debugFatFingerAdjustedPosition;
#endif

static inline IntRect fingerRectFromPoint(const IntPoint& point)
{
    return HitTestResult::rectForPoint(point,
                                       Olympia::Platform::Settings::get()->topFatFingerPadding(),
                                       Olympia::Platform::Settings::get()->rightFatFingerPadding(),
                                       Olympia::Platform::Settings::get()->bottomFatFingerPadding(),
                                       Olympia::Platform::Settings::get()->leftFatFingerPadding());
}

static IntRect transformedBoundingBoxForRange(const RefPtr<Range>& range)
{
    // based on Range::boundingBox, which does not handle transforms, and
    // RenderObject::absoluteBoundingBoxRect, which does
    IntRect result;
    Vector<FloatQuad> quads;
    range->textQuads(quads);
    const size_t n = quads.size();
    for (size_t i = 0; i < n; ++i)
        result.unite(quads[i].enclosingBoundingBox());
    return result;
}

static bool hasMousePressListener(Element* element)
{
    ASSERT(element);
    return element->hasEventListeners(eventNames().clickEvent)
        || element->hasEventListeners(eventNames().mousedownEvent)
        || element->hasEventListeners(eventNames().mouseupEvent);
}

static bool isElementClickable(Element* element)
{
    ASSERT(element);

    ExceptionCode ec = 0;

    // FIXME: We fall back to checking for the presence of CSS style "cursor: pointer" to indicate whether the element A
    // can be clicked when A neither registers mouse events handlers nor is a hyperlink or form control. This workaround
    // ensures that we don't break various Google web apps, including <http://maps.google.com>. Ideally, we should walk
    // up the DOM hierarchy to determine the first parent element that accepts mouse events.
    // Consider the HTML snippet: <div id="A" onclick="..."><div id="B">Example</div></div>
    // Notice, B is not a hyperlink, or form control, and does not register any mouse event handler. Then B cannot
    // be clicked. Suppose B specified the CSS property "cursor: pointer". Then, B will be considered as clickable.
    return hasMousePressListener(element)
        || element->webkitMatchesSelector("a,*:link,*:visited,*[role=button],button,input,select,label,area[href],textarea", ec)
        || computedStyle(element)->getPropertyValue(cssPropertyID("cursor")) == "pointer"
        || element->isContentEditable();
}

static inline bool isFieldWithText(Node* node)
{
    ASSERT(node);
    if (!node || !node->isElementNode())
        return false;

    Element* element = toElement(node);

    // check for an input element with text in it
    InputElement* inputElement = toInputElement(element);
    if (inputElement)
        return inputElement->isTextField() && !inputElement->value().isEmpty();

    // check for textarea element with text in it
    if (element->hasTagName(WebCore::HTMLNames::textareaTag)) {
        HTMLTextAreaElement* textAreaElement = static_cast<HTMLTextAreaElement*>(element);
        return !textAreaElement->value().isEmpty();
    }

    return false;
}

static inline int distanceBetweenPoints(const IntPoint& p1, const IntPoint& p2)
{
    int dx = p1.x() - p2.x();
    int dy = p1.y() - p2.y();
    return sqrt((double)((dx * dx) + (dy * dy)));
}

static bool isValidFrameOwner(WebCore::Element* element)
{
    ASSERT(element);
    return element->isFrameOwnerElement() && static_cast<HTMLFrameOwnerElement*>(element)->contentFrame();
}

FatFingers::FatFingers(WebPagePrivate* webPage)
    : m_webPage(webPage)
{
}

FatFingers::~FatFingers()
{
}

// NOTE: both parameter and the return values of this method are in *contents coordinates*.
IntPoint FatFingers::findBestPoint(const IntPoint& contentPos, TargetType targetType, Node*& nodeUnderFatFinger)
{
    nodeUnderFatFinger = 0;

    ASSERT(m_webPage);

#if DEBUG_FAT_FINGERS
    m_debugFatFingerRect = IntRect(0, 0, 0, 0);
    m_debugFatFingerClickPosition = m_webPage->mapToTransformed(m_webPage->mapFromContentsToViewport(contentPos));
    m_debugFatFingerAdjustedPosition = m_webPage->mapToTransformed(m_webPage->mapFromContentsToViewport(contentPos));
#endif

    if (!m_webPage->m_mainFrame)
        return contentPos;

    Vector<IntersectingRect> intersectingRects;
    bool foundOne = findIntersectingRects(contentPos, IntSize(0, 0), m_webPage->m_mainFrame->document(), targetType, intersectingRects, nodeUnderFatFinger);

#if DEBUG_FAT_FINGERS
    // force blit to make the fat fingers rects show up
    if (!m_debugFatFingerRect.isEmpty())
        m_webPage->m_backingStore->repaint(0, 0, m_webPage->transformedViewportSize().width(), m_webPage->transformedViewportSize().height(), true, true);
#endif

    if (!foundOne)
        return contentPos;

    Node* bestNode = 0;
    IntRect largestIntersectionRect;
    IntPoint bestPoint;

    Vector<IntersectingRect>::const_iterator endIt = intersectingRects.end();
    for (Vector<IntersectingRect>::const_iterator it = intersectingRects.begin(); it != endIt; ++it) {
        Node* currentNode = it->first;
        IntRect currentIntersectionRect = it->second;

        int currentIntersectionRectArea = currentIntersectionRect.width() * currentIntersectionRect.height();
        int largestIntersectionRectArea = largestIntersectionRect.width() * largestIntersectionRect.height();
        if (currentIntersectionRectArea > largestIntersectionRectArea
                || (currentIntersectionRectArea == largestIntersectionRectArea
                && distanceBetweenPoints(contentPos, currentIntersectionRect.center()) > distanceBetweenPoints(contentPos, largestIntersectionRect.center()))) {
            bestNode = currentNode;
            largestIntersectionRect = currentIntersectionRect;
        }
    }

    if (!bestNode || largestIntersectionRect.isEmpty())
        return contentPos;

    nodeUnderFatFinger = bestNode;

#if DEBUG_FAT_FINGERS
    m_debugFatFingerAdjustedPosition = m_webPage->mapToTransformed(m_webPage->mapFromContentsToViewport(largestIntersectionRect.center()));
#endif

    return largestIntersectionRect.center();
}

bool FatFingers::checkFingerIntersection(const IntRect& rect, const IntRect& fingerRect, const IntSize& offset, Node* node, Vector<IntersectingRect>& intersectingRects)
{
    IntRect intersectionRect = rect;
    intersectionRect.intersect(fingerRect);
    if (intersectionRect.isEmpty())
        return false;

#if DEBUG_FAT_FINGERS
    String nodeName;
    if (node->isTextNode())
        nodeName = "text node";
    else if (node->isElementNode())
        nodeName = String::format("%s node", toElement(node)->tagName().latin1().data());
    else
        nodeName = "unknown node";
    log(LogLevelInfo, "%s has rect %s, intersecting at %s (area %d)", nodeName.latin1().data(),
            rect.toString().latin1().data(), intersectionRect.toString().latin1().data(),
            intersectionRect.width() * intersectionRect.height());
#endif

    intersectionRect.move(offset);
    intersectingRects.append(std::make_pair(node, intersectionRect));
    return true;
}

bool FatFingers::findIntersectingRects(const IntPoint& contentPos, const IntSize& frameOffset, Document* document, TargetType targetType,
        Vector<IntersectingRect>& intersectingRects, Node*& nodeUnderFatFinger)
{
    if (!document || !document->frame()->view())
        return false;

    // The layout needs to be up-to-date to determine if a node is focusable.
    document->updateLayoutIgnorePendingStylesheets();

    // Create fingerRect.
    IntPoint frameContentPos(contentPos - frameOffset);
    IntPoint screenPoint = m_webPage->mapToTransformed(frameContentPos);
    IntRect screenFingerRect(fingerRectFromPoint(screenPoint));
    IntRect fingerRect = m_webPage->mapFromTransformed(screenFingerRect);

#if DEBUG_FAT_FINGERS
    log(LogLevelInfo, "fat finger rect now %s", fingerRect.toString().latin1().data());
    // only record the first finger rect
    if (document == m_webPage->m_mainFrame->document())
        m_debugFatFingerRect = m_webPage->mapToTransformed(m_webPage->mapFromContentsToViewport(fingerRect));
#endif

    // Convert 'frameContentPos' to viewport coordinates as required by Document::nodesFromRect().
    // Internally, nodesFromRect() converts it back to contents coordinates.
    // Both the scroll offset and the current zoom factor of the page are considered during this
    // conversion.
    IntPoint viewportPoint;
    if (document == m_webPage->m_mainFrame->document())
        viewportPoint = m_webPage->mapFromContentsToViewport(frameContentPos);
    else
        viewportPoint = frameContentPos - document->frame()->view()->scrollOffset();

    RefPtr<NodeList> intersectedNodes = document->nodesFromRect(viewportPoint.x(), viewportPoint.y(),
            Olympia::Platform::Settings::get()->topFatFingerPadding(),
            Olympia::Platform::Settings::get()->rightFatFingerPadding(),
            Olympia::Platform::Settings::get()->bottomFatFingerPadding(),
            Olympia::Platform::Settings::get()->leftFatFingerPadding(),
            false /*ignoreClipping*/);
    if (!intersectedNodes)
        return false;

    // Iterate over the list of nodes (and subrects of nodes where possible), for each saving the intersection of the bounding box with the finger rect
    bool foundOne = false;
    unsigned length = intersectedNodes->length();
    for (unsigned i = 0; i < length; i++) {
        Node* curNode = intersectedNodes->item(i);
        if (!curNode)
            continue;

        if (curNode->isElementNode() && isValidFrameOwner(toElement(curNode))) {
            // Recurse into child frame
            // Adjust client coordinates' origin to be top left of inner frame viewport.
            IntRect boundingRect = curNode->renderer()->absoluteBoundingBoxRect(true /*use transforms*/);
            IntSize offsetToParent(boundingRect.x(), boundingRect.y());

            HTMLFrameOwnerElement* owner = static_cast<HTMLFrameOwnerElement*>(curNode);
            Document* childDocument = owner && owner->contentFrame() ? owner->contentFrame()->document() : 0;
            if (!childDocument)
                continue;

            // Consider if the inner frame itself it also scrolled.
            ASSERT(childDocument->frame()->view());
            offsetToParent -= childDocument->frame()->view()->scrollOffset();

            foundOne |= findIntersectingRects(frameContentPos, frameOffset + offsetToParent, childDocument, targetType, intersectingRects, nodeUnderFatFinger);
        } else if (targetType == ClickableElement)
            foundOne |= checkForClickableElement(fingerRect, frameOffset, document, curNode, intersectingRects, nodeUnderFatFinger);
        else if (targetType == Text)
            foundOne |= checkForText(fingerRect, frameOffset, document, curNode, intersectingRects, nodeUnderFatFinger);
    }

    return foundOne;
}

bool FatFingers::checkForClickableElement(const IntRect& fingerRect, const IntSize& frameOffset, Document* document, Node* curNode,
        Vector<IntersectingRect>& intersectingRects, Node*& nodeUnderFatFinger)
{
    ASSERT(curNode);
    if (!curNode || !curNode->isElementNode())
        return false;

    // Check for plugins
    if (curNode->hasTagName(WebCore::HTMLNames::objectTag) || curNode->hasTagName(WebCore::HTMLNames::embedTag))
        nodeUnderFatFinger = curNode;

    if (!isElementClickable(toElement(curNode)) || !curNode->renderer())
        return false;

    if (curNode->isLink()) {
        // Links can wrap lines, and in such cases Node::getRect() can give us
        // not accurate rects, since it unites all InlineBox's rects. In this
        // cases, we can process each line of the link separately with our
        // intersection rect, getting a more accurate clicking.
        Vector<FloatQuad> quads;
        curNode->renderer()->absoluteQuads(quads);

        bool foundOne = false;
        size_t n = quads.size();
        ASSERT(n);
        for (size_t i = 0; i < n; ++i)
            foundOne |= checkFingerIntersection(quads[i].enclosingBoundingBox(), fingerRect, frameOffset, curNode, intersectingRects);
        return foundOne;
    }

    IntRect boundingRect = curNode->renderer()->absoluteBoundingBoxRect(true /*use transforms*/);
    return checkFingerIntersection(boundingRect, fingerRect, frameOffset, curNode, intersectingRects);
}

bool FatFingers::checkForText(const IntRect& fingerRect, const IntSize& frameOffset, Document* document, Node* curNode,
        Vector<IntersectingRect>& intersectingRects, Node*& nodeUnderFatFinger)
{
    ASSERT(curNode);
    if (!curNode)
        return false;

    if (isFieldWithText(curNode)) {
        // FIXME: Find all text in the field and find the best word.
        // For now, we will just select the whole field.
        IntRect boundingRect = curNode->renderer()->absoluteBoundingBoxRect(true /*use transforms*/);
        return checkFingerIntersection(boundingRect, fingerRect, frameOffset, curNode, intersectingRects);
    }

    if (curNode->isTextNode()) {
        WebCore::Text* curText = static_cast<WebCore::Text*>(curNode);
        String allText = curText->wholeText();

        // Iterate through all words, breaking at whitespace, to find the bounding box of each word
        TextBreakIterator* wordIterator = wordBreakIterator(allText.characters(), allText.length());

        int lastOffset = textBreakFirst(wordIterator);
        if (lastOffset == -1)
            return false;

        bool foundOne = false;
        int offset;
        while ((offset = textBreakNext(wordIterator)) != -1) {
            RefPtr<Range> range = Range::create(document, curText, lastOffset, curText, offset);
            if (!range->text().stripWhiteSpace().isEmpty()) {
#if DEBUG_FAT_FINGERS
                log(LogLevelInfo, "Checking word '%s'", range->text().latin1().data());
#endif
                foundOne |= checkFingerIntersection(transformedBoundingBoxForRange(range), fingerRect, frameOffset, curNode, intersectingRects);
            }
            lastOffset = offset;
        }
        return foundOne;
    }

    return false;
}

}
}

