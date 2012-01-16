/*
 * Copyright (C) 2010, 2011 Research In Motion Limited. All rights reserved.
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

#if USE(ACCELERATED_COMPOSITING)

#include "LayerRendererGLES2.h"

#include "GLES2Context.h"
#include "LayerGLES2.h"
#include "NotImplemented.h"
#include "Page.h"
#if PLATFORM(SKIA)
#include "NativeImageSkia.h"
#include "PlatformContextSkia.h"
#else
#include "ImageData.h"
#endif
#include <BlackBerryPlatformLog.h>

#include <GLES2/gl2.h>

#define ENABLE_SCISSOR 1

using namespace std;

namespace WebCore {

static void checkGLError()
{
#ifndef NDEBUG
    GLenum error = glGetError();
    if (error)
        LOG_ERROR("GL Error: 0x%x " , error);
#endif
}

static GLuint loadShader(GLenum type, const char* shaderSource)
{
    GLuint shader = glCreateShader(type);
    if (!shader)
        return 0;
    glShaderSource(shader, 1, &shaderSource, 0);
    glCompileShader(shader);
    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint loadShaderProgram(const char* vertexShaderSource, const char* fragmentShaderSource)
{
    GLuint vertexShader;
    GLuint fragmentShader;
    GLuint programObject;
    GLint linked;
    vertexShader = loadShader(GL_VERTEX_SHADER, vertexShaderSource);
    if (!vertexShader)
        return 0;
    fragmentShader = loadShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    if (!fragmentShader) {
        glDeleteShader(vertexShader);
        return 0;
    }
    programObject = glCreateProgram();
    if (!programObject)
        return 0;
    glAttachShader(programObject, vertexShader);
    glAttachShader(programObject, fragmentShader);
    glLinkProgram(programObject);
    glGetProgramiv(programObject, GL_LINK_STATUS, &linked);
    if (!linked) {
        glDeleteProgram(programObject);
        return 0;
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return programObject;
}

static TransformationMatrix orthoMatrix(float left, float right, float bottom, float top, float nearZ, float farZ)
{
    float deltaX = right - left;
    float deltaY = top - bottom;
    float deltaZ = farZ - nearZ;
    TransformationMatrix ortho;
    if (!deltaX || !deltaY || !deltaZ)
        return ortho;
    ortho.setM11(2.0f / deltaX);
    ortho.setM41(-(right + left) / deltaX);
    ortho.setM22(2.0f / deltaY);
    ortho.setM42(-(top + bottom) / deltaY);
    ortho.setM33(-2.0f / deltaZ);
    ortho.setM43(-(nearZ + farZ) / deltaZ);
    return ortho;
}

PassOwnPtr<LayerRendererGLES2> LayerRendererGLES2::create(Page* page)
{
    return new LayerRendererGLES2(page);
}

LayerRendererGLES2::LayerRendererGLES2(Page* page)
    : m_positionLocation(0)
    , m_texCoordLocation(1)
    , m_rootLayer(0)
    , m_page(page)
{
    for (int i = 0; i < LayerBaseGLES2::NumberOfLayerProgramShaders; ++i)
        m_layerProgramObject[i] = 0;

    m_hardwareCompositing = (initGL() && initializeSharedGLObjects());
}

LayerRendererGLES2::~LayerRendererGLES2()
{
    if (m_hardwareCompositing) {
        makeContextCurrent();
        glDeleteProgram(m_colorProgramObject);
        for (int i = 0; i < LayerBaseGLES2::NumberOfLayerProgramShaders; ++i)
            glDeleteProgram(m_layerProgramObject[i]);

        // Free up all GL textures.
        while (m_layers.begin() != m_layers.end()) {
            LayerSet::iterator iter = m_layers.begin();
            (*iter)->deleteTextures();
            (*iter)->setLayerRenderer(0);
            removeLayer(*iter);
        }
    }
}

static inline bool compareLayerZ(const LayerGLES2* a, const LayerGLES2* b)
{
    const TransformationMatrix& transformA = a->drawTransform();
    const TransformationMatrix& transformB = b->drawTransform();

    return transformA.m43() < transformB.m43();
}

// Re-composits all sublayers.
void LayerRendererGLES2::drawLayers(const FloatRect& visibleRect, const IntRect& layoutRect, const IntSize& contentsSize, const IntRect& dstRect)
{
    ASSERT(m_hardwareCompositing);

    bool wasEmpty = m_lastRenderingResults.isEmpty();
    m_lastRenderingResults = LayerRenderingResults();
    m_lastRenderingResults.wasEmpty = wasEmpty;

    if (!m_rootLayer)
        return;

    // These parameters are used to calculate position of fixed position elements
    m_visibleRect = visibleRect;
    m_layoutRect = layoutRect;
    m_contentsSize = contentsSize;

    // WebKit uses row vectors which are multiplied by the matrix on the left (i.e. v*M)
    // Transformations are composed on the left so that M1.xform(M2) means M2*M1
    // We therefore start with our (othogonal) projection matrix, which will be applied
    // as the last transformation. 
    TransformationMatrix matrix = orthoMatrix(0, visibleRect.width(), visibleRect.height(), 0, -1000, 1000);
    matrix.translate3d(-visibleRect.x(), -visibleRect.y(), 0);

    // OpenGL window coordinates origin is at the lower left corner of the surface while
    // WebKit uses upper left as the origin of the window coordinate system. The passed in 'dstRect'
    // is in WebKit window coordinate system. Here we setup the viewport to the corresponding value
    // in OpenGL window coordinates.
    int viewportY = std::max(0, m_gles2Context->surfaceSize().height() - dstRect.bottom());
    m_viewport = IntRect(dstRect.x(), viewportY, dstRect.width(), dstRect.height());

    const Vector<RefPtr<LayerGLES2> >& sublayers = m_rootLayer->getSublayers();
    for (size_t i = 0; i < sublayers.size(); i++) {
        float opacity = 1;
        FloatRect clipRect(-1, -1, 2, 2);
        updateLayersRecursive(sublayers[i].get(), matrix, opacity, clipRect);
    }

    // Decompose the dirty rect into a set of non-overlaping rectangles
    // (they need to not overlap so that the blending code doesn't draw any region twice)
    for (int i = 0; i < LayerRenderingResults::NUM_DIRTY_RECTS; ++i) {
        IntRectRegion region(m_lastRenderingResults.dirtyRect(i));
        m_lastRenderingResults.dirtyRegion = IntRectRegion::unionRegions(m_lastRenderingResults.dirtyRegion, region);
    }

    // If we won't draw anything, don't touch the OpenGL APIs.
    if (m_lastRenderingResults.isEmpty() && wasEmpty)
        return;

    // Okay, we're going to do some drawing
    makeContextCurrent();

    glViewport(m_viewport.x(), m_viewport.y(), m_viewport.width(), m_viewport.height());
    glEnableVertexAttribArray(m_positionLocation);
    glEnableVertexAttribArray(m_texCoordLocation);
    glActiveTexture(GL_TEXTURE0);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_STENCIL_TEST);

#if ENABLE_SCISSOR
    m_scissorRect = m_viewport;
    glEnable(GL_SCISSOR_TEST);
    glScissor(m_scissorRect.x(), m_scissorRect.y(), m_scissorRect.width(), m_scissorRect.height());
#endif

    glClearStencil(0);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // If culling is enabled then we will cull the backface.
    glCullFace(GL_BACK);
    // The orthographic projection is setup such that Y starts at zero and
    // increases going down the page which flips the winding order of triangles.
    // The layer quads are drawn in clock-wise order so the front face is CCW
    glFrontFace(GL_CCW);

    // The shader used to render layers returns pre-multiplied alpha colors
    // so we need to send the blending mode appropriately.
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    checkGLError();

    // Don't render the root layer, the BlackBerry port uses the BackingStore to draw the
    // root layer
    for (size_t i = 0; i < sublayers.size(); i++) {
        int currentStencilValue = 0;
        FloatRect clipRect(-1, -1, 2, 2);
        compositeLayersRecursive(sublayers[i].get(), currentStencilValue, clipRect);
    }

    glFlush();
    m_gles2Context->swapBuffers();

    LayerSet::iterator iter = m_layersLockingTextureResources.begin();
    for (; iter != m_layersLockingTextureResources.end(); ++iter)
        (*iter)->releaseTextureResources();

    m_layersLockingTextureResources.clear();
}

void LayerRendererGLES2::setRootLayer(LayerGLES2* layer)
{
    m_rootLayer = layer;
}

void LayerRendererGLES2::addLayer(LayerGLES2* layer)
{
    m_layers.add(layer);
}

bool LayerRendererGLES2::removeLayer(LayerGLES2* layer)
{
    LayerSet::iterator iter = m_layers.find(layer);
    if (iter == m_layers.end())
        return false;
    m_layers.remove(layer);
    return true;
}

void LayerRendererGLES2::addLayerToReleaseTextureResourcesList(LayerGLES2* layer)
{
    m_layersLockingTextureResources.add(layer);
}

// Transform normalized device coordinates to window coordinates
// as specified in the OpenGL ES 2.0 spec section 2.12.1
IntRect LayerRendererGLES2::toOpenGLWindowCoordinates(const FloatRect& r)
{
    float vw2 = m_viewport.width() / 2.0;
    float vh2 = m_viewport.height() / 2.0;
    float ox = m_viewport.x() + vw2;
    float oy = m_viewport.y() + vh2;
    return enclosingIntRect(FloatRect(r.x() * vw2 + ox, r.y() * vh2 + oy, r.width() * vw2, r.height() * vh2));
}

// Transform normalized device coordinates to window coordinates as WebKit understands them.
//
// The OpenGL surface may be larger than the WebKit window, and OpenGL window coordinates
// have origin in bottom left while WebKit window coordinates origin is in top left.
// The viewport is setup to cover the upper portion of the larger OpenGL surface.
IntRect LayerRendererGLES2::toWebKitWindowCoordinates(const FloatRect& r)
{
    float vw2 = m_viewport.width() / 2.0;
    float vh2 = m_viewport.height() / 2.0;
    float ox = m_viewport.x() + vw2;
    float oy = m_gles2Context->surfaceSize().height() - (m_viewport.y() + vh2);
    return enclosingIntRect(FloatRect(r.x() * vw2 + ox, -(r.y()+r.height()) * vh2 + oy, r.width() * vw2, r.height() * vh2));
}

// Draws a debug border around the layer's bounds.
void LayerRendererGLES2::drawDebugBorder(LayerGLES2* layer)
{
    Color borderColor = layer->borderColor();
    if (!borderColor.alpha())
        return;

    glUseProgram(m_colorProgramObject);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, &layer->getTransformedBounds());
    glUniform4f(m_colorColorLocation, borderColor.red() / 255.0,
                                      borderColor.green() / 255.0,
                                      borderColor.blue() / 255.0,
                                      1);

    glLineWidth(layer->borderWidth());
    glDrawArrays(GL_LINE_LOOP, 0, 4);
}

// Clears a rectangle inside the layer's bounds.
void LayerRendererGLES2::drawHolePunchRect(LayerGLES2* layer)
{
    glUseProgram(m_colorProgramObject);
    glUniform4f(m_colorColorLocation, 0, 0, 0, 0);

    glBlendFunc(GL_ONE, GL_ZERO);
    FloatQuad hole = layer->getTransformedHolePunchRect();
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, &hole);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    checkGLError();
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    IntRect holeWC = toWebKitWindowCoordinates(hole.boundingBox());
    m_lastRenderingResults.addHolePunchRect(holeWC);
}

void LayerRendererGLES2::updateLayersRecursive(LayerGLES2* layer, const TransformationMatrix& matrix, float opacity, FloatRect clipRect)
{

    // The contract for LayerGLES2::setLayerRenderer is it must be set if the layer has been rendered.
    // so do it now, before we render it in compositeLayersRecursive.
    layer->setLayerRenderer(this);

    // Compute the new matrix transformation that will be applied to this layer and
    // all its sublayers. It's important to remember that the layer's position
    // is the position of the layer's anchor point. Also, the coordinate system used
    // assumes that the origin is at the lower left even though the coordinates the browser
    // gives us for the layers are for the upper left corner. The Y flip happens via
    // the orthographic projection applied at render time.
    // The transformation chain for the layer is (using the Matrix x Vector order):
    // M = M[p] * Tr[l] * M[l] * Tr[c]
    // Where M[p] is the parent matrix passed down to the function
    //       Tr[l] is the translation matrix locating the layer's anchor point
    //       Tr[c] is the translation offset between the anchor point and the center of the layer
    //       M[l] is the layer's matrix (applied at the anchor point)
    // This transform creates a coordinate system whose origin is the center of the layer.
    // Note that the final matrix used by the shader for the layer is P * M * S . This final product
    // is computed in drawTexturedQuad().
    // Where: P is the projection matrix
    //        M is the layer's matrix computed above
    //        S is the scale adjustment (to scale up to the layer size)
    IntSize bounds = layer->bounds();
    FloatPoint anchorPoint = layer->anchorPoint();
    FloatPoint position = layer->position();

    // HasFixedContainer and HasFixedAncestorInDOMTree flags a mutually excluse: a Layer can
    // have none but it can not have both ...
    ASSERT(!(layer->hasFixedContainer() && layer->hasFixedAncestorInDOMTree()));

    if (layer->isFixedPosition() || layer->hasFixedAncestorInDOMTree()) {
        // The basic idea here is to set visible y to the value we want, and
        // layout y to the value WebCore layouted the fixed element to.
        float maximumScrollY = m_contentsSize.height() - m_visibleRect.height();
        float visibleY = max(0.0f, m_visibleRect.y());
        float layoutY = max(0.0f, min(maximumScrollY, (float)m_layoutRect.y()));

        // For stuff located on the lower half of the screen, we zoom relative to bottom.
        // This trick allows us to display fixed positioned elements aligned to top or
        // bottom correctly when panning and zooming, without actually knowing the
        // numeric values of the top and bottom CSS attributes.
        // In fact, the position is the location of the anchor, so to find the top left
        // we have to subtract the anchor times the bounds. The anchor defaults to
        // (0.5, 0.5) for most layers.
        if (position.y() - anchorPoint.y() * bounds.height() > layoutY + m_layoutRect.height()/2) {
            visibleY = min<float>(m_contentsSize.height(), m_visibleRect.y() + m_visibleRect.height());
            layoutY = min(m_contentsSize.height(), max(0, m_layoutRect.y()) + m_layoutRect.height());
        }

        position.setY(position.y() + (visibleY - layoutY));
    }

    // Offset between anchor point and the center of the quad.
    float centerOffsetX = (0.5 - anchorPoint.x()) * bounds.width();
    float centerOffsetY = (0.5 - anchorPoint.y()) * bounds.height();

    // M = M[p]
    TransformationMatrix localMatrix = matrix;
    // M = M[p] * Tr[l]
    localMatrix.translate3d(position.x(), position.y(), layer->anchorPointZ());
    // M = M[p] * Tr[l] * M[l]
    localMatrix.multLeft(layer->transform());
    // M = M[p] * Tr[l] * M[l] * Tr[c]
    localMatrix.translate3d(centerOffsetX, centerOffsetY, -layer->anchorPointZ());

    // Calculate the layer's opacity.
    opacity *= layer->opacity();

    layer->setDrawTransform(localMatrix);
    layer->setDrawOpacity(opacity);

#if ENABLE(VIDEO)
    bool layerVisible = clipRect.intersects(layer->getDrawRect()) || layer->mediaPlayer();
#else
    bool layerVisible = clipRect.intersects(layer->getDrawRect());
#endif

    if (layer->needsTexture() && layerVisible) {
        IntRect dirtyRect = toWebKitWindowCoordinates(intersection(layer->getDrawRect(), clipRect));
        m_lastRenderingResults.addDirtyRect(dirtyRect);
    }

    if (layer->masksToBounds())
        clipRect.intersect(layer->getDrawRect());

    // Flatten to 2D if the layer doesn't preserve 3D.
    if (!layer->preserves3D()) {
        localMatrix.setM13(0);
        localMatrix.setM23(0);
        localMatrix.setM31(0);
        localMatrix.setM32(0);
        // This corresponds to the depth range specified in the original orthographic projection matrix
        localMatrix.setM33(0.001);
        localMatrix.setM34(0);
        localMatrix.setM43(0);
    }

    // Apply the sublayer transform.
    localMatrix.multLeft(layer->sublayerTransform());

    // The origin of the sublayers is actually the bottom left corner of the layer
    // (or top left when looking it it from the browser's pespective) instead of the center.
    // The matrix passed down to the sublayers is therefore:
    // M[s] = M * Tr[-center]
    localMatrix.translate3d(-bounds.width() * 0.5, -bounds.height() * 0.5, 0);

    const Vector<RefPtr<LayerGLES2> >& sublayers = layer->getSublayers();
    for (size_t i = 0; i < sublayers.size(); i++) {
        updateLayersRecursive(sublayers[i].get(), localMatrix, opacity, clipRect);
    }
}

static bool hasRotationalComponent(const TransformationMatrix& m)
{
    return m.m12() != 0.0f || m.m13() != 0.0f || m.m23() != 0.0f ||
           m.m21() != 0.0f || m.m31() != 0.0f || m.m32() != 0.0f;
}

void LayerRendererGLES2::compositeLayersRecursive(LayerGLES2* layer, int stencilValue, FloatRect clipRect)
{
#if ENABLE(VIDEO)
    bool layerVisible = clipRect.intersects(layer->getDrawRect()) || layer->mediaPlayer();
#else
    bool layerVisible = clipRect.intersects(layer->getDrawRect());
#endif
    
    glStencilFunc(GL_EQUAL, stencilValue, 0xff);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

    // Note that there are two types of layers:
    // 1. Layers that have their own GraphicsContext and can draw their contents on demand (layer->drawsContent() == true).
    // 2. Layers that are just containers of images/video/etc that don't own a GraphicsContext (layer->contents() == true).
    if (layer->needsTexture() && layerVisible) {
        // Redraw the contents of the layer if necessary.
        if (layer->needsTexture() && layer->contentsDirty()) {
            // Update the contents of the layer before taking a snapshot. For layers that
            // are simply containers, the following call just clears the dirty flag but doesn't
            // actually do any draws/copies.
            layer->updateTextureContents(m_gles2Context.get());
        }

#if ENABLE_SCISSOR
        IntRect clipRectWC = toOpenGLWindowCoordinates(clipRect);
        if (m_scissorRect != clipRectWC) {
            m_scissorRect = clipRectWC;
            glScissor(m_scissorRect.x(), m_scissorRect.y(), m_scissorRect.width(), m_scissorRect.height());
        }
#endif

        if (layer->doubleSided())
            glDisable(GL_CULL_FACE);
        else
            glEnable(GL_CULL_FACE);

        if (layer->hasHolePunchRect())
            drawHolePunchRect(layer);

        LayerBaseGLES2::LayerProgramShader shader = layer->layerProgramShader();
        glUseProgram(m_layerProgramObject[shader]);

        glUniform1i(m_samplerLocation[shader], 0);
        glUniform1f(m_alphaLocation[shader], layer->drawOpacity());

        layer->drawTextures(m_positionLocation, m_texCoordLocation, m_visibleRect);
    }

    // Draw the debug border if there is one.
    drawDebugBorder(layer);

    // if we need to mask to bounds but the transformation has a rotational component
    // to it, scissoring is not enough and we need to use the stencil buffer for clipping
#if ENABLE_SCISSOR
    bool stencilClip = layer->masksToBounds() && hasRotationalComponent(layer->drawTransform());
#else
    bool stencilClip = layer->masksToBounds();
#endif

    if (stencilClip) {
        glStencilFunc(GL_EQUAL, stencilValue, 0xff);
        glStencilOp(GL_KEEP, GL_INCR, GL_INCR);
        
        glUseProgram(m_colorProgramObject);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, &layer->getTransformedBounds());
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    }

    if (layer->masksToBounds()) {
        clipRect.intersect(layer->getDrawRect());
    }

    const Vector<RefPtr<LayerGLES2> >& sublayers = layer->getSublayers();
    Vector<LayerGLES2*> sublayerList;
    size_t i;

    for (i = 0; i < sublayers.size(); i++)
        sublayerList.append(sublayers[i].get());

    if (layer->preserves3D())
        std::stable_sort(sublayerList.begin(), sublayerList.end(), compareLayerZ);

    int newStencilValue = stencilClip ? stencilValue+1 : stencilValue;
    for (i = 0; i < sublayerList.size(); i++)
        compositeLayersRecursive(sublayerList[i], newStencilValue, clipRect);

    if (stencilClip) {
        glStencilFunc(GL_LEQUAL, stencilValue, 0xff);
        glStencilOp(GL_KEEP, GL_REPLACE, GL_REPLACE);
        
        glUseProgram(m_colorProgramObject);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, &layer->getTransformedBounds());
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    }
}

bool LayerRendererGLES2::makeContextCurrent()
{
    return m_gles2Context->makeCurrent();
}

bool LayerRendererGLES2::initGL()
{
    m_gles2Context = GLES2Context::create(m_page);

    if (!m_gles2Context)
        return false;

    return true;
}

// Binds the given attribute name to a common location across all three programs
// used by the compositor. This allows the code to bind the attributes only once
// even when switching between programs.
void LayerRendererGLES2::bindCommonAttribLocation(int location, const char* attribName)
{
    for (int i = 0; i < LayerBaseGLES2::NumberOfLayerProgramShaders; ++i)
        glBindAttribLocation(m_layerProgramObject[i], location, attribName);

    glBindAttribLocation(m_colorProgramObject, location, attribName);
}

bool LayerRendererGLES2::initializeSharedGLObjects()
{
    // Shaders for drawing the layer contents.
    char vertexShaderString[] =
        "attribute vec4 a_position;   \n"
        "attribute vec2 a_texCoord;   \n"
        "varying vec2 v_texCoord;     \n"
        "void main()                  \n"
        "{                            \n"
        "  gl_Position = a_position;  \n"
        "  v_texCoord = a_texCoord;   \n"
        "}                            \n";
    char fragmentShaderStringRGBA[] =
        "varying mediump vec2 v_texCoord;                           \n"
        "uniform lowp sampler2D s_texture;                          \n"
        "uniform lowp float alpha;                                  \n"
        "void main()                                                \n"
        "{                                                          \n"
        "  gl_FragColor = texture2D(s_texture, v_texCoord) * alpha; \n"
        "}                                                          \n";
    char fragmentShaderStringBGRA[] =
        "varying mediump vec2 v_texCoord;                                \n"
        "uniform lowp sampler2D s_texture;                               \n"
        "uniform lowp float alpha;                                       \n"
        "void main()                                                     \n"
        "{                                                               \n"
        "  gl_FragColor = texture2D(s_texture, v_texCoord).bgra * alpha; \n"
        "}                                                               \n";

    // Shaders for drawing the debug borders around the layers.
    char colorVertexShaderString[] =
        "attribute vec4 a_position;   \n"
        "void main()                  \n"
        "{                            \n"
        "   gl_Position = a_position; \n"
        "}                            \n";
    char colorFragmentShaderString[] =
        "uniform lowp vec4 color;     \n"
        "void main()                  \n"
        "{                            \n"
        "  gl_FragColor = color;      \n"
        "}                            \n";

    makeContextCurrent();

    m_layerProgramObject[LayerBaseGLES2::LayerProgramShaderRGBA] =
        loadShaderProgram(vertexShaderString, fragmentShaderStringRGBA);
    if (!m_layerProgramObject[LayerBaseGLES2::LayerProgramShaderRGBA]) {
        LOG_ERROR("Failed to create shader program for RGBA layers");
        return false;
    }

    m_layerProgramObject[LayerBaseGLES2::LayerProgramShaderBGRA] =
        loadShaderProgram(vertexShaderString, fragmentShaderStringBGRA);
    if (!m_layerProgramObject[LayerBaseGLES2::LayerProgramShaderBGRA]) {
        LOG_ERROR("Failed to create shader program for BGRA layers");
        return false;
    }

    m_colorProgramObject = loadShaderProgram(colorVertexShaderString, colorFragmentShaderString);
    if (!m_colorProgramObject) {
        LOG_ERROR("Failed to create shader program for debug borders");
        return false;
    }

    // Specify the attrib location for the position and make it the same for all three programs to
    // avoid binding re-binding the vertex attributes.
    bindCommonAttribLocation(m_positionLocation, "a_position");
    bindCommonAttribLocation(m_texCoordLocation, "a_texCoord");

    checkGLError();

    // Re-link the shaders to get the new attrib location to take effect.
    for (int i = 0; i < LayerBaseGLES2::NumberOfLayerProgramShaders; ++i)
        glLinkProgram(m_layerProgramObject[i]);

    glLinkProgram(m_colorProgramObject);

    checkGLError();

    // Get locations of uniforms for the layer content shader program.
    for (int i = 0; i < LayerBaseGLES2::NumberOfLayerProgramShaders; ++i) {
        m_samplerLocation[i] = glGetUniformLocation(m_layerProgramObject[i], "s_texture");
        m_alphaLocation[i] = glGetUniformLocation(m_layerProgramObject[i], "alpha");
    }

    // Get locations of uniforms for the debug border shader program.
    m_colorColorLocation = glGetUniformLocation(m_colorProgramObject, "color");

    return true;
}

IntRect LayerRenderingResults::holePunchRect(int index) const {
    if (index >= m_holePunchRects.size())
        return IntRect();

    return m_holePunchRects.at(index);
}

void LayerRenderingResults::addHolePunchRect(const IntRect& rect)
{
    if (!rect.isEmpty())
        m_holePunchRects.append(rect);
}

void LayerRenderingResults::addDirtyRect(const IntRect& rect)
{
    IntRect dirtyUnion[NUM_DIRTY_RECTS];
    int smallestIncrease = INT_MAX;
    int modifiedRect = 0;
    for (int i = 0; i < NUM_DIRTY_RECTS; ++i) {
        dirtyUnion[i] = m_dirtyRects[i];
        dirtyUnion[i].unite(rect);
        int increase = dirtyUnion[i].width()*dirtyUnion[i].height() - m_dirtyRects[i].width()*m_dirtyRects[i].height();
        if (increase < smallestIncrease) {
            smallestIncrease = increase;
            modifiedRect = i;
        }
    }

    m_dirtyRects[modifiedRect] = dirtyUnion[modifiedRect];
}

bool LayerRenderingResults::isEmpty()
{
    for (int i = 0; i < NUM_DIRTY_RECTS; ++i) {
        if (!m_dirtyRects[i].isEmpty())
            return false;
    }
    return true;
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
