/*
 * Copyright (C) Research In Motion Limited 2010-2011. All rights reserved.
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


#ifndef LayerGLES2_h
#define LayerGLES2_h

#if USE(ACCELERATED_COMPOSITING)

#include <BlackBerryPlatformGuardedPointer.h>
#include "LayerBaseGLES2.h"

#include "ImageData.h"
#include "FloatQuad.h"

#include <wtf/Deque.h>
#include <wtf/ThreadSafeShared.h>

namespace Olympia {
namespace Platform {
namespace Graphics {
class Buffer;
}
}
}

namespace WebCore {

class DestroyOnCompositingThread;
class GLES2Context;
class LayerMessage;
class LayerRendererGLES2;

class LayerGLES2 : public ThreadSafeShared<LayerGLES2>, public LayerBaseGLES2, public Olympia::Platform::GuardedPointerBase {
public:
    static PassRefPtr<LayerGLES2> create(LayerType);

    ~LayerGLES2();

    // Thread safe
    void invokeSetNeedsDisplay(PassRefPtr<ImageData>, FloatRect dirtyRect, IntSize requiredTextureSize);
    void setNeedsDisplay(PassRefPtr<ImageData>, const FloatRect& dirtyRect, const IntSize& requiredTextureSize);
    void setPluginView(PluginView*);
#if ENABLE(VIDEO)
    void setMediaPlayer(MediaPlayer*);
#endif

    // Not thread safe
    void updateTextureContents(GLES2Context*);
    const LayerGLES2* rootLayer() const;
    void setSublayers(const Vector<RefPtr<LayerGLES2> >& sublayers);
    const Vector<RefPtr<LayerGLES2> >& getSublayers() const { return m_sublayers; }
    void setSuperlayer(LayerGLES2* superlayer) { m_superlayer = superlayer; }
    LayerGLES2* superlayer() const;
    bool contentsDirty() { return m_contentsDirty; }

    // The layer renderer must be set if the layer has been rendered
    void setLayerRenderer(LayerRendererGLES2*);

    void setDrawTransform(const TransformationMatrix& matrix);
    const TransformationMatrix& drawTransform() const { return m_drawTransform; }

    void setDrawOpacity(float opacity) { m_drawOpacity = opacity; }
    float drawOpacity() const { return m_drawOpacity; }

    FloatRect getDrawRect() const { return m_drawRect; }
    const FloatQuad& getTransformedBounds() const { return m_transformedBounds; }
    FloatQuad getTransformedHolePunchRect() const;

    void deleteTextures();
    bool texturesWereDeleted() const;

    void drawTextures(int positionLocation, int texCoordLocation, const FloatRect& visibleRect);

    void releaseTextureResources();

private:
    LayerGLES2(LayerType);

    friend class DestroyOnCompositingThread;
    void destroyOnCompositingThread();

    void updateTileContents(const IntRect& tile);

    void removeFromSuperlayer();

    size_t numSublayers() const
    {
        return m_sublayers.size();
    }

    // Returns the index of the sublayer or -1 if not found.
    int indexOfSublayer(const LayerGLES2*);

    // This should only be called from removeFromSuperlayer.
    void removeSublayer(LayerGLES2*);

    LayerRendererGLES2* m_layerRenderer;

    Vector<RefPtr<LayerGLES2> > m_sublayers;
    LayerGLES2* m_superlayer;

    bool m_contentsDirty;
    RefPtr<ImageData> m_contents;
    FloatRect m_dirtyRect;
    IntSize m_requiredTextureSize;

    Vector<unsigned int> m_allocatedTextureIds;
    IntSize m_allocatedTextureSize;

    // Vertex data for the bounds of this layer
    FloatQuad m_transformedBounds;
    // The bounding rectangle of the transformed layer
    FloatRect m_drawRect;

    Olympia::Platform::Graphics::Buffer* m_pluginBuffer;

    TransformationMatrix m_drawTransform;
    float m_drawOpacity;

    bool m_texturesWereDeleted;
};

}

#endif // USE(ACCELERATED_COMPOSITING)

#endif
