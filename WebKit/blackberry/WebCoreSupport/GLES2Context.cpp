/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
    {
    }

    Olympia::WebKit::BackingStoreCompositingSurface* compositingSurface() const
    {
        return Olympia::WebKit::SurfacePool::globalSurfacePool()->compositingSurface();
    }

    Olympia::Platform::Graphics::Buffer* buffer() const
    {
#if ENABLE_COMPOSITING_SURFACE
        Olympia::WebKit::BackingStoreCompositingSurface* surface = compositingSurface();
        if (surface)
            return surface->backBuffer()->nativeBuffer();
#endif
        if (m_window)
            return m_window->buffer();

        ASSERT_NOT_REACHED();
        return 0;
    }
    Olympia::Platform::Graphics::Window* m_window;
};

PassOwnPtr<GLES2Context> GLES2Context::create(Page* page)
{
    PassOwnPtr<GLES2Context> context(new GLES2Context(page));

    return context;
}

GLES2Context::GLES2Context(Page* page)
    : d(new GLES2ContextPrivate)
{
    if (page) {
        Olympia::WebKit::WebPagePrivate* webPagePrivate =
            static_cast<Olympia::WebKit::WebPagePrivate*>(page->chrome()->client()->platformPageClient());

        if (!d->compositingSurface()) {
            d->m_window = static_cast<Olympia::Platform::Graphics::Window*>(webPagePrivate->m_client->compositingWindow());
            d->m_window->setVisible(true);
        }
    } else {
        // Create offscreen context
        notImplemented();
    }
}

GLES2Context::~GLES2Context()
{
    if (d->m_window)
        d->m_window->setVisible(false);
}

IntSize GLES2Context::surfaceSize() const
{
#if ENABLE_COMPOSITING_SURFACE
    Olympia::WebKit::BackingStoreCompositingSurface* surface = d->compositingSurface();
    if (surface)
        return surface->backBuffer()->surfaceSize();
#endif
    if (d->m_window)
        return d->m_window->surfaceSize();

    ASSERT_NOT_REACHED();
    return IntSize();
}

bool GLES2Context::makeCurrent()
{
    Olympia::Platform::Graphics::makeBufferCurrent(d->buffer(), Olympia::Platform::Graphics::GLES2);
    return true;
}

bool GLES2Context::destroy()
{
    return true;
}

bool GLES2Context::swapBuffers()
{
    GLenum glErrorCode = GL_NO_ERROR;
    ASSERT((glErrorCode = glGetError()) == GL_NO_ERROR);

#if ENABLE_COMPOSITING_SURFACE
    // Sniff, this is so sad
    glFinish();

    Olympia::WebKit::BackingStoreCompositingSurface* surface = d->compositingSurface();
    if (surface)
        surface->swapBuffers();
#endif
    if (d->m_window) {
        d->m_window->post(Olympia::Platform::IntRect(
            Olympia::Platform::IntPoint(0, 0), d->m_window->surfaceSize()));
    }

    return true;
}

}
