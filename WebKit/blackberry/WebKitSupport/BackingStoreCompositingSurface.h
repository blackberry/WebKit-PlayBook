/*
 * Copyright (C) Research In Motion Limited 2009. All rights reserved.
 */

#ifndef BackingStoreCompositingSurface_h
#define BackingStoreCompositingSurface_h

#if USE(ACCELERATED_COMPOSITING)

#include "IntRect.h"
#include "IntRectRegion.h"
#include "IntSize.h"

namespace Olympia {
    namespace Platform {
    namespace Graphics {
    struct Buffer;
    }
    }

    namespace WebKit {
        class CompositingSurfaceBuffer {
        public:
            CompositingSurfaceBuffer(const WebCore::IntSize&);
            ~CompositingSurfaceBuffer();
            WebCore::IntSize surfaceSize() const;

            Olympia::Platform::Graphics::Buffer* nativeBuffer() const;

        private:
            WebCore::IntSize m_size;
            mutable Olympia::Platform::Graphics::Buffer* m_buffer;
        };


        class BackingStoreCompositingSurface : public ThreadSafeShared<BackingStoreCompositingSurface> {
        public:
            static  PassRefPtr<BackingStoreCompositingSurface> create(const WebCore::IntSize& size, bool doubleBuffered)
            {
                return adoptRef(new BackingStoreCompositingSurface(size, doubleBuffered));
            }

            ~BackingStoreCompositingSurface();

            CompositingSurfaceBuffer* frontBuffer() const;
            CompositingSurfaceBuffer* backBuffer() const;
            bool isDoubleBuffered() const { return m_isDoubleBuffered; }
            void swapBuffers();

            bool needsSync() const { return m_needsSync; }
            void setNeedsSync(bool needsSync) { m_needsSync = needsSync; }

        private:
            BackingStoreCompositingSurface(const WebCore::IntSize&, bool doubleBuffered);

            mutable unsigned m_frontBuffer;
            mutable unsigned m_backBuffer;
            bool m_isDoubleBuffered;
            bool m_needsSync;
        };
    }
}

#endif // USE(ACCELERATED_COMPOSITING)

#endif // BackingStoreCompositingSurface_h
