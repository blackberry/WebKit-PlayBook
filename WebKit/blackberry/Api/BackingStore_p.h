/*
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 */

#ifndef BackingStore_p_h
#define BackingStore_p_h

#include "BackingStore.h"
#include "IntPoint.h"
#include "IntSize.h"
#include "RenderQueue.h"
#include "TileIndex.h"
#include "TileIndexHash.h"
#include "Timer.h"
#include "TransformationMatrix.h"

#include <BlackBerryPlatformGraphics.h>
#include <BlackBerryPlatformGuardedPointer.h>
#include <wtf/HashMap.h>
#include <wtf/Vector.h>

#if PLATFORM(OPENVG)
#include <egl.h>
#endif

#include <pthread.h>

namespace WebCore {
    class IntRect;
    typedef WTF::Vector<IntRect> IntRectList;
    class TransformationMatrix;
}

namespace Olympia {
    namespace WebKit {

        class BackingStoreTile;
        class TileBuffer;
        class WebPage;
        class BackingStoreClient;

        typedef WTF::HashMap<TileIndex, BackingStoreTile*> TileMap;
        class BackingStoreGeometry {
        public:
            BackingStoreGeometry()
                : m_numberOfTilesWide(0)
                , m_numberOfTilesHigh(0) {}

            /* The backingstore geometry functions */
            WebCore::IntRect backingStoreRect() const;
            WebCore::IntSize backingStoreSize() const;

            /* Set and get the state */
            int numberOfTilesWide() const { return m_numberOfTilesWide; }
            void setNumberOfTilesWide(int n) { m_numberOfTilesWide = n; }
            int numberOfTilesHigh() const { return m_numberOfTilesHigh; }
            void setNumberOfTilesHigh(int n) { m_numberOfTilesHigh = n; }
            WebCore::IntPoint backingStoreOffset() const { return m_backingStoreOffset; }
            void setBackingStoreOffset(const WebCore::IntPoint& o) { m_backingStoreOffset = o; }
            BackingStoreTile* tileAt(TileIndex index) const { return m_tileMap.get(index); }
            const TileMap& tileMap() const { return m_tileMap; }
            void setTileMap(const TileMap& map) { m_tileMap = map; }

          private:
            int m_numberOfTilesWide;
            int m_numberOfTilesHigh;
            WebCore::IntPoint m_backingStoreOffset;
            TileMap m_tileMap;
        };

        class BackingStoreWindowBufferState {
        public:
            WebCore::IntRectRegion blittedRegion() const { return m_blittedRegion; }
            void addBlittedRegion(const WebCore::IntRectRegion& region)
            {
                m_blittedRegion = WebCore::IntRectRegion::unionRegions(m_blittedRegion, region);
            }
            void clearBlittedRegion(const WebCore::IntRectRegion& region)
            {
                m_blittedRegion = WebCore::IntRectRegion::subtractRegions(m_blittedRegion, region);
            }
            void clearBlittedRegion() { m_blittedRegion = WebCore::IntRectRegion(); }

            bool isRendered(const WebCore::IntPoint& scrollPosition, const WebCore::IntRectRegion& contents) const
            {
                return WebCore::IntRectRegion::subtractRegions(contents, m_blittedRegion).isEmpty();
            }

          private:
            WebCore::IntRectRegion m_blittedRegion;
        };

        class BackingStorePrivate : public Olympia::Platform::GuardedPointerBase {
        public:
            enum TileMatrixDirection { Horizontal, Vertical };
            BackingStorePrivate();
            ~BackingStorePrivate();

            // Returns whether direct rendering is explicitly turned on or is
            // required because the surface pool is not large enough to meet
            // the minimum number of tiles required to scroll
            bool shouldDirectRenderingToWindow() const;

            // Suspends all screen updates so that 'blitContents' is disabled.
            void suspendScreenAndBackingStoreUpdates();

            // Resumes all screen updates so that 'blitContents' is enabled.
            void resumeScreenAndBackingStoreUpdates(BackingStore::ResumeUpdateOperation op);

