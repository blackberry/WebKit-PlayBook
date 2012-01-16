/*
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 */

#include "config.h"
#include "BackingStoreTile.h"

#include "GraphicsContext.h"
#include "SurfacePool.h"

#if PLATFORM(OPENVG)
#include "EGLDisplayOpenVG.h"
#include "EGLUtils.h"
#include "VGUtils.h"
#endif

#include <BlackBerryPlatformGraphics.h>
#include <BlackBerryPlatformPrimitives.h>

using namespace Olympia::Platform;
using namespace Olympia::Platform::Graphics;

namespace Olympia {
namespace WebKit {

TileBuffer::TileBuffer(const WebCore::IntSize& size)
  : m_size(size)
  , m_buffer(0)
{
}

TileBuffer::~TileBuffer()
{
    destroyBuffer(m_buffer);
}

WebCore::IntSize TileBuffer::size() const
{
    return m_size;
}

WebCore::IntRect TileBuffer::rect() const
{
    return WebCore::IntRect(WebCore::IntPoint(0,0), m_size);
}

bool TileBuffer::isRendered() const
{
    return isRendered(rect());
}

bool TileBuffer::isRendered(const WebCore::IntRectRegion& contents) const
{
    return WebCore::IntRectRegion::subtractRegions(contents, m_renderedRegion).isEmpty();
}

void TileBuffer::clearRenderedRegion(const WebCore::IntRectRegion& region)
{
    m_renderedRegion = WebCore::IntRectRegion::subtractRegions(m_renderedRegion, region);
}

void TileBuffer::clearRenderedRegion()
{
    m_renderedRegion = WebCore::IntRectRegion();
}

void TileBuffer::addRenderedRegion(const WebCore::IntRectRegion& region)
{
    m_renderedRegion = WebCore::IntRectRegion::unionRegions(region, m_renderedRegion);
}

WebCore::IntRectRegion TileBuffer::renderedRegion() const
{
    return m_renderedRegion;
}

WebCore::IntRectRegion TileBuffer::notRenderedRegion() const
{
    return WebCore::IntRectRegion::subtractRegions(rect(), renderedRegion());
}

Buffer* TileBuffer::nativeBuffer() const
{
    if (!m_buffer)
        m_buffer = createBuffer(m_size, Olympia::Platform::Graphics::TileBuffer);

    return m_buffer;
}


BackingStoreTile::BackingStoreTile(const WebCore::IntSize& size, BufferingMode mode)
    : m_bufferingMode(mode)
    , m_committed(false)
    , m_backgroundPainted(false)
    , m_horizontalShift(0)
    , m_verticalShift(0)
{
    m_frontBuffer = reinterpret_cast<unsigned>(new TileBuffer(size));
}

BackingStoreTile::~BackingStoreTile()
{
    TileBuffer* front = reinterpret_cast<TileBuffer*>(m_frontBuffer);
    delete front;
    m_frontBuffer = 0;
}

WebCore::IntSize BackingStoreTile::size() const
{
    return frontBuffer()->size();
}

WebCore::IntRect BackingStoreTile::rect() const
{
    return frontBuffer()->rect();
}

TileBuffer* BackingStoreTile::frontBuffer() const
{
    ASSERT(m_frontBuffer);
    return reinterpret_cast<TileBuffer*>(m_frontBuffer);
}

TileBuffer* BackingStoreTile::backBuffer() const
{
    if (m_bufferingMode == SingleBuffered)
        return frontBuffer();

    return SurfacePool::globalSurfacePool()->backBuffer();
}

void BackingStoreTile::swapBuffers()
{
    if (m_bufferingMode == SingleBuffered)
        return;

#if OS(QNX)
    // store temps
    unsigned front = reinterpret_cast<unsigned>(frontBuffer());
    unsigned back = reinterpret_cast<unsigned>(backBuffer());

    // atomic change
    _smp_xchg(&m_frontBuffer, back);
    _smp_xchg(&SurfacePool::globalSurfacePool()->m_backBuffer, front);
#else
#error "implement me!"
#endif
}

void BackingStoreTile::reset()
{
    setCommitted(false);
    frontBuffer()->clearRenderedRegion();
    backBuffer()->clearRenderedRegion();
    clearShift();
}

void BackingStoreTile::paintBackground()
{
    m_backgroundPainted = true;

    clearBufferWhite(backBuffer()->nativeBuffer());
}

void BackingStoreTile::didCommitSwapGroup()
{
    backBuffer()->clearRenderedRegion();
}

}
}
