/*
 * Copyright (C) 2009, 2010, 2011 Research In Motion Limited. All rights reserved.
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
#include "BackingStoreCompositingSurface.h"

#include "GraphicsContext.h"
#include "SurfacePool.h"

#if USE(ACCELERATED_COMPOSITING)
#if USE(OPENVG)
#include "EGLDisplayOpenVG.h"
#include "EGLUtils.h"
#include "VGUtils.h"
#endif

#include <BlackBerryPlatformGraphics.h>

using namespace BlackBerry::Platform;
using namespace BlackBerry::Platform::Graphics;

namespace BlackBerry {
namespace WebKit {

CompositingSurfaceBuffer::CompositingSurfaceBuffer(const Platform::IntSize& size)
    : m_size(size)
    , m_buffer(0)
{
}

CompositingSurfaceBuffer::~CompositingSurfaceBuffer()
{
    destroyBuffer(m_buffer);
}

Platform::IntSize CompositingSurfaceBuffer::surfaceSize() const
{
    return m_size;
}

Buffer* CompositingSurfaceBuffer::nativeBuffer() const
{
    if (!m_buffer) {
        m_buffer = createBuffer(m_size,
                                BlackBerry::Platform::Graphics::TileBuffer,
                                BlackBerry::Platform::Graphics::GLES2);
    }
    return m_buffer;
}

BackingStoreCompositingSurface::BackingStoreCompositingSurface(const Platform::IntSize& size, bool doubleBuffered)
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
