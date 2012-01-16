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


#ifndef LayerBaseGLES2_h
#define LayerBaseGLES2_h

#include "Color.h"
#include "FloatPoint.h"
#include "FloatRect.h"
#include "IntRect.h"
#include "PlatformString.h"
#include "TransformationMatrix.h"

#if USE(ACCELERATED_COMPOSITING)

namespace WebCore {

class PluginView;
#if ENABLE(VIDEO)
class MediaPlayer;
#endif

class LayerBaseGLES2 {
public:
    enum LayerType { Layer, TransformLayer };
    enum FilterType { Linear, Nearest, Trilinear, Lanczos };
    enum ContentsGravityType { Center, Top, Bottom, Left, Right, TopLeft, TopRight,
                               BottomLeft, BottomRight, Resize, ResizeAspect, ResizeAspectFill };
    enum LayerProgramShader { LayerProgramShaderRGBA = 0,
                              LayerProgramShaderBGRA,
                              NumberOfLayerProgramShaders };

    LayerBaseGLES2(LayerType type)
        : m_layerType(type)
        , m_anchorPoint(0.5, 0.5)
        , m_backgroundColor(0, 0, 0, 0)
        , m_borderColor(0, 0, 0, 0)
        , m_edgeAntialiasingMask(0)
        , m_opacity(1.0)
        , m_zPosition(0.0)
        , m_anchorPointZ(0.0)
        , m_borderWidth(0.0)
        , m_clearsContext(false)
        , m_doubleSided(true)
        , m_hidden(false)
        , m_masksToBounds(false)
        , m_opaque(true)
        , m_geometryFlipped(false)
        , m_preserves3D(false)
        , m_needsDisplayOnBoundsChange(false)
        , m_contentsGravity(Bottom)
        , m_layerProgramShader(LayerProgramShaderRGBA)
        , m_needsTexture(false)
        , m_isFixedPosition(false)
        , m_hasFixedContainer(false)
        , m_hasFixedAncestorInDOMTree(false)
        , m_pluginView(0)
#if ENABLE(VIDEO)
        , m_mediaPlayer(0)
#endif
    {
    }

    FloatPoint anchorPoint() const { return m_anchorPoint; }

    float anchorPointZ() const { return m_anchorPointZ; }

    Color backgroundColor() const { return m_backgroundColor; }

    Color borderColor() const { return m_borderColor; }

    float borderWidth() const { return m_borderWidth; }

    IntSize bounds() const { return m_bounds; }

    bool clearsContext() const { return m_clearsContext; }

    ContentsGravityType contentsGravity() const { return m_contentsGravity; }

    bool doubleSided() const { return m_doubleSided; }

    uint32_t edgeAntialiasingMask() const { return m_edgeAntialiasingMask; }

    FloatRect frame() const { return m_frame; }

    bool isHidden() const { return m_hidden; }

    bool masksToBounds() const { return m_masksToBounds; }

    String name() const { return m_name; }

    float opacity() const { return m_opacity; }

    bool opaque() const { return m_opaque; }

    FloatPoint position() const { return m_position; }

    float zPosition() const {  return m_zPosition; }

    const TransformationMatrix& sublayerTransform() const { return m_sublayerTransform; }

    const TransformationMatrix& transform() const { return m_transform; }

    bool geometryFlipped() const { return m_geometryFlipped; }

    bool preserves3D() const { return m_preserves3D; }

    bool needsTexture() const { return m_needsTexture; }

    LayerProgramShader layerProgramShader() const { return m_layerProgramShader; }

    bool isFixedPosition() const { return m_isFixedPosition; }
    bool hasFixedContainer() const { return m_hasFixedContainer; }
    bool hasFixedAncestorInDOMTree() const { return m_hasFixedAncestorInDOMTree; }

    PluginView* pluginView() const { return m_pluginView; }

    IntRect holePunchRect() const { return m_holePunchRect; }
    bool hasHolePunchRect() const { return !m_holePunchRect.isEmpty(); }

#if ENABLE(VIDEO)
    MediaPlayer* mediaPlayer() const { return m_mediaPlayer; }
#endif

    /* THIS FUNCTION MUST BE UPDATED AS MEMBER VARIABLES ARE ADDED OR CHANGED */
    void replicate(LayerBaseGLES2 *to) const
    {
        to->m_layerType = m_layerType;

        to->m_bounds = m_bounds;
        to->m_backingStoreSize = m_backingStoreSize;
        to->m_position = m_position;
        to->m_anchorPoint = m_anchorPoint;
        to->m_backgroundColor = m_backgroundColor;
        to->m_borderColor = m_borderColor;

        to->m_frame = m_frame;
        to->m_transform = m_transform;
        to->m_sublayerTransform = m_sublayerTransform;

        to->m_edgeAntialiasingMask = m_edgeAntialiasingMask;
        to->m_opacity = m_opacity;
        to->m_zPosition = m_zPosition;
        to->m_anchorPointZ = m_anchorPointZ;
        to->m_borderWidth = m_borderWidth;

        to->m_clearsContext = m_clearsContext;
        to->m_doubleSided = m_doubleSided;
        to->m_hidden = m_hidden;
        to->m_masksToBounds = m_masksToBounds;
        to->m_opaque = m_opaque;
        to->m_geometryFlipped = m_geometryFlipped;
        to->m_preserves3D = m_preserves3D;
        to->m_needsDisplayOnBoundsChange = m_needsDisplayOnBoundsChange;

        to->m_contentsGravity = m_contentsGravity;
        to->m_name = m_name.threadsafeCopy();

        to->m_layerProgramShader = m_layerProgramShader;
        to->m_needsTexture = m_needsTexture;
        to->m_isFixedPosition = m_isFixedPosition;
        to->m_hasFixedContainer = m_hasFixedContainer;
        to->m_hasFixedAncestorInDOMTree = m_hasFixedAncestorInDOMTree;

        to->m_pluginView = m_pluginView;
#if ENABLE(VIDEO)
        to->m_mediaPlayer = m_mediaPlayer;
#endif
        to->m_holePunchRect = m_holePunchRect;
    }

protected:
    LayerType m_layerType;

    IntSize m_bounds;
    IntSize m_backingStoreSize;
    FloatPoint m_position;
    FloatPoint m_anchorPoint;
    Color m_backgroundColor;
    Color m_borderColor;

    FloatRect m_frame;
    TransformationMatrix m_transform;
    TransformationMatrix m_sublayerTransform;

    uint32_t m_edgeAntialiasingMask;
    float m_opacity;
    float m_zPosition;
    float m_anchorPointZ;
    float m_borderWidth;

    bool m_clearsContext;
    bool m_doubleSided;
    bool m_hidden;
    bool m_masksToBounds;
    bool m_opaque;
    bool m_geometryFlipped;
    bool m_preserves3D;
    bool m_needsDisplayOnBoundsChange;

    ContentsGravityType m_contentsGravity;
    String m_name;

    LayerProgramShader m_layerProgramShader;
    bool m_needsTexture;
    bool m_isFixedPosition;
    bool m_hasFixedContainer;
    bool m_hasFixedAncestorInDOMTree;

    PluginView* m_pluginView;
#if ENABLE(VIDEO)
    MediaPlayer* m_mediaPlayer;
#endif
    IntRect m_holePunchRect;

    /***** If you change this class's members, update replicate() above!! *****/

};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)

#endif // LayerBaseGLES2_h