            /* Called from outside WebKit and within WebKit via ChromeClientBlackBerry */
            void repaint(const WebCore::IntRect& windowRect,
                         bool contentChanged, bool immediate);

            /* Called from within WebKit via ChromeClientBlackBerry */
            void slowScroll(const WebCore::IntSize& delta,
                            const WebCore::IntRect& windowRect,
                            bool immediate);

            /* Called from within WebKit via ChromeClientBlackBerry */
            void scroll(const WebCore::IntSize& delta,
                        const WebCore::IntRect& scrollViewRect,
                        const WebCore::IntRect& clipRect);
            void scrollingStartedHelper(const WebCore::IntSize& delta);

            bool shouldSuppressNonVisibleRegularRenderJobs() const;
            bool shouldPerformRenderJobs() const;
            bool shouldPerformRegularRenderJobs() const;
            void startRenderTimer();
            void stopRenderTimer();
            void renderOnTimer(WebCore::Timer<BackingStorePrivate>*);
            void renderOnIdle();
            bool willFireTimer();

            /* Set of helper methods for the scrollBackingStore() method */
            WebCore::IntRect contentsRect() const;
            WebCore::IntRect expandedContentsRect() const;
            WebCore::IntRect visibleContentsRect() const;
            WebCore::IntRect unclippedVisibleContentsRect() const;
            bool shouldMoveLeft(const WebCore::IntRect&) const;
            bool shouldMoveRight(const WebCore::IntRect&) const;
            bool shouldMoveUp(const WebCore::IntRect&) const;
            bool shouldMoveDown(const WebCore::IntRect&) const;
            bool canMoveX(const WebCore::IntRect&) const;
            bool canMoveY(const WebCore::IntRect&) const;
            bool canMoveLeft(const WebCore::IntRect&) const;
            bool canMoveRight(const WebCore::IntRect&) const;
            bool canMoveUp(const WebCore::IntRect&) const;
            bool canMoveDown(const WebCore::IntRect&) const;

            WebCore::IntRect backingStoreRectForScroll(int deltaX, int deltaY,
                                                       const WebCore::IntRect&) const;
            void setBackingStoreRect(const WebCore::IntRect&);

            typedef WTF::Vector<TileIndex> TileIndexList;
            TileIndexList indexesForBackingStoreRect(const WebCore::IntRect&) const;

            WebCore::IntPoint originOfLastRenderForTile(const TileIndex& index,
                                                        BackingStoreTile* tile,
                                                        const WebCore::IntRect& backingStoreRect) const;

            TileIndex indexOfLastRenderForTile(const TileIndex& index, BackingStoreTile* tile) const;
            TileIndex indexOfTile(const WebCore::IntPoint& origin,
                                  const WebCore::IntRect& backingStoreRect) const;
            void clearAndUpdateTileOfNotRenderedRegion(const TileIndex& index,
                                                       BackingStoreTile* tile,
                                                       const WebCore::IntRectRegion&,
                                                       const WebCore::IntRect& backingStoreRect,
                                                       bool update = true);
            bool isCurrentVisibleJob(const TileIndex& index, BackingStoreTile* tile,
                                     const WebCore::IntRect& backingStoreRect) const;

            /* Responsible for scrolling the backingstore and updating the
             * tile matrix geometry.
             */
            void scrollBackingStore(int deltaX, int deltaY);

            /* Render the tiles dirty rects and blit to the screen */
            bool renderDirectToWindow(const WebCore::IntRect& rect, bool renderContentOnly);
            bool render(const WebCore::IntRect& rect, bool renderContentOnly);
            bool render(const WebCore::IntRectList& rectList, bool renderContentOnly);

            /* Called by the render queue to ensure that the queue is in a
             * constant state before performing a render job.
             */
            void requestLayoutIfNeeded() const;

            /* Helper render methods */
            void renderVisibleContents(bool renderContentOnly = false);
            void renderBackingStore(bool renderContentOnly = false);
            void blitVisibleContents();

