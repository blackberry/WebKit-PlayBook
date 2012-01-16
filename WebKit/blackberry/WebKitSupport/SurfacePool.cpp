/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 */

#include "SurfacePool.h"

#if PLATFORM(OPENVG)
#include "EGLDisplayOpenVG.h"
#include "EGLUtils.h"
#include "SurfaceOpenVG.h"
#elif PLATFORM(SKIA)
#include "PlatformContextSkia.h"
#endif

#if USE(ACCELERATED_COMPOSITING)
#include "BackingStoreCompositingSurface.h"
#endif

#include <BlackBerryPlatformGraphics.h>
#include <BlackBerryPlatformMisc.h>
#include <BlackBerryPlatformScreen.h>
#include <BlackBerryPlatformSettings.h>

using namespace WebCore;

namespace Olympia {
namespace WebKit {

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
{
}

void SurfacePool::initialize(const WebCore::IntSize& tileSize)
{
    static bool initialized = false;
    if (initialized)
        return;
    initialized = true;

#if PLATFORM(OPENVG)
    m_tileRenderingSurface = createPlatformGraphicsContext(Olympia::Platform::Graphics::drawingSurface());
#elif PLATFORM(SKIA)
    m_tileRenderingSurface = Olympia::Platform::Graphics::drawingSurface();
#endif

#if USE(ACCELERATED_COMPOSITING) && ENABLE_COMPOSITING_SURFACE
    int landscapeWidth = Olympia::Platform::Graphics::Screen::landscapeWidth();
    // Use width x width so we don't have to reallocate the surface
    // on screen rotation
    IntSize size(landscapeWidth, landscapeWidth);

    m_compositingSurface = BackingStoreCompositingSurface::create(size, false /*doubleBuffered*/);
#endif

    const unsigned tileNumber = Olympia::Platform::Settings::get()->numberOfBackingStoreTiles();

    if (!tileNumber)
        return; // we only use direct rendering when 0 tiles are specified

    // Create the shared backbuffer
    m_backBuffer = reinterpret_cast<unsigned>(new TileBuffer(tileSize));

    for (size_t i = 0; i < tileNumber; ++i)
        m_tilePool.append(BackingStoreTile::create(tileSize, BackingStoreTile::DoubleBuffered));
}

PlatformGraphicsContext* SurfacePool::createPlatformGraphicsContext(Olympia::Platform::Graphics::Drawable* drawable) const
{
#if PLATFORM(OPENVG)
    SurfaceOpenVG* platformGraphicsContext = SurfaceOpenVG::adoptExistingSurface(
        Olympia::Platform::Graphics::eglDisplay(), drawable->surface(),
        SurfaceOpenVG::DontTakeSurfaceOwnership, drawable->surfaceType());

    platformGraphicsContext->setApplyFlipTransformationOnPainterCreation(drawable->isFlipTransformationRequired());
#elif PLATFORM(SKIA)
    PlatformContextSkia* platformGraphicsContext = new PlatformContextSkia(drawable);
#endif
    return platformGraphicsContext;
}

PlatformGraphicsContext* SurfacePool::lockTileRenderingSurface() const
{
#if PLATFORM(OPENVG)
    return m_tileRenderingSurface;
#elif PLATFORM(SKIA)
    if (!m_tileRenderingSurface)
        return 0;
    return createPlatformGraphicsContext(Olympia::Platform::Graphics::lockBufferDrawable(m_tileRenderingSurface));
#endif
}

void SurfacePool::releaseTileRenderingSurface(PlatformGraphicsContext* context) const
{
#if PLATFORM(SKIA)
    if (!m_tileRenderingSurface)
        return;

    delete context;
    Olympia::Platform::Graphics::releaseBufferDrawable(m_tileRenderingSurface);
#endif
}

void SurfacePool::initializeVisibleTileBuffer(const WebCore::IntSize& visibleSize)
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

}
}
