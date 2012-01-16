/*
 * Copyright (C) 2010, 2011 Research In Motion Limited. All rights reserved.
 */

#ifndef FatFingers_h
#define FatFingers_h

#include <utility>
#include <wtf/Vector.h>

namespace WebCore {
class IntPoint;
class IntRect;
class IntSize;
class Node;
class Element;
class Document;
}

#define DEBUG_FAT_FINGERS 0

namespace Olympia {
namespace WebKit {

class WebPagePrivate;

class FatFingers {
public:
    FatFingers(WebPagePrivate* webpage);
    ~FatFingers();

    enum TargetType { ClickableElement, Text };

    WebCore::IntPoint findBestPoint(const WebCore::IntPoint& contentPos,
                                    TargetType targetType,
                                    WebCore::Node*& nodeUnderFatFinger);

#if DEBUG_FAT_FINGERS
    // These debug vars are all in content coordinates.  They are public so
    // they can be read from BackingStore, which will draw a visible rect
    // around the fat fingers area.
    static WebCore::IntRect m_debugFatFingerRect;
    static WebCore::IntPoint m_debugFatFingerClickPosition;
    static WebCore::IntPoint m_debugFatFingerAdjustedPosition;
#endif

private:
    typedef std::pair<WebCore::Node*, WebCore::IntRect> IntersectingRect;

    bool checkFingerIntersection(const WebCore::IntRect& rect,
                                 const WebCore::IntRect& fingerRect,
                                 const WebCore::IntSize& offset,
                                 WebCore::Node* node,
                                 WTF::Vector<IntersectingRect>& intersectingRects);

    bool findIntersectingRects(const WebCore::IntPoint& contentPos,
                               const WebCore::IntSize& frameOffset,
                               WebCore::Document* document,
                               TargetType targetType,
                               WTF::Vector<IntersectingRect>& intersectingRects,
                               WebCore::Node*& nodeUnderFatFinger);

    bool checkForClickableElement(const WebCore::IntRect& fingerRect,
                                  const WebCore::IntSize& frameOffset,
                                  WebCore::Document* document,
                                  WebCore::Node* node,
                                  WTF::Vector<IntersectingRect>& intersectingRects,
                                  WebCore::Node*& nodeUnderFatFinger);

    bool checkForText(const WebCore::IntRect& fingerRect,
                      const WebCore::IntSize& frameOffset,
                      WebCore::Document* document,
                      WebCore::Node* node,
                      WTF::Vector<IntersectingRect>& intersectingRects,
                      WebCore::Node*& nodeUnderFatFinger);

    WebPagePrivate* m_webPage;
};

}
}

#endif // FatFingers_h

