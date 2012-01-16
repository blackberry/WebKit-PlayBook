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


#ifndef LayerRendererGLES2_h
#define LayerRendererGLES2_h

#if USE(ACCELERATED_COMPOSITING)

#include "IntRect.h"
#include "IntRectRegion.h"
#include "LayerBaseGLES2.h"
#include "TransformationMatrix.h"
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

#if PLATFORM(CG)
#include <CoreGraphics/CGContext.h>
#include <wtf/RetainPtr.h>
#endif

namespace WebCore {

class GLES2Context;
class LayerGLES2;
class Page;

class LayerRenderingResults {
public:
    LayerRenderingResults() : wasEmpty(true) { }

    void addHolePunchRect(const IntRect& rect);
    IntRect holePunchRect(int index) const;
    int holePunchRectSize() { return m_holePunchRects.size(); }

    static const int NUM_DIRTY_RECTS = 3;
    const IntRect& dirtyRect(int i) const { return m_dirtyRects[i]; }
    void addDirtyRect(const IntRect& dirtyRect);
    bool isEmpty();

    bool wasEmpty;

    IntRectRegion dirtyRegion;

private:
    Vector<IntRect> m_holePunchRects; // in compositing surface coordinates
    IntRect m_dirtyRects[NUM_DIRTY_RECTS];
};

// Class that handles drawing of composited render layers using GL.
class LayerRendererGLES2 : public Noncopyable {
public:
    static PassOwnPtr<LayerRendererGLES2> create(Page* page);

    LayerRendererGLES2(Page* page);
    ~LayerRendererGLES2();

    // Recomposites all the layers.
    void drawLayers(const FloatRect& visibleRect, const IntRect& layoutRect, const IntSize& contentsSize, const IntRect& dstRect);

    void setRootLayer(LayerGLES2* layer);
    LayerGLES2* rootLayer() { return m_rootLayer.get(); }

    // Keep track of layers that need cleanup when the LayerRenderer is destroyed
    void addLayer(LayerGLES2*);
    bool removeLayer(LayerGLES2*);

    // Keep track of layers that need to release their textures when we swap buffers
    void addLayerToReleaseTextureResourcesList(LayerGLES2*);

    bool hardwareCompositing() const { return m_hardwareCompositing; }

    GLES2Context* context() const { return m_gles2Context.get(); }

    const LayerRenderingResults& lastRenderingResults() const { return m_lastRenderingResults; }

private:
    void updateLayersRecursive(LayerGLES2*, const TransformationMatrix& parentMatrix, float opacity, FloatRect clipRect);
    void compositeLayersRecursive(LayerGLES2*, int stencilValue, FloatRect clipRect);

    void drawDebugBorder(LayerGLES2*);
    void drawHolePunchRect(LayerGLES2*);

    IntRect toOpenGLWindowCoordinates(const FloatRect& r);
    IntRect toWebKitWindowCoordinates(const FloatRect& r);

    void bindCommonAttribLocation(int location, const char* attribName);

    bool initGL();
    bool makeContextCurrent();

    bool initializeSharedGLObjects();
    
    // GL shader program object IDs.
    unsigned int m_layerProgramObject[LayerBaseGLES2::NumberOfLayerProgramShaders];
    unsigned int m_colorProgramObject;

    // Shader uniform and attribute locations.
    const int m_positionLocation;
    const int m_texCoordLocation;
    int m_samplerLocation[LayerBaseGLES2::NumberOfLayerProgramShaders];
    int m_alphaLocation[LayerBaseGLES2::NumberOfLayerProgramShaders];
    int m_colorColorLocation;

    // Current draw configuration
    FloatRect m_visibleRect;
    IntRect m_layoutRect;
    IntSize m_contentsSize;

    IntRect m_viewport;
    IntRect m_scissorRect;

    RefPtr<LayerGLES2> m_rootLayer;

    bool m_hardwareCompositing;

    // Map associating layers with textures ids used by the GL compositor.
    typedef HashSet<LayerGLES2*> LayerSet;
    LayerSet m_layers;
    LayerSet m_layersLockingTextureResources;

    OwnPtr<GLES2Context> m_gles2Context;

    // The WebCore Page that the compositor renders into.
    Page* m_page;

    LayerRenderingResults m_lastRenderingResults;
};

}

#endif // USE(ACCELERATED_COMPOSITING)

#endif
