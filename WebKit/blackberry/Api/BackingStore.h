/*
 * Copyright (C) Research In Motion Limited 2009. All rights reserved.
 */

#ifndef BackingStore_h
#define BackingStore_h

#include "BlackBerryGlobal.h"

#include <BlackBerryPlatformGraphics.h>
#include <BlackBerryPlatformPrimitives.h>

namespace WebCore {
    class ChromeClientBlackBerry;
    class FloatPoint;
    class FrameLoaderClientBlackBerry;
    class GLES2Context;
    class IntRect;
}

namespace Olympia {
    namespace WebKit {

        class WebPage;
        class WebPagePrivate;
        class BackingStorePrivate;
        class BackingStoreClient;

        class OLYMPIA_EXPORT BackingStore {
        public:
            enum ResumeUpdateOperation { None, Blit, RenderAndBlit };
            BackingStore(WebPage*, BackingStoreClient*);
            virtual ~BackingStore();

            void createSurface();

            void suspendScreenAndBackingStoreUpdates();
            void resumeScreenAndBackingStoreUpdates(ResumeUpdateOperation op);

            bool isScrollingOrZooming() const;
            void setScrollingOrZooming(bool scrollingOrZooming);

            void blitContents(const Olympia::Platform::IntRect& dstRect,
                              const Olympia::Platform::IntRect& contents);
            void repaint(int x, int y, int width, int height,
                         bool contentChanged, bool immediate);

            bool hasIdleJobs() const;
            void renderOnIdle();

            bool isDirectRenderingToWindow() const;

        private:
            friend class Olympia::WebKit::BackingStoreClient;
            friend class Olympia::WebKit::WebPage;
            friend class Olympia::WebKit::WebPagePrivate; // FIXME: Hack like the rest
            friend class WebCore::ChromeClientBlackBerry;
            friend class WebCore::FrameLoaderClientBlackBerry;
            friend class WebCore::GLES2Context;
            BackingStorePrivate *d;
        };
    }
}

#endif // BackingStore_h
