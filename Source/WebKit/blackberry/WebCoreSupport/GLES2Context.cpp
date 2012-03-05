/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2010, 2011 Research In Motion Limited. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "GLES2Context.h"

#include "Assertions.h"
#include "BackingStoreCompositingSurface.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "NotImplemented.h"
#include "Page.h"
#include "SurfacePool.h"
#include "WebPageClient.h"
#include "WebPage_p.h"

#include <BlackBerryPlatformWindow.h>

#include <GLES2/gl2.h>

namespace WebCore {

struct GLES2ContextPrivate {
    GLES2ContextPrivate()
        : m_window(0)
        , m_isWebPageWindow(false)
    {
    }

    BlackBerry::WebKit::BackingStoreCompositingSurface* compositingSurface() const
    {
        return BlackBerry::WebKit::SurfacePool::globalSurfacePool()->compositingSurface();
    }

    BlackBerry::Platform::Graphics::Buffer* buffer() const
    {
        if (m_window)
            return m_window->buffer();
#if ENABLE_COMPOSITING_SURFACE
        BlackBerry::WebKit::BackingStoreCompositingSurface* surface = compositingSurface();
        if (surface)
            return surface->backBuffer()->nativeBuffer();
#endif

        ASSERT_NOT_REACHED();
        return 0;
    }
    BlackBerry::Platform::Graphics::Window* m_window;
    bool m_isWebPageWindow;
};

PassOwnPtr<GLES2Context> GLES2Context::create(Page* page)
{
    return adoptPtr(new GLES2Context(page));
}

GLES2Context::GLES2Context(Page* page)
    : d(adoptPtr(new GLES2ContextPrivate))
{
    if (page) {
        BlackBerry::WebKit::WebPagePrivate* webPagePrivate =
            static_cast<BlackBerry::WebKit::WebPagePrivate*>(page->chrome()->client()->platformPageClient());

        bool mainWindowHasGLUsage = webPagePrivate->m_client->window()->windowUsage() == BlackBerry::Platform::Graphics::Window::GLES2Usage;

        if (mainWindowHasGLUsage || !d->compositingSurface()) {
            d->m_window = static_cast<BlackBerry::Platform::Graphics::Window*>(webPagePrivate->m_client->compositingWindow());
            d->m_isWebPageWindow = d->m_window == webPagePrivate->m_client->window();

            if (!d->m_isWebPageWindow)
                d->m_window->setVisible(true);
        }
    } else {
        // Create offscreen context
        notImplemented();
    }
}

GLES2Context::~GLES2Context()
{
    if (d->m_window && !d->m_isWebPageWindow)
        d->m_window->setVisible(false);
}

IntSize GLES2Context::surfaceSize() const
{
    if (d->m_window)
        return d->m_window->surfaceSize();
#if ENABLE_COMPOSITING_SURFACE
    BlackBerry::WebKit::BackingStoreCompositingSurface* surface = d->compositingSurface();
    if (surface)
        return surface->backBuffer()->surfaceSize();
#endif
    ASSERT_NOT_REACHED();
    return IntSize();
}

bool GLES2Context::makeCurrent()
{
    BlackBerry::Platform::Graphics::makeBufferCurrent(d->buffer(), BlackBerry::Platform::Graphics::GLES2);
    return true;
}

bool GLES2Context::destroy()
{
    return true;
}

bool GLES2Context::swapBuffers()
{
    ASSERT(glGetError() == GL_NO_ERROR);

#if ENABLE_COMPOSITING_SURFACE
    // Sniff, this is so sad
    glFinish();

    BlackBerry::WebKit::BackingStoreCompositingSurface* surface = d->compositingSurface();
    if (surface)
        surface->swapBuffers();
#endif
    if (d->m_window && !d->m_isWebPageWindow) {
        d->m_window->post(BlackBerry::Platform::IntRect(
            BlackBerry::Platform::IntPoint(0, 0), d->m_window->surfaceSize()));
    }

    return true;
}

}
