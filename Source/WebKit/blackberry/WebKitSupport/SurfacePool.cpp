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
#include "SurfacePool.h"

#if USE(OPENVG)
#include "EGLDisplayOpenVG.h"
#include "EGLUtils.h"
#include "SurfaceOpenVG.h"
#elif USE(SKIA)
#include "PlatformContextSkia.h"
#endif

#if USE(ACCELERATED_COMPOSITING)
#include "BackingStoreCompositingSurface.h"
#endif

#include <BlackBerryPlatformGraphics.h>
#include <BlackBerryPlatformLog.h>
#include <BlackBerryPlatformMisc.h>
#include <BlackBerryPlatformScreen.h>
#include <BlackBerryPlatformSettings.h>

#define SHARED_PIXMAP_GROUP "webkit_backingstore_group"

namespace BlackBerry {
namespace WebKit {

#if USE(ACCELERATED_COMPOSITING) && ENABLE_COMPOSITING_SURFACE
static PassRefPtr<BackingStoreCompositingSurface> createCompositingSurface()
{
    BlackBerry::Platform::IntSize screenSize = BlackBerry::Platform::Graphics::Screen::size();
    return BackingStoreCompositingSurface::create(screenSize, false /*doubleBuffered*/);
}
#endif

SurfacePool* SurfacePool::globalSurfacePool()
{
    static SurfacePool* s_instance = 0;
    if (!s_instance)
        s_instance = new SurfacePool;
    return s_instance;
}

SurfacePool::SurfacePool()
    : m_visibleTileBuffer(0)
#if USE(ACCELERATED_COMPOSITING)
    , m_compositingSurface(0)
#endif
    , m_tileRenderingSurface(0)
    , m_backBuffer(0)
    , m_initialized(false)
    , m_buffersSuspended(false)
{
}

void SurfacePool::initialize(const BlackBerry::Platform::IntSize& tileSize)
{
    if (m_initialized)
        return;
    m_initialized = true;

    const unsigned numberOfTiles = BlackBerry::Platform::Settings::get()->numberOfBackingStoreTiles();
    const unsigned maxNumberOfTiles = BlackBerry::Platform::Settings::get()->maximumNumberOfBackingStoreTilesAcrossProcesses();

    if (numberOfTiles) { // only allocate if we actually use a backingstore
        unsigned byteLimit = (maxNumberOfTiles /*pool*/ + 2 /*visible tile buffer, backbuffer*/) * tileSize.width() * tileSize.height() * 4;
        bool success = BlackBerry::Platform::Graphics::createPixmapGroup(SHARED_PIXMAP_GROUP, byteLimit);
        if (!success) {
            BlackBerry::Platform::log(BlackBerry::Platform::LogLevelWarn,
                "Shared buffer pool could not be set up, using regular memory allocation instead.");
        }
    }

#if USE(OPENVG)
    m_tileRenderingSurface = createPlatformGraphicsContext(BlackBerry::Platform::Graphics::drawingSurface());
#elif USE(SKIA)
    m_tileRenderingSurface = BlackBerry::Platform::Graphics::drawingSurface();
#endif

#if USE(ACCELERATED_COMPOSITING) && ENABLE_COMPOSITING_SURFACE
    m_compositingSurface = createCompositingSurface();
#endif

    if (!numberOfTiles)
        return; // we only use direct rendering when 0 tiles are specified

    // Create the shared backbuffer
    m_backBuffer = reinterpret_cast<unsigned>(new TileBuffer(tileSize));

    for (size_t i = 0; i < numberOfTiles; ++i)
        m_tilePool.append(BackingStoreTile::create(tileSize, BackingStoreTile::DoubleBuffered));
}

PlatformGraphicsContext* SurfacePool::createPlatformGraphicsContext(BlackBerry::Platform::Graphics::Drawable* drawable) const
{
#if USE(OPENVG)
    SurfaceOpenVG* platformGraphicsContext = SurfaceOpenVG::adoptExistingSurface(
        BlackBerry::Platform::Graphics::eglDisplay(), drawable->surface(),
        SurfaceOpenVG::DontTakeSurfaceOwnership, drawable->surfaceType());

    platformGraphicsContext->setApplyFlipTransformationOnPainterCreation(drawable->isFlipTransformationRequired());
#elif USE(SKIA)
    WebCore::PlatformContextSkia* platformGraphicsContext = new WebCore::PlatformContextSkia(drawable);
#endif
    return platformGraphicsContext;
}

PlatformGraphicsContext* SurfacePool::lockTileRenderingSurface() const
{
#if USE(OPENVG)
    return m_tileRenderingSurface;
#elif USE(SKIA)
    if (!m_tileRenderingSurface)
        return 0;
    return createPlatformGraphicsContext(BlackBerry::Platform::Graphics::lockBufferDrawable(m_tileRenderingSurface));
#endif
}

void SurfacePool::releaseTileRenderingSurface(PlatformGraphicsContext* context) const
{
#if USE(SKIA)
    if (!m_tileRenderingSurface)
        return;

    delete context;
    BlackBerry::Platform::Graphics::releaseBufferDrawable(m_tileRenderingSurface);
#endif
}

void SurfacePool::initializeVisibleTileBuffer(const BlackBerry::Platform::IntSize& visibleSize)
{
    if (!m_visibleTileBuffer || m_visibleTileBuffer->size() != visibleSize) {
        delete m_visibleTileBuffer;
        m_visibleTileBuffer = BackingStoreTile::create(visibleSize, BackingStoreTile::SingleBuffered);
    }
}

TileBuffer* SurfacePool::backBuffer() const
{
    ASSERT(m_backBuffer);
    return reinterpret_cast<TileBuffer*>(m_backBuffer);
}

#if USE(ACCELERATED_COMPOSITING)
BackingStoreCompositingSurface* SurfacePool::compositingSurface() const
{
    return m_compositingSurface.get();
}
#endif

void SurfacePool::notifyScreenRotated()
{
#if USE(ACCELERATED_COMPOSITING) && ENABLE_COMPOSITING_SURFACE
    // Recreate compositing surface at new screen resolution
    m_compositingSurface = createCompositingSurface();
#endif
}

std::string SurfacePool::sharedPixmapGroup() const
{
    return SHARED_PIXMAP_GROUP;
}

void SurfacePool::createBuffers()
{
    if (!m_initialized || m_tilePool.isEmpty() || !m_buffersSuspended)
        return;

    // Create the tile pool
    for (size_t i = 0; i < m_tilePool.size(); ++i)
        BlackBerry::Platform::Graphics::createPixmapBuffer(m_tilePool[i]->frontBuffer()->nativeBuffer());

    // Create the m_visibleTileBuffer
    if (m_visibleTileBuffer)
        BlackBerry::Platform::Graphics::createPixmapBuffer(m_visibleTileBuffer->frontBuffer()->nativeBuffer());

    // Create the backbuffer
    if (backBuffer())
        BlackBerry::Platform::Graphics::createPixmapBuffer(backBuffer()->nativeBuffer());

    m_buffersSuspended = false;
}

void SurfacePool::releaseBuffers()
{
    if (!m_initialized || m_tilePool.isEmpty() || m_buffersSuspended)
        return;

    m_buffersSuspended = true;

    // Release the tile pool
    for (size_t i = 0; i < m_tilePool.size(); ++i) {
        m_tilePool[i]->frontBuffer()->clearRenderedRegion();
        // Clear the buffer to prevent accidental leakage of (possibly sensitive) pixel data.
        BlackBerry::Platform::Graphics::clearBuffer(m_tilePool[i]->frontBuffer()->nativeBuffer(), 0, 0, 0, 0);
        BlackBerry::Platform::Graphics::destroyPixmapBuffer(m_tilePool[i]->frontBuffer()->nativeBuffer());
    }

    // Release the m_visibleTileBuffer
    if (m_visibleTileBuffer) {
        m_visibleTileBuffer->frontBuffer()->clearRenderedRegion();
        BlackBerry::Platform::Graphics::clearBuffer(m_visibleTileBuffer->frontBuffer()->nativeBuffer(), 0, 0, 0, 0);
        BlackBerry::Platform::Graphics::destroyPixmapBuffer(m_visibleTileBuffer->frontBuffer()->nativeBuffer());
    }

    // Release the backbuffer
    if (backBuffer()) {
        backBuffer()->clearRenderedRegion();
        BlackBerry::Platform::Graphics::clearBuffer(backBuffer()->nativeBuffer(), 0, 0, 0, 0);
        BlackBerry::Platform::Graphics::destroyPixmapBuffer(backBuffer()->nativeBuffer());
    }
}

}
}
