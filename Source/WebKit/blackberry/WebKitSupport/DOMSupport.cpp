/*
 * Copyright (C) 2011 Research In Motion Limited. All rights reserved.
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
#include "DOMSupport.h"

#include "FloatQuad.h"
#include "Frame.h"
#include "FrameView.h"
#include "HTMLFormElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLTextAreaElement.h"
#include "Node.h"
#include "Range.h"
#include "RenderObject.h"
#include "RenderText.h"
#include "RenderTextControl.h"
#include "TextIterator.h"
#include "VisibleSelection.h"
#include "WTFString.h"

#include <limits>

using WTF::Vector;

using namespace WebCore;

namespace BlackBerry {
namespace WebKit {
namespace DOMSupport {

void visibleTextQuads(const Range& range, Vector<FloatQuad>& quads, bool useSelectionHeight)
{
    // Range::textQuads includes hidden text, which we don't want.
    // To work around this, this is a copy of it which skips hidden elements.
    Node* startContainer = range.startContainer();
    Node* endContainer = range.endContainer();

    if (!startContainer || !endContainer)
        return;

    Node* stopNode = range.pastLastNode();
    for (Node* node = range.firstNode(); node != stopNode; node = node->traverseNextNode()) {
        RenderObject* r = node->renderer();
        if (!r || !r->isText())
            continue;

        if (r->style()->visibility() != VISIBLE)
            continue;

        RenderText* renderText = toRenderText(r);
        int startOffset = node == startContainer ? range.startOffset() : 0;
        int endOffset = node == endContainer ? range.endOffset() : std::numeric_limits<int>::max();
        renderText->absoluteQuadsForRange(quads, startOffset, endOffset, useSelectionHeight);
    }
}

bool isTextInputElement(Element* element)
{
    return element->isTextFormControl()
           || element->hasTagName(HTMLNames::textareaTag)
           || element->isContentEditable();
}

bool isPasswordElement(const Element* element)
{
    return element && element->hasTagName(HTMLNames::inputTag)
           && static_cast<const HTMLInputElement*>(element)->isPasswordField();
}

WTF::String inputElementText(Element* element)
{
    if (!element)
        return WTF::String();

    WTF::String elementText;
    if (element->hasTagName(HTMLNames::inputTag)) {
        const HTMLInputElement* inputElement = static_cast<const HTMLInputElement*>(element);
        elementText = inputElement->value();
    } else if (element->hasTagName(HTMLNames::textareaTag)) {
        const HTMLTextAreaElement* inputElement = static_cast<const HTMLTextAreaElement*>(element);
        elementText = inputElement->value();
    } else if (element->isContentEditable()) {
        RefPtr<Range> rangeForNode = rangeOfContents(element);
        elementText = rangeForNode.get()->text();
    }
    return elementText;
}

bool isElementTypePlugin(const WebCore::Element* element)
{
    if (!element)
        return false;

    if (element->hasTagName(HTMLNames::objectTag)
        || element->hasTagName(HTMLNames::embedTag)
        || element->hasTagName(HTMLNames::appletTag))
        return true;

    return false;
}

WebCore::HTMLTextFormControlElement* toTextControlElement(WebCore::Node* node)
{
    if (!(node && node->isElementNode()))
        return 0;

    Element* element = static_cast<Element*>(node);
    if (!element->isFormControlElement())
        return 0;

    HTMLFormControlElement* formElement = static_cast<HTMLFormControlElement*>(element);
    if (!formElement->isTextFormControl())
        return 0;

    return static_cast<HTMLTextFormControlElement*>(formElement);
}

bool isPopupInputField(const WebCore::Element* element)
{
    return isDateTimeInputField(element) || isColorInputField(element);
}

bool isDateTimeInputField(const WebCore::Element* element)
{
    if (!element->hasTagName(HTMLNames::inputTag))
        return false;

    const HTMLInputElement* inputElement = static_cast<const HTMLInputElement*>(element);

    // The following types have popup's.
    if (inputElement->isDateControl()
        || inputElement->isDateTimeControl()
        || inputElement->isDateTimeLocalControl()
        || inputElement->isTimeControl()
        || inputElement->isMonthControl())
            return true;

    return false;
}

bool isColorInputField(const WebCore::Element* element)
{
    if (!element->hasTagName(HTMLNames::inputTag))
        return false;

    const HTMLInputElement* inputElement = static_cast<const HTMLInputElement*>(element);

#if ENABLE(INPUT_COLOR)
    if (inputElement->isColorControl())
        return true;
#endif

    return false;
}

AttributeState elementSupportsAutocorrect(const WebCore::Element* element)
{
    // First we check the input item itself. If the attribute is not defined,
    // we check its parent form.
    QualifiedName autocorrectAttr = QualifiedName(nullAtom, "autocorrect", nullAtom);
    if (element->fastHasAttribute(autocorrectAttr)) {
        AtomicString attributeString = element->fastGetAttribute(autocorrectAttr);
        if (equalIgnoringCase(attributeString, "off"))
            return Off;
        if (equalIgnoringCase(attributeString, "on"))
            return On;
        // If we haven't returned, it wasn't set properly. Check the form for an explicit setting
        // because the attribute was provided, but invalid.
    }
    if (element->isFormControlElement()) {
        const HTMLFormControlElement* formElement = static_cast<const HTMLFormControlElement*>(element);
        if (formElement->form() && formElement->form()->fastHasAttribute(autocorrectAttr)) {
            AtomicString attributeString = formElement->form()->fastGetAttribute(autocorrectAttr);
            if (equalIgnoringCase(attributeString, "off"))
                return Off;
            if (equalIgnoringCase(attributeString, "on"))
                return On;
        }
    }
    return Default;
}

// Check if this is an input field that will be focused & require input support.
bool isTextBasedContentEditableElement(Element* element)
{
    if (!element)
        return false;

    if (element->isReadOnlyFormControl())
        return false;

    if (isPopupInputField(element))
        return false;

    return element->isTextFormControl() || element->isContentEditable();
}

IntRect transformedBoundingBoxForRange(const Range& range)
{
    // based on Range::boundingBox, which does not handle transforms, and
    // RenderObject::absoluteBoundingBoxRect, which does
    IntRect result;
    Vector<FloatQuad> quads;
    visibleTextQuads(range, quads);
    const size_t n = quads.size();
    for (size_t i = 0; i < n; ++i)
        result.unite(quads[i].enclosingBoundingBox());
    return result;
}

VisibleSelection visibleSelectionForInputElement(Element* element)
{
    return visibleSelectionForRangeInputElement(element, 0, inputElementText(element).length());
}

VisibleSelection visibleSelectionForRangeInputElement(Element* element, int start, int end)
{
    HTMLTextFormControlElement* controlElement = DOMSupport::toTextControlElement(element);
    if (controlElement) {
        RenderTextControl* textRender = toRenderTextControl(element->renderer());
        if (!textRender)
            return VisibleSelection();

        VisiblePosition startPosition = textRender->visiblePositionForIndex(start);
        VisiblePosition endPosition;
        if (start == end)
            endPosition = startPosition;
        else
            endPosition = textRender->visiblePositionForIndex(end);

        return VisibleSelection(startPosition, endPosition);
    }

    // Must be content editable, generate the range.
    RefPtr<Range> selectionRange = TextIterator::rangeFromLocationAndLength(element, start, end - start);
    if (start == end)
        return VisibleSelection(selectionRange.get()->startPosition(), DOWNSTREAM);

    VisiblePosition visibleStart(selectionRange->startPosition(), DOWNSTREAM);
    VisiblePosition visibleEnd(selectionRange->endPosition(), SEL_DEFAULT_AFFINITY);
    return VisibleSelection(visibleStart, visibleEnd);
}

Node* DOMContainerNodeForPosition(const Position& position)
{
    Node* nodeAtPos = position.containerNode();
    if (nodeAtPos->isInShadowTree())
        nodeAtPos = nodeAtPos->shadowAncestorNode();
    return nodeAtPos;
}

bool isPositionInNode(Node* node, const Position& position)
{
    int offset = 0;
    Node* domNodeAtPos = DOMContainerNodeForPosition(position);
    if (domNodeAtPos == position.containerNode())
        offset = position.computeOffsetInContainerNode();

    RefPtr<Range> rangeForNode = rangeOfContents(node);
    int ec;
    return rangeForNode->isPointInRange(domNodeAtPos, offset, ec);
}

// This is a Tristate return to allow us to override name matching when
// autocomplete is expressly requested for a field. Default indicates
// that the setting is On which is the default but not expressly requested
// for the element being checked. On indicates that it is directly added
// to the element.
AttributeState elementSupportsAutocomplete(const Element* element)
{
    if (element->hasTagName(HTMLNames::inputTag)) {
        const HTMLInputElement* inputElement = static_cast<const HTMLInputElement*>(element);
        if (inputElement->fastHasAttribute(HTMLNames::autocompleteAttr)) {
            if (equalIgnoringCase(inputElement->fastGetAttribute(HTMLNames::autocompleteAttr), "on"))
                return On;
        }
        return inputElement->shouldAutocomplete() ? Default : Off;
    }
    return Default;
}

bool matchesReservedStringPreventingAutocomplete(AtomicString& string)
{
    if (string.contains("email", false /*caseSensitive*/)
        || string.contains("user", false /*caseSensitive*/)
        || string.contains("name", false /*caseSensitive*/)
        || string.contains("login", false /*caseSensitive*/))
        return true;
    return false;
}

