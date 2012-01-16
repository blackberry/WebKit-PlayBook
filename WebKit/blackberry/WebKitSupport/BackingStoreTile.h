/*
 * Copyright (C) Research In Motion Limited 2009. All rights reserved.
 */

#ifndef BackingStoreTile_h
#define BackingStoreTile_h

#include "IntRect.h"
#include "IntRectRegion.h"
#include "IntSize.h"
#include "SwapGroup.h"

namespace Olympia {
    namespace Platform {
    namespace Graphics {
    struct Buffer;
    }
    }

    namespace WebKit {
        class TileBuffer {
            public:
                TileBuffer(const WebCore::IntSize&);
                ~TileBuffer();
                WebCore::IntSize size() const;
                WebCore::IntRect rect() const;

                bool isRendered() const;
                bool isRendered(const WebCore::IntRectRegion& contents) const;
                void clearRenderedRegion();
                void clearRenderedRegion(const WebCore::IntRectRegion& region);
                void addRenderedRegion(const WebCore::IntRectRegion& region);
                WebCore::IntRectRegion renderedRegion() const;
                WebCore::IntRectRegion notRenderedRegion() const;

                Olympia::Platform::Graphics::Buffer* nativeBuffer() const;

            private:
                WebCore::IntSize m_size;
                WebCore::IntRectRegion m_renderedRegion;
                mutable Olympia::Platform::Graphics::Buffer* m_buffer;
        };


        class BackingStoreTile : public Swappable {
        public:
            enum BufferingMode { SingleBuffered, DoubleBuffered };

            static BackingStoreTile* create(const WebCore::IntSize& size, BufferingMode mode)
            {
                return new BackingStoreTile(size, mode);
            }

            ~BackingStoreTile();

            WebCore::IntSize size() const;
            WebCore::IntRect rect() const;

            TileBuffer* frontBuffer() const;
            TileBuffer* backBuffer() const;
            bool isDoubleBuffered() const { return m_bufferingMode == DoubleBuffered; }

            void reset();
            bool backgroundPainted() const { return m_backgroundPainted; }
            void paintBackground();

            bool isCommitted() const { return m_committed; }
            void setCommitted(bool committed) { m_committed = committed; }

            void clearShift() { m_horizontalShift = 0; m_verticalShift = 0; }
            int horizontalShift() const { return m_horizontalShift; }
            void setHorizontalShift(int shift) { m_horizontalShift = shift; }
            int verticalShift() const { return m_verticalShift; }
            void setVerticalShift(int shift) { m_verticalShift = shift; }

            // Swappable
            void swapBuffers();
            void didCommitSwapGroup();

        private:
            BackingStoreTile(const WebCore::IntSize&, BufferingMode mode);

            mutable unsigned m_frontBuffer;
            BufferingMode m_bufferingMode;
            bool m_checkered;
            bool m_committed;
            bool m_backgroundPainted;
            int m_horizontalShift;
            int m_verticalShift;
        };
    }
}

#endif // BackingStoreTile_h
