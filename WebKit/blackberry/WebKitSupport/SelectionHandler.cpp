/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 */

#include "config.h"
#include "SelectionHandler.h"

#include "Document.h"
#include "Editor.h"
#include "EditorClient.h"
#include "FatFingers.h"
#include "Frame.h"
#include "FrameView.h"
#include "InputHandler.h"
#include "IntRect.h"
#include "IntRectRegion.h"
#include "Page.h"
#include "RenderPart.h"
#include "TouchEventHandler.h"
#include "WebPage.h"
#include "WebPageClient.h"
#include "WebPage_p.h"
#include "htmlediting.h"

#include "HitTestResult.h"
#include "HTMLAnchorElement.h"
#include "HTMLAreaElement.h"

#define SHOWDEBUG_SELECTIONHANDLER 0

using namespace WebCore;

namespace Olympia {
namespace WebKit {

SelectionHandler::SelectionHandler(WebPagePrivate* page)
    : m_webPage(page)
    , m_selectionActive(false)
{
}

SelectionHandler::~SelectionHandler()
{
}

void SelectionHandler::cancelSelection()
{
    m_selectionActive = false;

#if SHOWDEBUG_SELECTIONHANDLER
    Olympia::Platform::log(Olympia::Platform::LogLevelInfo, "SelectionHandler::cancelSelection");
#endif
    if (m_webPage->m_inputHandler->isInputMode())
        m_webPage->m_inputHandler->cancelSelection();
    else
        m_webPage->focusedOrMainFrame()->selection()->clear();
}

WebString SelectionHandler::selectedText() const
{
    return m_webPage->focusedOrMainFrame()->editor()->selectedText();
}

bool SelectionHandler::findNextString(const WTF::String& searchString, bool forward)
{
    if (searchString.isEmpty()) {
        cancelSelection();
        return false;
    }

    ASSERT(m_webPage->m_page);

    m_webPage->m_page->unmarkAllTextMatches();

    bool result = m_webPage->m_page->findString(searchString, WTF::TextCaseInsensitive, forward ? WebCore::FindDirectionForward : WebCore::FindDirectionBackward, true /*should wrap*/);
    if (result && m_webPage->focusedOrMainFrame()->selection()->selectionType() == VisibleSelection::NoSelection) {
        // Word was found but could not be selected on this page.
        result = m_webPage->m_page->markAllMatchesForText(searchString, WTF::TextCaseInsensitive, true /*should highlight*/, 0 /*limit to match 0 = unlimited*/);
    }

    // Defocus the input field if one is active.
    if (m_webPage->m_inputHandler->isInputMode())
        m_webPage->m_inputHandler->nodeFocused(0);

    return result;
}

bool static selectionIsContainedByAnchorNode(const VisibleSelection& selection)
{
    // Check whether the start or end of the selection is outside of the selections
    // anchor node.
    return (selection.start().anchorType() == WebCore::Position::PositionIsOffsetInAnchor
            && selection.end().anchorType() == WebCore::Position::PositionIsOffsetInAnchor);
}

void SelectionHandler::getConsolidatedRegionOfTextQuadsForSelection(const VisibleSelection& selection, IntRectRegion& region) const
{
    ASSERT(region.isEmpty());

    if (!selection.isRange())
        return;

    ASSERT(selection.firstRange());

    FrameView* frameView = m_webPage->focusedOrMainFrame()->view();

    // frame Rect is in frame coordinates.
    IntRect frameRect(IntPoint(0, 0), frameView->contentsSize());

    // The framePos is in main frame coordinates.
    IntPoint framePosition(m_webPage->mainFrame()->view()->windowToContents(frameView->contentsToWindow(IntPoint::zero())));

    Vector<FloatQuad> quadList;
    selection.firstRange().get()->textQuads(quadList, true /*use selection height*/);

    if (!quadList.isEmpty()) {
        // The ranges rect list is based on render elements and may include multiple adjacent rects.
        // Use IntRectRegion to consolidate these rects into bands as well as a container to pass
        // to the client.

        for (unsigned i = 0; i < quadList.size(); i++) {
            IntRect enclosingRect = quadList[i].enclosingBoundingBox();
            enclosingRect.intersect(frameRect);
            enclosingRect.move(framePosition.x(), framePosition.y());
            region = unionRegions(region, enclosingRect);
        }
    }
}

void SelectionHandler::setSelection(WebCore::IntPoint start, WebCore::IntPoint end)
{
    m_selectionActive = true;

    ASSERT(m_webPage);
    ASSERT(m_webPage->focusedOrMainFrame());
    ASSERT(m_webPage->focusedOrMainFrame()->selection());
    ASSERT(m_webPage->mainFrame() && m_webPage->mainFrame()->view());

    Frame* focusedFrame = m_webPage->focusedOrMainFrame();
    FrameView* mainFrameView = m_webPage->mainFrame()->view();
    SelectionController* controller = focusedFrame->selection();

#if SHOWDEBUG_SELECTIONHANDLER
    Olympia::Platform::log(Olympia::Platform::LogLevelInfo, "SelectionHandler::setSelection adjusted points %d, %d, %d, %d", start.x(), start.y(), end.x(), end.y());
#endif

    IntPoint relativeStart(start);
    IntPoint relativeEnd(end);

    // Note that IntPoint(-1, -1) is being our sentinel so far for
    // clipped out selection starting or ending location.
    bool startIsValid = start != WebCore::IntPoint(-1, -1);
    bool endIsValid = end != WebCore::IntPoint(-1, -1);

    // At least one of the locations must be valid.
    ASSERT(startIsValid || endIsValid);

    // Validate the new points to avoid crossing frame or editing boundaries.
    if (m_webPage->mainFrame() != focusedFrame) {
        // Current focus frame is not the main frame, it must be a subframe.
        WebCore::IntRect subframeRect = focusedFrame->view()->frameRect();

        ASSERT(focusedFrame->tree());
        ASSERT(focusedFrame->tree()->parent());
        ASSERT(focusedFrame->tree()->parent()->view());

        // Convert the frame rect to main window content coordinates.  This is only required
        // if the parent frame is not the mainframe.  If the parent is the mainframe subframeRect
        // is already in main window content coordinates.
        if (focusedFrame->tree()->parent() != m_webPage->mainFrame())
            subframeRect = mainFrameView->windowToContents(focusedFrame->tree()->parent()->view()->contentsToWindow(subframeRect));

        if ((!subframeRect.contains(start) && startIsValid)
            || (!subframeRect.contains(end) && endIsValid)) {
            // Requested selection points are outside of current frame.
            selectionPositionChanged();
            return;
        }

        // Adjust the points to be relative to the subframe not the main frame.
        relativeStart = startIsValid ?
                        focusedFrame->view()->windowToContents(mainFrameView->contentsToWindow(start)) :
                        focusedFrame->selection()->selection().visibleStart().absoluteCaretBounds().location();

        relativeEnd = endIsValid ?
                      focusedFrame->view()->windowToContents(mainFrameView->contentsToWindow(end)) :
                      focusedFrame->selection()->selection().visibleEnd().absoluteCaretBounds().location();

    }

    VisiblePosition visibleStart (focusedFrame->visiblePositionForPoint(relativeStart));
    VisiblePosition visibleEnd (focusedFrame->visiblePositionForPoint(relativeEnd));

    if (m_webPage->m_inputHandler->isInputMode()) {

        // NOTE:  This only works if the end/start handles are near the edge of selection, otherwise the
        // selection is actually different than the requested values.  This can't be fixed easily though
        // because we would need to cache the last request from the client to know the client hasn't
        // requested the selection change, but we can't determine when a non-user selection event has
        // triggered the change to clear those values.
        // An API split to setStart/setEnd may be required to properly fix this.

        // The following situations can happen:
        //   (1) The start selection point is clipped out (ie at -1, -1, invalid)
        //   (2) The end selection point is clipped out (ie at -1, -1, invalid)
        // (3-4) If both are valid, then only we only change one point at a time.
        if (!startIsValid) // #1
            visibleStart = VisiblePosition(controller->selection().start());
        else if (!endIsValid) // #2
            visibleEnd = VisiblePosition(controller->selection().end());
        else if (visibleStart != controller->selection().start()) // #3
            visibleEnd = controller->selection().end();
        else if (visibleEnd != controller->selection().end()) // #4
            visibleStart = controller->selection().start();
    }

    VisibleSelection newSelection(visibleStart, visibleEnd);
    if (controller->selection() == newSelection) {
        selectionPositionChanged();
        return;
    }

    if (!m_webPage->m_inputHandler->isInputMode() && (isEditablePosition(newSelection.start()) || isEditablePosition(newSelection.end()))) {
        // One of the points is in an input field but we are not in input mode.  Ignore this selection change to require the entire
        // field to be selected and not enter input mode.
        selectionPositionChanged();
        return;
    }

    // Check whether selection is occurring inside of an input field.  Do not handle as
    // an else if, an input field may be active inside of a subframe.
    if (m_webPage->m_inputHandler->isInputMode() && !selectionIsContainedByAnchorNode(newSelection)) {

        // Padding to prevent accidental trigger of up/down when intending to do horizontal movement.
        const int verticalPadding = 20;

        bool selectionWasChanged = false;
        if (newSelection.start() != controller->selection().start()) {
            // Start isn't equal, treat it as changed value.
            // Use the current selection to determine which way we should attempt to move
            // the selection.
            WebCore::IntRect currentStart(focusedFrame->selection()->selection().visibleStart().absoluteCaretBounds());
            WebCore::IntRect newStart(newSelection.visibleStart().absoluteCaretBounds());

            // Invert the selection so that the cursor point is at the beginning.
            controller->setSelection(VisibleSelection(controller->selection().end(), controller->selection().start()));

            // Do height movement check first but add padding.  We may be off on both x & y axis and only
            // want to scroll in one direction at a time.
            if (newStart.location().y() + verticalPadding < currentStart.topLeft().y()) {
                // Move up
                selectionWasChanged = controller->modify(SelectionController::AlterationExtend, SelectionController::DirectionBackward, LineGranularity, true);
            } else if (newStart.location().y() > currentStart.bottomLeft().y() + verticalPadding) {
                // move down
                selectionWasChanged = controller->modify(SelectionController::AlterationExtend, SelectionController::DirectionForward, LineGranularity, true);
            } else if (newStart.location().x() < currentStart.location().x()) {
                // move left
                selectionWasChanged = controller->modify(SelectionController::AlterationExtend, SelectionController::DirectionLeft, CharacterGranularity, true);
            }
        } else if (newSelection.end() != controller->selection().end()) {
            // End isn't equal, treat it as changed value.
            // Use the current selection to determine which way we should attempt to move
            // the selection.
            WebCore::IntRect currentEnd(focusedFrame->selection()->selection().visibleEnd().absoluteCaretBounds());
            WebCore::IntRect newEnd(newSelection.visibleEnd().absoluteCaretBounds());

            // Reset the selection so that the end is the edit point.
            controller->setSelection(VisibleSelection(controller->selection().start(), controller->selection().end()));

            // Do height movement check first but add padding.  We may be off on both x & y axis and only
            // want to scroll in one direction at a time.
            if (newEnd.location().y() + verticalPadding < currentEnd.topLeft().y()) {
                // Move up
                selectionWasChanged = controller->modify(SelectionController::AlterationExtend, SelectionController::DirectionBackward, LineGranularity, true);
            } else if (newEnd.location().y() > currentEnd.bottomLeft().y() + verticalPadding) {
                // move down
                selectionWasChanged = controller->modify(SelectionController::AlterationExtend, SelectionController::DirectionForward, LineGranularity, true);
            } else if (newEnd.location().x() > currentEnd.location().x()) {
                // move right
                selectionWasChanged = controller->modify(SelectionController::AlterationExtend, SelectionController::DirectionRight, CharacterGranularity, true);
            }
        }
        // Already handled, or requested selection points are not contained within the anchor node (input field).
        if (!selectionWasChanged)
            selectionPositionChanged();
        return;
    }

    // If the selection size is reduce to less than a character, selection type becomes
    // Caret.  As long as it is still a range, it's a valid selection.  Selection cannot
    // be cancelled through this function.
    IntRectRegion region;
    getConsolidatedRegionOfTextQuadsForSelection(newSelection, region);
    if (!region.isEmpty()) {
        // Check if the handles reversed position.
        if (startIsValid && endIsValid && !newSelection.isBaseFirst())
            m_webPage->m_client->notifySelectionHandlesReversed();

        controller->setSelection(newSelection);

#if SHOWDEBUG_SELECTIONHANDLER
        Olympia::Platform::log(Olympia::Platform::LogLevelInfo, "SelectionHandler::setSelection selection points valid, selection updated");
#endif
    } else {
        // Requested selection results in an empty selection, skip this change.
        selectionPositionChanged();

#if SHOWDEBUG_SELECTIONHANDLER
        Olympia::Platform::log(Olympia::Platform::LogLevelInfo, "SelectionHandler::setSelection selection points invalid, selection not updated");
#endif
    }
}

// TODO re-use this in context.  Must be updated to include an option to return the href.
// This function should be moved to a new unit file.  Names suggetions include DOMQueries
// and NodeTypes.  Functions currently in InputHandler.cpp, SelectionHandler.cpp and WebPage.cpp
// can all be moved in.
static bool nodeIsLink(WebCore::Node* node)
{
    if (!node)
        return false;

    Node* linkNode = node->enclosingLinkEventParentOrSelf();

    return (linkNode && linkNode->isLink());
}

void SelectionHandler::selectAtPoint(WebCore::IntPoint& location)
{
    // If point is invalid trigger selection based expansion.
    static WebCore::IntPoint invalidPoint(-1, -1);
    if (location == invalidPoint) {
        selectObject(WordGranularity);
        return;
    }

    WebCore::Node* nodeUnderFatFinger;
    WebCore::IntPoint fatFingerLocation = FatFingers(m_webPage).findBestPoint(location, FatFingers::Text, nodeUnderFatFinger);

    if (!nodeUnderFatFinger)
        return;

    // If the node at the point is a link, focus on the entire link, not a word.
    if (nodeIsLink(nodeUnderFatFinger)) {
        selectObject(nodeUnderFatFinger);
        return;
    }

    // selectAtPoint API currently only supports WordGranularity but may be extended in the future.
    selectObject(fatFingerLocation, WordGranularity);
}

static bool expandSelectionToGranularity(Frame* frame, VisibleSelection selection, TextGranularity granularity, bool isInputMode)
{
    ASSERT(frame);
    ASSERT(frame->selection());

    if (!(selection.start().anchorNode() && selection.start().anchorNode()->isTextNode()))
        return false;

    selection.expandUsingGranularity(granularity);
    RefPtr<Range> newRange = selection.toNormalizedRange();
    RefPtr<Range> oldRange = frame->selection()->selection().toNormalizedRange();
    EAffinity affinity = frame->selection()->affinity();

    if (isInputMode && !frame->editor()->client()->shouldChangeSelectedRange(oldRange.get(), newRange.get(), affinity, false))
        return false;

    return frame->selection()->setSelectedRange(newRange.get(), affinity, true);
}

void SelectionHandler::selectObject(WebCore::IntPoint& location, TextGranularity granularity)
{
    ASSERT(location.x() >= 0 && location.y() >= 0);
    ASSERT(m_webPage && m_webPage->mainFrame() && m_webPage->mainFrame()->selection());
    Frame* frame = m_webPage->mainFrame();

#if SHOWDEBUG_SELECTIONHANDLER
    Olympia::Platform::log(Olympia::Platform::LogLevelInfo, "SelectionHandler::selectObject adjusted points %d, %d", location.x(), location.y());
#endif

    VisiblePosition pointLocation(frame->visiblePositionForPoint(location));
    VisibleSelection selection = VisibleSelection(pointLocation, pointLocation);

    m_selectionActive = expandSelectionToGranularity(frame, selection, granularity, m_webPage->m_inputHandler->isInputMode());
}

void SelectionHandler::selectObject(WebCore::TextGranularity granularity)
{
    // Using caret location, must be inside an input field.
    if (!m_webPage->m_inputHandler->isInputMode())
        return;

    ASSERT(m_webPage && m_webPage->mainFrame() && m_webPage->mainFrame()->selection());
    Frame* frame = m_webPage->mainFrame();

#if SHOWDEBUG_SELECTIONHANDLER
    Olympia::Platform::log(Olympia::Platform::LogLevelInfo, "SelectionHandler::selectObject using current selection");
#endif

    // Use the current selection as the selection point.
    ASSERT(frame->selection()->selectionType() != VisibleSelection::NoSelection);
    m_selectionActive = expandSelectionToGranularity(m_webPage->focusedOrMainFrame(), frame->selection()->selection(), granularity, true /* isInputMode */);
}

void SelectionHandler::selectObject(WebCore::Node* node)
{
    if (!node)
        return;

    m_selectionActive = true;

    ASSERT(m_webPage && m_webPage->mainFrame() && m_webPage->mainFrame()->selection());
    Frame* frame = m_webPage->mainFrame();

#if SHOWDEBUG_SELECTIONHANDLER
    Olympia::Platform::log(Olympia::Platform::LogLevelInfo, "SelectionHandler::selectNode");
#endif

    VisibleSelection selection = VisibleSelection::selectionFromContentsOfNode(node);
    frame->selection()->setSelection(selection);
}

// Gets the distance between two points, but tries to exit earlier if it will be larger than
// the provided threshold value.  Omit taking the square root as we are just interested in a
// relative distance and only compare to the result of this function.
static inline int distanceBetweenPointsIfBelowThreshold(const WebCore::IntPoint& p1, const WebCore::IntPoint& p2, const int threshold)
{
    int dx = p1.x() - p2.x();
    int dy = p1.y() - p2.y();
    if (threshold > 0 && (dx > threshold || dy > threshold))
        return threshold + 1;
    return (dx * dx) + (dy * dy);
}

static void adjustCaretRects(WebCore::IntRect& startCaret, bool isStartCaretClippedOut,
                             WebCore::IntRect& endCaret, bool isEndCaretClippedOut,
                             const Vector<IntRect> rectList, const WebCore::IntPoint& startReferencePoint,
                             const WebCore::IntPoint& endReferencePoint)
{
    if (isStartCaretClippedOut)
        startCaret.setLocation(WebCore::IntPoint(-1, -1));
    else
        startCaret = rectList[0];

    if (isEndCaretClippedOut)
        endCaret.setLocation(WebCore::IntPoint(-1, -1));
    else {
        endCaret = rectList[0];
        endCaret.setLocation(endCaret.topRight());
    }

    if (isStartCaretClippedOut && isEndCaretClippedOut)
        return;

    // Reset width to 1 as we are strictly interested in caret location.
    startCaret.setWidth(1);
    endCaret.setWidth(1);

    int offsetToStartReferencePoint = distanceBetweenPointsIfBelowThreshold(startCaret.location(), startReferencePoint, -1);
    int offsetToEndReferencePoint = distanceBetweenPointsIfBelowThreshold(endCaret.location(), endReferencePoint, -1);

    for (unsigned i = 1; i < rectList.size(); i++) {
        IntRect currentRect(rectList[i]);
        // Compare start and end and update.

        if (!isStartCaretClippedOut) {
            int offset = distanceBetweenPointsIfBelowThreshold(currentRect.location(), startReferencePoint, offsetToStartReferencePoint);
            if (offset < offsetToStartReferencePoint) {
                startCaret.setLocation(currentRect.location());
                startCaret.setHeight(currentRect.height());
                offsetToStartReferencePoint = offset;
            }
        }

        if (!isEndCaretClippedOut) {
            int offset = distanceBetweenPointsIfBelowThreshold(currentRect.topRight(), endReferencePoint, offsetToEndReferencePoint);
            if (offset < offsetToEndReferencePoint) {
                endCaret.setLocation(currentRect.topRight());
                endCaret.setHeight(currentRect.height());
                offsetToEndReferencePoint = offset;
            }
        }
    }
}

// Note:  This is the only function in SelectionHandler in which the coordinate
// system is not entirely WebKit.
void SelectionHandler::selectionPositionChanged(bool invalidateSelectionPoints)
{
    // Enter selection mode if selection type is RangeSelection, and disable selection if
    // selection is active and becomes caret selection.  None shouldn't disable as it is
    // a temporary state when crossing handles.
    Frame* frame = m_webPage->focusedOrMainFrame();
    IntPoint framePos(m_webPage->mainFrame()->view()->windowToContents(frame->view()->contentsToWindow(IntPoint::zero())));
    if (m_selectionActive && frame->selection()->selectionType() == VisibleSelection::CaretSelection)
        m_selectionActive = false;
    else if (frame->selection()->selectionType() == VisibleSelection::RangeSelection)
        m_selectionActive = true;
    else if (!m_selectionActive)
        return;

    WebCore::IntRect startCaret;
    WebCore::IntRect endCaret;

    WebCore::IntRect unclippedStartCaret;
    WebCore::IntRect unclippedEndCaret;

    // Get the reference points for start and end caret positions.
    WebCore::IntPoint startCaretReferencePoint(frame->selection()->selection().visibleStart().absoluteCaretBounds().location());
    startCaretReferencePoint.move(framePos.x(), framePos.y());
    WebCore::IntPoint endCaretReferencePoint(frame->selection()->selection().visibleEnd().absoluteCaretBounds().location());
    endCaretReferencePoint.move(framePos.x(), framePos.y());

    // Get the text rects from the selections range.
    IntRectRegion region;
    getConsolidatedRegionOfTextQuadsForSelection(frame->selection()->selection(), region);
    if (!region.isEmpty()) {

        Vector<IntRect> rectList = region.rects();
        adjustCaretRects(unclippedStartCaret, false /*unclipped*/, unclippedEndCaret, false /*unclipped*/, rectList, startCaretReferencePoint, endCaretReferencePoint);

        WebCore::IntRect containingContentRect;
        // Don't allow the region to extend outside of the all its ancestor frames' visible area.
        if (frame != m_webPage->mainFrame()) {
            containingContentRect = m_webPage->getRecursiveVisibleWindowRect(frame->view(), true /*no clip to main frame window*/);
            containingContentRect = m_webPage->m_mainFrame->view()->windowToContents(containingContentRect);
            region = intersectRegions(containingContentRect, region);
        }

        // Don't allow the region to extend outside of the input field.
        if (m_webPage->m_inputHandler->isInputMode()
            && frame->document()->focusedNode()
            && frame->document()->focusedNode()->renderer()) {

            // Adjust the bounding box to the frame offset.
            IntRect boundingBox(frame->document()->focusedNode()->renderer()->absoluteBoundingBoxRect());
            boundingBox.move(containingContentRect.location().x(), containingContentRect.location().y());

            region = intersectRegions(boundingBox, region);
        }

#if SHOWDEBUG_SELECTIONHANDLER
        for (unsigned int i = 0; i < rectList.size(); i++)
            Olympia::Platform::log(Olympia::Platform::LogLevelInfo, "Rect list - Unmodified #%d, (%d, %d) (%d x %d)", i, rectList[i].x(), rectList[i].y(), rectList[i].width(), rectList[i].height());
        for (unsigned int i = 0; i < region.numRects(); i++)
            Olympia::Platform::log(Olympia::Platform::LogLevelInfo, "Rect list  - Consolidated #%d, (%d, %d) (%d x %d)", i, region.rects()[i].x(), region.rects()[i].y(), region.rects()[i].width(), region.rects()[i].height());
#endif

        bool shouldCareAboutPossibleClippedOutSelection = frame != m_webPage->mainFrame() || m_webPage->m_inputHandler->isInputMode();

        if (!region.isEmpty() || shouldCareAboutPossibleClippedOutSelection) {
            // Adjust the handle markers to be at the end of the painted rect.  When selecting links
            // and other elements that may have a larger visible area than needs to be rendered a gap
            // can exist between the handle and overlay region.

            bool shouldClipStartCaret = !region.isRectInRegion(unclippedStartCaret);
            bool shouldClipEndCaret = !region.isRectInRegion(unclippedEndCaret);

            // Find the top corner and bottom corner.
            Vector<IntRect> clippedRectList = region.rects();
            adjustCaretRects(startCaret, shouldClipStartCaret, endCaret, shouldClipEndCaret, clippedRectList, startCaretReferencePoint, endCaretReferencePoint);

            // Translate the caret values as they must be in transformed coordinates.
            if (!shouldClipStartCaret) {
                startCaret = m_webPage->mapToTransformed(startCaret);
                m_webPage->clipToTransformedContentsRect(startCaret);
            }

            if (!shouldClipEndCaret) {
                endCaret = m_webPage->mapToTransformed(endCaret);
                m_webPage->clipToTransformedContentsRect(endCaret);
            }
        }
    }

#if SHOWDEBUG_SELECTIONHANDLER
        Olympia::Platform::log(Olympia::Platform::LogLevelInfo, "SelectionHandler::selectionPositionChanged Start Rect=%s End Rect=%s",
                                       startCaret.toString().utf8().data(), endCaret.toString().utf8().data());
#endif

    m_webPage->m_client->notifySelectionDetailsChanged(startCaret, endCaret, region, invalidateSelectionPoints);
}

bool SelectionHandler::selectionContains(const WebCore::IntPoint& point)
{
    ASSERT(m_webPage && m_webPage->focusedOrMainFrame() && m_webPage->focusedOrMainFrame()->selection());
    return m_webPage->focusedOrMainFrame()->selection()->contains(point);
}

}
}