// This checks to see if an input element has a name or id attribute set to
// username or email. These are rough checks to avoid major sites that use
// login fields as input type=text and auto correction interfers with.
bool elementIdOrNameIndicatesNoAutocomplete(const Element* element)
{
    if (!element->hasTagName(HTMLNames::inputTag))
        return false;

    AtomicString idAttribute = element->getIdAttribute();
    if (matchesReservedStringPreventingAutocomplete(idAttribute))
        return true;

    if (element->fastHasAttribute(HTMLNames::nameAttr)) {
        AtomicString nameAttribute = element->fastGetAttribute(HTMLNames::nameAttr);
        if (matchesReservedStringPreventingAutocomplete(nameAttribute))
            return true;
    }

    return false;
}

WebCore::IntPoint convertPointToFrame(const WebCore::Frame* sourceFrame, const WebCore::Frame* targetFrame, const WebCore::IntPoint& point)
{
    if (sourceFrame == targetFrame)
        return point;

    ASSERT(sourceFrame && sourceFrame->view());
    ASSERT(targetFrame && targetFrame->view());
    ASSERT(targetFrame->tree());

    WebCore::Frame* targetFrameParent = targetFrame->tree()->parent();
    WebCore::IntRect targetFrameRect = targetFrame->view()->frameRect();

    // Convert the target frame rect to source window content coordinates. This is only required
    // if the parent frame is not the source. If the parent is the source subframeRect
    // is already in source content coordinates.
    if (targetFrameParent != sourceFrame)
        targetFrameRect = sourceFrame->view()->windowToContents(targetFrameParent->view()->contentsToWindow(targetFrameRect));

    // Requested point is outside of target frame, return InvalidPoint.
    if (!targetFrameRect.contains(point))
        return InvalidPoint;

    // Adjust the points to be relative to the target.
    return targetFrame->view()->windowToContents(sourceFrame->view()->contentsToWindow(point));
}

} // DOMSupport
} // WebKit
} // BlackBerry