            /* Assumes the rect to be in window/viewport coordinates */
            void copyPreviousContentsToBackSurfaceOfWindow();
            void copyPreviousContentsToBackSurfaceOfTile(const WebCore::IntRect&,
                                                         BackingStoreTile*);
            void paintDefaultBackground(const WebCore::IntRect& contents,
                                        const WebCore::TransformationMatrix&,
                                        bool flush);
            void blitContents(const WebCore::IntRect& dstRect,
                              const WebCore::IntRect& contents);

            typedef std::pair<TileIndex, WebCore::IntRect> TileRect;
            WebCore::IntRect blitTileRect(TileBuffer*,
                                          const TileRect&,
                                          const WebCore::IntPoint&,
                                          const WebCore::TransformationMatrix&,
                                          BackingStoreGeometry*);

#if USE(ACCELERATED_COMPOSITING)
            void blendCompositingSurface(const WebCore::IntPoint& origin,
                                         const WebCore::IntRect& dstRect,
                                         const WebCore::TransformationMatrix& matrix);
            void clearCompositingSurface();
            bool drawSubLayers();
#endif

            void blitHorizontalScrollbar(const WebCore::IntPoint&);
            void blitVerticalScrollbar(const WebCore::IntPoint&);

            /* Returns whether the tile index is currently visible or not */
            bool isTileVisible(const TileIndex&) const;
            bool isTileVisible(const WebCore::IntPoint&) const;

            /* Returns a rect that is the union of all tiles that are visible */
            WebCore::IntRect visibleTilesRect() const;

            /* Used to clip to the visible content for instance */
            WebCore::IntRect tileVisibleContentsRect(const TileIndex&) const;

            /* Used to clip to the unclipped visible content for instance which includes overscroll */
            WebCore::IntRect tileUnclippedVisibleContentsRect(const TileIndex&) const;

            /* Used to clip to the contents for instance */
            WebCore::IntRect tileContentsRect(const TileIndex&,
                                              const WebCore::IntRect&) const;
            WebCore::IntRect tileContentsRect(const TileIndex&,
                                              const WebCore::IntRect&,
                                              BackingStoreGeometry* state) const;

            /* This is called by WebPage once load is committed to reset the render queue */
            void resetRenderQueue();
            /* This is called by FrameLoaderClient that explicitly paints on first visible layout */
            void clearVisibleZoom();

            /* This is called by WebPage once load is committed to reset all the tiles */
            void resetTiles(bool resetBackground);

            /* This is called by WebPage after load is complete to update all the tiles */
            void updateTiles(bool updateVisible, bool immediate);

            /* This is called during scroll and by the render queue */
            void updateTilesForScrollOrNotRenderedRegion(bool checkLoading = true);

            /* Reset an individual tile */
            void resetTile(const TileIndex&, BackingStoreTile*, bool resetBackground);

            /* Update an individual tile */
            void updateTile(const TileIndex&, bool immediate);
            void updateTile(const WebCore::IntPoint&, bool immediate);

            WebCore::IntRect mapFromTilesToTransformedContents(const TileRect&) const;
            WebCore::IntRect mapFromTilesToTransformedContents(const TileRect&,
                                                               const WebCore::IntRect&) const;

            typedef WTF::Vector<TileRect> TileRectList;
            TileRectList mapFromTransformedContentsToAbsoluteTileBoundaries(
                const WebCore::IntRect&) const;
            TileRectList mapFromTransformedContentsToTiles(const WebCore::IntRect&) const;
            TileRectList mapFromTransformedContentsToTiles(const WebCore::IntRect&,
                                                           BackingStoreGeometry*) const;

            void updateTileMatrixIfNeeded();

            /* Called by WebPagePrivate::notifyTransformedContentsSizeChanged */
            void contentsSizeChanged(const WebCore::IntSize&);

            /* Called by WebPagePrivate::notifyTransformedScrollChanged */
            void scrollChanged(const WebCore::IntPoint&);

            /* Called by WebpagePrivate::notifyTransformChanged */
            void transformChanged();

            /* Called by WebpagePrivate::actualVisibleSizeChanged */
            void actualVisibleSizeChanged(const WebCore::IntSize&);

            /* Called by WebPagePrivate::setScreenRotated */
            void orientationChanged();

            /* Sets the geometry of the tile matrix */
            void setGeometryOfTileMatrix(int numberOfTilesWide, int numberOfTilesHigh);

