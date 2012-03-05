/*
 * Copyright (C) Research In Motion Limited 2009-2011. All rights reserved.
 */

#ifndef InRegionScroller_h
#define InRegionScroller_h

#include <BlackBerryPlatformPrimitives.h>
#include <interaction/ScrollViewBase.h>

namespace WebCore {
class RenderLayer;
}

namespace BlackBerry {
namespace WebKit {

class WebPagePrivate;

class InRegionScroller : public Platform::ScrollViewBase {
public:

    InRegionScroller();
    InRegionScroller(WebPagePrivate*, WebCore::RenderLayer*);

    WebCore::RenderLayer* layer() const;

private:
    Platform::IntPoint calculateMinimumScrollPosition(const Platform::IntSize& viewportSize, float overscrollLimitFactor) const;
    Platform::IntPoint calculateMaximumScrollPosition(const Platform::IntSize& viewportSize, const Platform::IntSize& contentsSize, float overscrollLimitFactor) const;

    WebPagePrivate* m_webPage;
    WebCore::RenderLayer* m_layer;
};

}
}

#endif
