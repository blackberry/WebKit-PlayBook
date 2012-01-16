/*
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 */

#include "config.h"
#include "BackingStoreCompositingSurface.h"

#include "GraphicsContext.h"
#include "SurfacePool.h"

#if USE(ACCELERATED_COMPOSITING)
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

CompositingSurfaceBuffer::CompositingSurfaceBuffer(const WebCore::IntSize& size)
    : m_size(size)
    , m_buffer(0)
{
}

CompositingSurfaceBuffer::~CompositingSurfaceBuffer()
{
    destroyBuffer(m_buffer);
}

WebCore::IntSize CompositingSurfaceBuffer::surfaceSize() const
{
    return m_size;
}

Buffer* CompositingSurfaceBuffer::nativeBuffer() const
{
    if (!m_buffer) {
        m_buffer = createBuffer(m_size,
                                Olympia::Platform::Graphics::TileBuffer,
                                Olympia::Platform::Graphics::GLES2);
    }
    return m_buffer;
}

BackingStoreCompositingSurface::BackingStoreCompositingSurface(const WebCore::IntSize& size, bool doubleBuffered)
    : m_isDoubleBuffered(doubleBuffered)
    , m_needsSync(true)
{
    m_frontBuffer = reinterpret_cast<unsigned>(new CompositingSurfaceBuffer(size));
    m_backBuffer = !doubleBuffered ? 0 : reinterpret_cast<unsigned>(new CompositingSurfaceBuffer(size));
}

BackingStoreCompositingSurface::~BackingStoreCompositingSurface()
{
    CompositingSurfaceBuffer* front = reinterpret_cast<CompositingSurfaceBuffer*>(m_frontBuffer);
    delete front;
    m_frontBuffer = 0;

    CompositingSurfaceBuffer* back = reinterpret_cast<CompositingSurfaceBuffer*>(m_backBuffer);
    delete back;
    m_backBuffer = 0;
}

CompositingSurfaceBuffer* BackingStoreCompositingSurface::frontBuffer() const
{
    ASSERT(m_frontBuffer);
    return reinterpret_cast<CompositingSurfaceBuffer*>(m_frontBuffer);
}

CompositingSurfaceBuffer* BackingStoreCompositingSurface::backBuffer() const
{
    if (!m_isDoubleBuffered)
        return frontBuffer();

    ASSERT(m_backBuffer);
    return reinterpret_cast<CompositingSurfaceBuffer*>(m_backBuffer);
}

void BackingStoreCompositingSurface::swapBuffers()
{
    if (!m_isDoubleBuffered)
        return;

#if OS(QNX)
    // store temps
    unsigned front = reinterpret_cast<unsigned>(frontBuffer());
    unsigned back = reinterpret_cast<unsigned>(backBuffer());

    // atomic change
    _smp_xchg(&m_frontBuffer, back);
    _smp_xchg(&m_backBuffer, front);
#else
#error "implement me!"
#endif
}

}
}
#endif // USE(ACCELERATED_COMPOSITING)
