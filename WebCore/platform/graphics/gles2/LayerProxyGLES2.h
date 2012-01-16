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


#ifndef LayerProxyGLES2_h
#define LayerProxyGLES2_h

#if USE(ACCELERATED_COMPOSITING)

#include "LayerBaseGLES2.h"
#include "GraphicsContext.h"
#include "GraphicsLayerGLES2.h"
#include "ImageData.h"

#include <wtf/OwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>


namespace skia {
class PlatformCanvas;
}

namespace WebCore {

class LayerGLES2;

class LayerProxyGLES2 : public RefCounted<LayerProxyGLES2>, public LayerBaseGLES2 {
public:
    static PassRefPtr<LayerProxyGLES2> create(LayerType, GraphicsLayerGLES2* owner = 0);

    ~LayerProxyGLES2();

    void display(PlatformGraphicsContext*);

    void addSublayer(PassRefPtr<LayerProxyGLES2>);
    void insertSublayer(PassRefPtr<LayerProxyGLES2>, size_t index);
    void replaceSublayer(LayerProxyGLES2* reference, PassRefPtr<LayerProxyGLES2> newLayer);
    void removeFromSuperlayer();

    void setAnchorPoint(const FloatPoint& anchorPoint) { m_anchorPoint = anchorPoint; setNeedsCommit(); }

    void setAnchorPointZ(float anchorPointZ) { m_anchorPointZ = anchorPointZ; setNeedsCommit(); }

    void setBackgroundColor(const Color& color) { m_backgroundColor = color; setNeedsCommit(); }

    void setBorderColor(const Color& color) { m_borderColor = color; setNeedsCommit(); }

    void setBorderWidth(float width) { m_borderWidth = width; setNeedsCommit(); }

    void setBounds(const IntSize&);

    void setClearsContext(bool clears) { m_clearsContext = clears; setNeedsCommit(); }

    void setContentsGravity(ContentsGravityType gravityType) { m_contentsGravity = gravityType; setNeedsCommit(); }

    void setDoubleSided(bool doubleSided) { m_doubleSided = doubleSided; setNeedsCommit(); }

    void setEdgeAntialiasingMask(uint32_t mask) { m_edgeAntialiasingMask = mask; setNeedsCommit(); }

    void setFrame(const FloatRect&);

    void setHidden(bool hidden) { m_hidden = hidden; setNeedsCommit(); }

    void setMasksToBounds(bool masksToBounds) { m_masksToBounds = masksToBounds; }

    void setName(const String& name) { m_name = name; }

    void setNeedsDisplay(const FloatRect& dirtyRect);
    void setNeedsDisplay();

    void setNeedsDisplayOnBoundsChange(bool needsDisplay) { m_needsDisplayOnBoundsChange = needsDisplay; }

    void setOpacity(float opacity) { m_opacity = opacity; setNeedsCommit(); }

    void setOpaque(bool opaque) { m_opaque = opaque; setNeedsCommit(); }

    void setPosition(const FloatPoint& position) { m_position = position;  setNeedsCommit(); }

    void setZPosition(float zPosition) { m_zPosition = zPosition; setNeedsCommit(); }

    const LayerProxyGLES2* rootLayer() const;

    void removeAllSublayers();

    void setSublayers(const Vector<RefPtr<LayerProxyGLES2> >&);

    const Vector<RefPtr<LayerProxyGLES2> >& getSublayers() const { return m_sublayers; }

    void setSublayerTransform(const TransformationMatrix& transform) { m_sublayerTransform = transform; setNeedsCommit(); }

    LayerProxyGLES2* superlayer() const;

    void setTransform(const TransformationMatrix& transform) { m_transform = transform; setNeedsCommit(); }

    void setGeometryFlipped(bool flipped) { m_geometryFlipped = flipped; setNeedsCommit(); }

    void setPreserves3D(bool preserves3D) { m_preserves3D = preserves3D; setNeedsCommit(); }

    void setFixedPosition(bool fixed) { m_isFixedPosition = fixed; setNeedsCommit(); }
    void setHasFixedContainer(bool fixed) { m_hasFixedContainer = fixed; setNeedsCommit(); }
    void setHasFixedAncestorInDOMTree(bool fixed) { m_hasFixedAncestorInDOMTree = fixed; setNeedsCommit(); }

    void updateTextureContents();
    bool contentsDirty() { return m_contentsDirty; }

    void setContents(NativeImagePtr contents);
    NativeImagePtr contents() const { return m_contents; }

    void setOwner(GraphicsLayerGLES2* owner) { m_owner = owner; }

    bool drawsContent() const { return m_drawsContent; }
    void setDrawsContent(bool drawsContent);

    void setPluginView(PluginView* pluginView);
#if ENABLE(VIDEO)
    void setMediaPlayer(MediaPlayer* mediaPlayer);
#endif
    void setHolePunchRect(const IntRect& rect) { m_holePunchRect = rect; setNeedsCommit(); }

    void commitOnWebKitThread();
    void commitOnCompositingThread();
    LayerGLES2* impl() const { return m_impl.get(); }

    // Only used when this layer is the root layer of a frame.
    void setAbsoluteOffset(const FloatSize& offset) { m_absoluteOffset = offset; }

private:
    LayerProxyGLES2(LayerType, GraphicsLayerGLES2* owner);

    void setNeedsTexture(bool needsTexture) { m_needsTexture = needsTexture; }
    void setLayerProgramShader(LayerBaseGLES2::LayerProgramShader shader) { m_layerProgramShader = shader; }

    PassRefPtr<ImageData> createImageDataByPaintingContents(const IntRect& dirtyRect);

    void updateLayerHierarchy();

    void setNeedsCommit();

    void setSuperlayer(LayerProxyGLES2* superlayer) { m_superlayer = superlayer; }

    size_t numSublayers() const
    {
        return m_sublayers.size();
    }

    // Returns the index of the sublayer or -1 if not found.
    int indexOfSublayer(const LayerProxyGLES2*);

    // This should only be called from removeFromSuperlayer.
    void removeSublayer(LayerProxyGLES2*);

    GraphicsLayerGLES2* m_owner;

    Vector<RefPtr<LayerProxyGLES2> > m_sublayers;
    LayerProxyGLES2* m_superlayer;

    NativeImagePtr m_contents;
    bool m_contentsDirty;
    IntSize m_allocatedTextureSize;
    FloatRect m_dirtyRect;

    RefPtr<LayerGLES2> m_impl;
    bool m_drawsContent;
    FloatSize m_absoluteOffset;
};

}

#endif // USE(ACCELERATED_COMPOSITING)

#endif
