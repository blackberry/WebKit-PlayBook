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

#ifndef GraphicsLayerGLES2_h
#define GraphicsLayerGLES2_h

#if USE(ACCELERATED_COMPOSITING)

#include "GraphicsContext.h"
#include "GraphicsLayer.h"

namespace WebCore {

class LayerProxyGLES2;

class GraphicsLayerGLES2 : public GraphicsLayer {
public:
    GraphicsLayerGLES2(GraphicsLayerClient*);
    virtual ~GraphicsLayerGLES2();

    virtual void setName(const String&);

    // for hosting this GraphicsLayer in a native layer hierarchy
    virtual NativeLayer nativeLayer() const;

    virtual bool setChildren(const Vector<GraphicsLayer*>&);
    virtual void addChild(GraphicsLayer*);
    virtual void addChildAtIndex(GraphicsLayer*, int index);
    virtual void addChildAbove(GraphicsLayer*, GraphicsLayer* sibling);
    virtual void addChildBelow(GraphicsLayer*, GraphicsLayer* sibling);
    virtual bool replaceChild(GraphicsLayer* oldChild, GraphicsLayer* newChild);

    virtual void removeFromParent();

    virtual void setPosition(const FloatPoint&);
    virtual void setAnchorPoint(const FloatPoint3D&);
    virtual void setSize(const FloatSize&);

    virtual void setTransform(const TransformationMatrix&);

    virtual void setChildrenTransform(const TransformationMatrix&);

    virtual void setPreserves3D(bool);
    virtual void setMasksToBounds(bool);
    virtual void setDrawsContent(bool);
    virtual void setFixedPosition(bool);
    virtual void setHasFixedContainer(bool);
    virtual void setHasFixedAncestorInDOMTree(bool);

    virtual void setBackgroundColor(const Color&);
    virtual void clearBackgroundColor();

    virtual void setContentsOpaque(bool);
    virtual void setBackfaceVisibility(bool);

    virtual void setOpacity(float);

    virtual void setNeedsDisplay();
    virtual void setNeedsDisplayInRect(const FloatRect&);

    virtual void setContentsNeedsDisplay();

    virtual void setContentsRect(const IntRect&);

    virtual void setContentsToImage(Image*);
    virtual void setContentsToMedia(PlatformLayer*);
    virtual void setContentsToCanvas(PlatformLayer*);

    virtual PlatformLayer* platformLayer() const;

    virtual void setDebugBackgroundColor(const Color&);
    virtual void setDebugBorder(const Color&, float borderWidth);

    void notifySyncRequired()
    {
        if (m_client)
            m_client->notifySyncRequired(this);
    }

private:
    void updateOpacityOnLayer();

    LayerProxyGLES2* primaryLayer() const  { return m_transformLayer.get() ? m_transformLayer.get() : m_layer.get(); }
    LayerProxyGLES2* hostLayerForSublayers() const;
    LayerProxyGLES2* layerForSuperlayer() const;

    void updateSublayerList();
    void updateLayerPosition();
    void updateLayerSize();
    void updateAnchorPoint();
    void updateTransform();
    void updateChildrenTransform();
    void updateMasksToBounds();
    void updateContentsOpaque();
    void updateBackfaceVisibility();
    void updateLayerPreserves3D();
    void updateLayerDrawsContent();
    void updateFixedPosition();
    void updateHasFixedContainer();
    void updateHasFixedAncestorInDOMTree();
    void updateLayerBackgroundColor();

    void updateContentsImage();
    void updateContentsVideo();
    void updateContentsRect();

    void setupContentsLayer(LayerProxyGLES2*);
    LayerProxyGLES2* contentsLayer() const { return m_contentsLayer.get(); }

    RefPtr<LayerProxyGLES2> m_layer;
    RefPtr<LayerProxyGLES2> m_transformLayer;
    RefPtr<LayerProxyGLES2> m_contentsLayer;

    enum ContentsLayerPurpose {
        NoContentsLayer = 0,
        ContentsLayerForImage,
        ContentsLayerForVideo,
        ContentsLayerForCanvas,
    };

    ContentsLayerPurpose m_contentsLayerPurpose;
    bool m_contentsLayerHasBackgroundColor : 1;

    NativeImagePtr m_pendingContentsImage;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)

#endif
