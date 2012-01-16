/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 */

#ifndef SurfacePool_h
#define SurfacePool_h

#include "BackingStoreTile.h"
#include "GraphicsContext.h"
#include "IntSize.h"
#include "BlackBerryPlatformGraphics.h"

#if PLATFORM(OPENVG)
#include "SurfaceOpenVG.h"
#elif PLATFORM(SKIA)
#include "PlatformContextSkia.h"
#endif

#include <wtf/Vector.h>

#define ENABLE_COMPOSITING_SURFACE 1

namespace Olympia {
    namespace WebKit {

        class BackingStoreCompositingSurface;

        class SurfacePool {
        public:
            static SurfacePool* globalSurfacePool();

            void initialize(const WebCore::IntSize&);

            int isEmpty() const { return m_tilePool.isEmpty(); }
            int size() const { return m_tilePool.size(); }

            typedef WTF::Vector<BackingStoreTile*> TileList;
            const TileList tileList() const { return m_tilePool; }

            PlatformGraphicsContext* createPlatformGraphicsContext(Olympia::Platform::Graphics::Drawable*) const;
            PlatformGraphicsContext* lockTileRenderingSurface() const;
            void releaseTileRenderingSurface(PlatformGraphicsContext*) const;
            BackingStoreTile* visibleTileBuffer() const { return m_visibleTileBuffer; }

            void initializeVisibleTileBuffer(const WebCore::IntSize&);

            // This is a shared back buffer that is used by all the tiles since
            // only one tile will be rendering it at a time and we invalidate
            // the whole tile every time we render by copying from the front
            // buffer those portions that we don't render.  This allows us to
            // have N+1 tilebuffers rather than N*2 for our double buffered
            // backingstore
            TileBuffer* backBuffer() const;

            BackingStoreCompositingSurface* compositingSurface() const;

        private:
            // This is necessary so BackingStoreTile can atomically swap buffers with m_backBuffer
            friend class BackingStoreTile;

            SurfacePool();

            TileList m_tilePool;
            BackingStoreTile* m_visibleTileBuffer;
#if USE(ACCELERATED_COMPOSITING)
            RefPtr<BackingStoreCompositingSurface> m_compositingSurface;
#endif
#if PLATFORM(OPENVG)
            PlatformGraphicsContext* m_tileRenderingSurface;
#elif PLATFORM(SKIA)
            Olympia::Platform::Graphics::Buffer* m_tileRenderingSurface;
#endif
            unsigned m_backBuffer;
        };
    }
}

#endif // SurfacePool_h
