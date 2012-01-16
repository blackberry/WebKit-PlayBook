/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 */

#ifndef SelectionHandler_h
#define SelectionHandler_h

#include "BlackBerryPlatformPrimitives.h"
#include "TextGranularity.h"

namespace WTF {
class String;
}

namespace WebCore {
class IntPoint;
class IntRect;
class IntRectRegion;
class Node;
class VisibleSelection;
}

namespace Olympia {
namespace WebKit {

class WebPagePrivate;
class WebString;

class SelectionHandler {
public:
    SelectionHandler(WebPagePrivate*);
    ~SelectionHandler();

    bool isSelectionActive() { return m_selectionActive; }
    void setSelectionActive(bool active) { m_selectionActive = active; }

    void cancelSelection();
    WebString selectedText() const;

    bool findNextString(const WTF::String&, bool);
    bool selectionContains(const WebCore::IntPoint&);

    void setSelection(WebCore::IntPoint start, WebCore::IntPoint end);
    void selectAtPoint(WebCore::IntPoint&);
    void selectObject(WebCore::IntPoint&, WebCore::TextGranularity);
    void selectObject(WebCore::TextGranularity);
    void selectObject(WebCore::Node*);

    void selectionPositionChanged(bool invalidateSelectionPoints = false);

private:
    void getConsolidatedRegionOfTextQuadsForSelection(const WebCore::VisibleSelection&, WebCore::IntRectRegion&) const;

    WebPagePrivate* m_webPage;

    bool m_selectionActive;
};

}
}

#endif // SelectionHandler_h
