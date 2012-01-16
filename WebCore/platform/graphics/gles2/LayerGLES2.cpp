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

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "LayerGLES2.h"

#include "GLES2Context.h"
#include "LayerMessage.h"
#include "LayerRendererGLES2.h"
#if ENABLE(VIDEO)
#include "MediaPlayer.h"
#include "MediaPlayerPrivateMMrenderer.h"
#endif
#include "PluginView.h"

#include <GLES2/gl2.h>
#include <BlackBerryPlatformGraphics.h>

#define MAX_TEXTURE_SIZE 2048

namespace WebCore {

using namespace std;

const int bytesPerPixel = 4;

static void copyImageData(unsigned char* dst, int dstStride, int dstX, int dstY,
                    const unsigned char* src, int srcStride, int srcX, int srcY, int width, int height)
{
    dst += (dstY * dstStride) + (bytesPerPixel * dstX);
    src += (srcY * srcStride) + (bytesPerPixel * srcX);
    int length = bytesPerPixel * width;

    for (int y = 0; y < height; ++y) {
        memcpy(dst, src, length);
        dst += dstStride;
        src += srcStride;
    }
}

PassRefPtr<LayerGLES2> LayerGLES2::create(LayerType type)
{
    return adoptRef(new LayerGLES2(type));
}

LayerGLES2::LayerGLES2(LayerType type)
    : LayerBaseGLES2(type)
    , m_layerRenderer(0)
    , m_superlayer(0)
    , m_contentsDirty(false)
    , m_pluginBuffer(0)
    , m_drawOpacity(0)
    , m_texturesWereDeleted(false)
{
}

LayerGLES2::~LayerGLES2()
{
    // Unfortunately, ThreadSafeShared<T> is hardwired to call T::~T().
    // To switch threads in case the last reference is released on the
    // WebKit thread, we send a sync message to the compositing thread.
    destroyOnCompositingThread();
}

void LayerGLES2::destroyOnCompositingThread()
{
    if (!isCompositingThread()) {
        dispatchSyncCompositingMessage(Olympia::Platform::createMessage0(&LayerGLES2::destroyOnCompositingThread, this));
        return;
    }

    ASSERT(!superlayer());

    // Remove the superlayer reference from all sublayers.
    while (m_sublayers.size())
        m_sublayers[0]->removeFromSuperlayer();

    // Delete all allocated textures
    deleteTextures();

    // We just deleted all our textures, no need for the
    // layer renderer to track us anymore
    if (m_layerRenderer)
        m_layerRenderer->removeLayer(this);
}

void LayerGLES2::setLayerRenderer(LayerRendererGLES2* renderer)
{
    // It's not expected that layers will ever switch renderers.
    ASSERT(!renderer || !m_layerRenderer || renderer == m_layerRenderer);

    m_layerRenderer = renderer;
    if (m_layerRenderer)
        m_layerRenderer->addLayer(this);
}

void LayerGLES2::deleteTextures()
{
    releaseTextureResources();

    if (m_allocatedTextureIds.size()) {
        // At this point there must be a layer renderer. If we were ever rendered, we may have
        // textures, and the layer renderer must be set
        ASSERT(m_layerRenderer);
        m_layerRenderer->context()->makeCurrent();
        glDeleteTextures(m_allocatedTextureIds.size(), m_allocatedTextureIds.data());
        m_allocatedTextureIds.clear();
        m_texturesWereDeleted = true;
    }

    // For various reasons, e.g. page cache, someone may try
    // to render us after the textures were deleted.
    m_allocatedTextureSize = IntSize();
}

bool LayerGLES2::texturesWereDeleted() const
{
    return m_texturesWereDeleted;
}

void LayerGLES2::setDrawTransform(const TransformationMatrix& matrix)
{
    m_drawTransform = matrix;

    float bx = m_bounds.width()/2.0;
    float by = m_bounds.height()/2.0;
    m_transformedBounds.setP1(matrix.mapPoint(FloatPoint(-bx, -by)));
    m_transformedBounds.setP2(matrix.mapPoint(FloatPoint(-bx, by)));
    m_transformedBounds.setP3(matrix.mapPoint(FloatPoint(bx, by)));
    m_transformedBounds.setP4(matrix.mapPoint(FloatPoint(bx, -by)));

    m_drawRect = m_transformedBounds.boundingBox();
}

FloatQuad LayerGLES2::getTransformedHolePunchRect() const
{
    float x = -m_bounds.width()/2.0 + m_holePunchRect.x();
    float y = -m_bounds.height()/2.0 + m_holePunchRect.y();
    float w = m_holePunchRect.width();
    float h = m_holePunchRect.height();
    FloatQuad result;
    result.setP1(m_drawTransform.mapPoint(FloatPoint(x, y)));
    result.setP2(m_drawTransform.mapPoint(FloatPoint(x, y+h)));
    result.setP3(m_drawTransform.mapPoint(FloatPoint(x+w, y+h)));
    result.setP4(m_drawTransform.mapPoint(FloatPoint(x+w, y)));

    return result;
}

static void transformPoint(float x, float y, const TransformationMatrix& m, float* result)
{
    result[0] = x * m.m11() + y * m.m21() + m.m41();
    result[1] = x * m.m12() + y * m.m22() + m.m42();
    result[2] = x * m.m13() + y * m.m23() + m.m43();
    result[3] = x * m.m14() + y * m.m24() + m.m44();
}

void LayerGLES2::drawTextures(int positionLocation, int texCoordLocation, const FloatRect& visibleRect)
{
    float texcoords[4 * 2] = { 0, 0,  0, 1,  1, 1,  1, 0 };
    float vertices[4 * 4];

#if PLATFORM(BLACKBERRY) && OS(QNX)
    if (m_pluginView) {
        // The layer contains Flash, video, or other plugin contents.
        m_pluginBuffer = m_pluginView->lockFrontBufferForRead();

        if (!m_pluginBuffer)
            return;

        if (!Olympia::Platform::Graphics::lockAndBindBufferGLTexture(m_pluginBuffer, GL_TEXTURE_2D)) {
            m_pluginView->unlockFrontBuffer();
            return;
        }

        m_layerRenderer->addLayerToReleaseTextureResourcesList(this);

        glVertexAttribPointer(positionLocation, 2, GL_FLOAT, GL_FALSE, 0, &m_transformedBounds);
        glVertexAttribPointer(texCoordLocation, 2, GL_FLOAT, GL_FALSE, 0, texcoords);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        return;
    }
#if ENABLE(VIDEO)
    else if (m_mediaPlayer) {
        // We need to specify the media player location in contents coordinates. The 'visibleRect'
        // specifies the content region covered by our viewport. So we transform from our
        // normalized device coordinates [-1, 1] to the 'visibleRect'.
        float vrw2 = visibleRect.width() / 2.0;
        float vrh2 = visibleRect.height() / 2.0;
        float x = m_transformedBounds.p1().x() * vrw2 + vrw2 + visibleRect.x();
        float y = -m_transformedBounds.p1().y() * vrh2 + vrh2 + visibleRect.y();
        m_mediaPlayer->paint(0, IntRect((int)(x+0.5), (int)(y+0.5), m_bounds.width(), m_bounds.height()));
        MediaPlayerPrivate *mpp = static_cast<MediaPlayerPrivate *>(m_mediaPlayer->platformMedia().media.qnxMediaPlayer);
        mpp->drawBufferingAnimation(m_drawTransform, positionLocation, texCoordLocation);
        return;
    }
#endif
#endif

    glVertexAttribPointer(positionLocation, 4, GL_FLOAT, GL_FALSE, 0, vertices);
    glVertexAttribPointer(texCoordLocation, 2, GL_FLOAT, GL_FALSE, 0, texcoords);
    int t = 0;
    for (int x = 0; x < m_allocatedTextureSize.width(); x += MAX_TEXTURE_SIZE) {
        for (int y = 0; y < m_allocatedTextureSize.height(); y += MAX_TEXTURE_SIZE) {
            int w = min(m_allocatedTextureSize.width() - x, MAX_TEXTURE_SIZE);
            int h = min(m_allocatedTextureSize.height() - y, MAX_TEXTURE_SIZE);
            float ox = x - m_allocatedTextureSize.width()/2.0;
            float oy = y - m_allocatedTextureSize.height()/2.0;

            // We apply the transformation by hand, since we need the z coordinate
            // as well (to do perspective correct texturing) and we don't need
            // to divide by w by hand, the GPU will do that for us
            transformPoint(ox, oy, m_drawTransform, &vertices[0]);
            transformPoint(ox, oy+h, m_drawTransform, &vertices[4]);
            transformPoint(ox+w, oy+h, m_drawTransform, &vertices[8]);
            transformPoint(ox+w, oy, m_drawTransform, &vertices[12]);

            glBindTexture(GL_TEXTURE_2D, m_allocatedTextureIds[t++]);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        }
    }
}

void LayerGLES2::releaseTextureResources()
{
    if (m_pluginView && m_pluginBuffer) {
        Olympia::Platform::Graphics::releaseBufferGLTexture(m_pluginBuffer);
        m_pluginBuffer = 0;
        m_pluginView->unlockFrontBuffer();
    }
}

void LayerGLES2::updateTileContents(const IntRect& tile)
{
    ASSERT(m_contents);
    if (!m_contents)
        return;

    if (pluginView())
        return; // texture comes from the buffer

    IntRect dirtyRect(m_dirtyRect);
    IntSize requiredTextureSize;

    requiredTextureSize = m_requiredTextureSize.shrunkTo(tile.size());
    if (m_requiredTextureSize != m_allocatedTextureSize)
        dirtyRect = IntRect(IntPoint(), m_requiredTextureSize);
    dirtyRect.intersect(tile);
    if (dirtyRect.isEmpty())
        return;

    int stride = bytesPerPixel * m_contents->width();
    int x = dirtyRect.x() - m_dirtyRect.x();
    int y = dirtyRect.y() - m_dirtyRect.y();
    unsigned char* pixels = m_contents->data()->data()->data();
    pixels += (y * stride) + (bytesPerPixel * x);
    if (pixels) {
        bool subImage = (m_requiredTextureSize == m_allocatedTextureSize);
        IntSize size = subImage ? dirtyRect.size() : requiredTextureSize;

        if (m_contents->width() != size.width()) {
#if defined(GL_UNPACK_ROW_LENGTH)
            glPixelStorei(GL_UNPACK_ROW_LENGTH, m_contents->width());
#else
            unsigned char* tmp = static_cast<unsigned char*>(malloc(size.width() * size.height() * bytesPerPixel));
            copyImageData(tmp, bytesPerPixel * size.width(), 0, 0,
                          pixels, stride, 0, 0,
                          std::min((int)m_contents->width(), size.width()), size.height());
            pixels = tmp;
#endif
        }

        if (subImage) {
            glTexSubImage2D(GL_TEXTURE_2D, 0 /*level*/,
                            dirtyRect.x() - tile.x(), dirtyRect.y() - tile.y(),
                            size.width(), size.height(),
                            GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        } else {
            glTexImage2D(GL_TEXTURE_2D, 0 /*level*/, GL_RGBA /*internal*/,
                         size.width(), size.height(),
                         0 /*border*/, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        }

        if (m_contents->width() != size.width()) {
#if defined(GL_UNPACK_ROW_LENGTH)
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#else
            free(pixels);
#endif
        }
    }
}

static unsigned int allocateTextureId()
{
    unsigned int texid;
    glGenTextures(1, &texid);
    glBindTexture(GL_TEXTURE_2D, texid);
    // Do basic linear filtering on resize.
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // NPOT textures in GL ES only work when the wrap mode is set to GL_CLAMP_TO_EDGE.
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return texid;
}

void LayerGLES2::updateTextureContents(GLES2Context* context)
{
    ASSERT(context);
    context->makeCurrent();

    if (m_contents) {
        unsigned int tileNum = 0;
        IntRect tile;
        for (tile.setX(0); tile.x() < m_requiredTextureSize.width(); tile.setX(tile.x() + MAX_TEXTURE_SIZE)) {
            for (tile.setY(0); tile.y() < m_requiredTextureSize.height(); tile.setY(tile.y() + MAX_TEXTURE_SIZE)) {
                tile.setWidth(min(m_requiredTextureSize.width() - tile.x(), MAX_TEXTURE_SIZE));
                tile.setHeight(min(m_requiredTextureSize.height() - tile.y(), MAX_TEXTURE_SIZE));

                if (m_allocatedTextureIds.size() <= tileNum)
                    m_allocatedTextureIds.append(allocateTextureId());

                glBindTexture(GL_TEXTURE_2D, m_allocatedTextureIds[tileNum]);

                updateTileContents(tile);

                ++tileNum;
            }
        }

        m_contents.clear();
        m_texturesWereDeleted = false;
    }

    m_allocatedTextureSize = m_requiredTextureSize;

    m_contentsDirty = false;
    m_dirtyRect.setSize(FloatSize());
}

void LayerGLES2::invokeSetNeedsDisplay(PassRefPtr<ImageData> contents, FloatRect dirtyRect, IntSize requiredTextureSize)
{
    setNeedsDisplay(contents, dirtyRect, requiredTextureSize);
}

void LayerGLES2::setNeedsDisplay(PassRefPtr<ImageData> contents, const FloatRect& dirtyRect, const IntSize& requiredTextureSize)
{
    if (!isCompositingThread()) {
        dispatchCompositingMessage(Olympia::Platform::createMessage3(&LayerGLES2::invokeSetNeedsDisplay,
                                                                     this,
                                                                     contents, dirtyRect, requiredTextureSize));
        return;
    }

    // Handle the case where we need display because we no longer need to be displayed,
    // due to texture size becoming 0 x 0
    if (requiredTextureSize.isEmpty()) {
        m_dirtyRect = IntRect();
        m_contents = 0;
        m_contentsDirty = false;
        m_requiredTextureSize = requiredTextureSize;
        return;
    }

    // If the dirty area is empty, there's nothing to do
    if (dirtyRect.isEmpty())
        return;

    // In order to display, we will need a texture.
    ASSERT(contents);

    // Detect if we missed an updateTextureContents().
    // If the contents are still dirty, and this is is not an update of the whole texture
    // we need to draw the new contents into the old
    if (m_contentsDirty && (dirtyRect != IntRect(IntPoint(), requiredTextureSize))) {
        ASSERT(m_contents);

        // if there's no layer renderer yet, we have never been rendered, and
        // we have no choice but to drop this update
        if (!m_layerRenderer)
            return;

        updateTextureContents(m_layerRenderer->context());
    }

    m_dirtyRect = dirtyRect;
    m_contents = contents;
    m_contentsDirty = true;
    m_requiredTextureSize = requiredTextureSize;
}

void LayerGLES2::setPluginView(PluginView* pluginView)
{
    if (!isCompositingThread()) {
        dispatchSyncCompositingMessage(Olympia::Platform::createMessage1(&LayerGLES2::setPluginView, this,
                                                                         pluginView));
        return;
    }

    m_pluginView = pluginView;
}

#if ENABLE(VIDEO)
void LayerGLES2::setMediaPlayer(MediaPlayer* mediaPlayer)
{
    if (!isCompositingThread()) {
        dispatchSyncCompositingMessage(Olympia::Platform::createMessage1(&LayerGLES2::setMediaPlayer, this,
                                                                         mediaPlayer));
        return;
    }

    m_mediaPlayer = mediaPlayer;
}
#endif

void LayerGLES2::removeSublayer(LayerGLES2* sublayer)
{
    ASSERT(isCompositingThread());

    int foundIndex = indexOfSublayer(sublayer);
    if (foundIndex == -1)
        return;

    sublayer->setSuperlayer(0);
    m_sublayers.remove(foundIndex);
}

int LayerGLES2::indexOfSublayer(const LayerGLES2* reference)
{
    for (size_t i = 0; i < m_sublayers.size(); i++) {
        if (m_sublayers[i] == reference)
            return i;
    }
    return -1;
}

const LayerGLES2* LayerGLES2::rootLayer() const
{
    const LayerGLES2* layer = this;
    for (LayerGLES2* superlayer = layer->superlayer(); superlayer; layer = superlayer, superlayer = superlayer->superlayer()) { }
    return layer;
}

LayerGLES2* LayerGLES2::superlayer() const
{
    return m_superlayer;
}

void LayerGLES2::removeFromSuperlayer()
{
    if (m_superlayer)
        m_superlayer->removeSublayer(this);
}

void LayerGLES2::setSublayers(const Vector<RefPtr<LayerGLES2> >& sublayers)
{
    if (sublayers == m_sublayers)
        return;

    while (m_sublayers.size()) {
        RefPtr<LayerGLES2> layer = m_sublayers[0].get();
        ASSERT(layer->superlayer());
        layer->removeFromSuperlayer();
    }

    m_sublayers.clear();

    size_t listSize = sublayers.size();
    for (size_t i = 0; i < listSize; i++) {
        RefPtr<LayerGLES2> sublayer = sublayers[i];
        sublayer->removeFromSuperlayer();
        sublayer->setSuperlayer(this);
        m_sublayers.insert(i, sublayer);
    }
}

}

#endif // USE(ACCELERATED_COMPOSITING)