            /* Create the surfaces of the backingstore */
            void createSurfaces();
            void createVisibleTileBuffer();

            /* Various calculations of quantities relevant to backingstore */
            WebCore::IntPoint originOfTile(const TileIndex&) const;
            WebCore::IntPoint originOfTile(const TileIndex&, const WebCore::IntRect&) const;
            int minimumNumberOfTilesWide() const;
            int minimumNumberOfTilesHigh() const;
            WebCore::IntSize expandedContentsSize() const;

            /* The tile geometry methods are all static function */
            static int tileWidth();
            static int tileHeight();
            static WebCore::IntSize tileSize();
            static WebCore::IntRect tileRect();

            /* This takes transformed contents coordinates */
            void renderContents(Olympia::Platform::Graphics::Buffer* buffer,
                                const WebCore::IntPoint& surfaceOffset,
                                const WebCore::IntRect& contentsRect) const;

            void blitToWindow(const WebCore::IntRect& dstRect,
                              const Olympia::Platform::Graphics::Buffer* srcBuffer,
                              const WebCore::IntRect& srcRect,
                              bool blend,
                              unsigned char globalAlpha);
            void checkerWindow(const WebCore::IntRect& dstRect,
                               const WebCore::IntPoint& contentsOrigin,
                               double contentsScale);

            void setGLStateDirty() { m_isGLStateDirty = true; }

            void invalidateWindow();
            void invalidateWindow(const WebCore::IntRect& dst);
            void clearWindow();
            void clearWindow(const WebCore::IntRect&,
                             unsigned char red,
                             unsigned char green,
                             unsigned char blue,
                             unsigned char alpha = 255);

            bool isScrollingOrZooming() const;
            void setScrollingOrZooming(bool scrollingOrZooming, bool shouldBlit=true);

            void lockBackingStore();
            void unlockBackingStore();

            BackingStoreGeometry* frontState() const;
            BackingStoreGeometry* backState() const;
            void swapState();

            BackingStoreWindowBufferState* windowFrontBufferState() const;
            BackingStoreWindowBufferState* windowBackBufferState() const;

            static void setCurrentBackingStoreOwner(WebPage* webPage) { BackingStorePrivate::g_currentBackingStoreOwner = webPage; }
            static WebPage* currentBackingStoreOwner() { return BackingStorePrivate::g_currentBackingStoreOwner; }
            bool isActive() const;

            // FIXME: Remove invokers! Grrr...
            void invokeBlitContents(WebCore::IntRect dstRect,
                                    WebCore::IntRect contents);
            void invokeBlendCompositingSurface(WebCore::IntPoint origin,
                                               WebCore::IntRect dstRect,
                                               WebCore::TransformationMatrix matrix);
            void invokeInvalidateWindow(WebCore::IntRect dst);
            void invokeClearWindow(WebCore::IntRect rect,
                                   unsigned char red,
                                   unsigned char green,
                                   unsigned char blue,
                                   unsigned char alpha);


            static WebPage* g_currentBackingStoreOwner;

            bool m_suspendScreenUpdates;
            bool m_suspendBackingStoreUpdates;

            bool m_suspendRenderJobs;
            bool m_suspendRegularRenderJobs;
            bool m_isScrollingOrZooming;
            mutable bool m_isGLStateDirty;
            WebPage* m_webPage;
            BackingStoreClient* m_client;
            RenderQueue* m_renderQueue;
            mutable WebCore::IntSize m_previousDelta;

#if PLATFORM(OPENVG)
            EGLDisplay m_eglDisplay;
#endif
            mutable unsigned m_frontState;
            mutable unsigned m_backState;

            unsigned m_currentWindowBackBuffer;
            mutable BackingStoreWindowBufferState m_windowBufferState[2];

            TileMatrixDirection m_preferredTileMatrixDimension;

            WebCore::IntRect m_visibleTileBufferRect;

            // Last resort timer for rendering
            WebCore::Timer<BackingStorePrivate>* m_renderTimer;

            pthread_mutex_t m_mutex;
        };
    }
}

#endif // BackingStore_p_h
