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

#if PLATFORM(OPENVG)
#define AFFINE_TRANSFORM_NO_VG
#endif

#include "LayerProxyGLES2.h"

#include "LayerGLES2.h"
#if PLATFORM(SKIA)
#include "NativeImageSkia.h"
#include "PlatformContextSkia.h"
#include "skia/ext/platform_canvas.h"
#else
#include "BitmapImage.h"
#include "ImageBuffer.h"
#include "ImageData.h"
#endif
#include "PluginView.h"
#include "RenderLayerBacking.h"

#include <BlackBerryPlatformGraphics.h>


namespace WebCore {

using namespace std;

PassRefPtr<LayerProxyGLES2> LayerProxyGLES2::create(LayerType type, GraphicsLayerGLES2* owner)
{
    return adoptRef(new LayerProxyGLES2(type, owner));
}

LayerProxyGLES2::LayerProxyGLES2(LayerType type, GraphicsLayerGLES2* owner)
    : LayerBaseGLES2(type)
    , m_owner(owner)
    , m_superlayer(0)
    , m_contents(0)
    , m_contentsDirty(false)
    , m_drawsContent(false)
{
    m_impl = LayerGLES2::create(type);
}

LayerProxyGLES2::~LayerProxyGLES2()
{
    // Our superlayer should be holding a reference to us so there should be no
    // way for us to be destroyed while we still have a superlayer.
    ASSERT(!superlayer());

    // Remove the superlayer reference from all sublayers.
    removeAllSublayers();
}

PassRefPtr<ImageData> LayerProxyGLES2::createImageDataByPaintingContents(const IntRect& dirtyRect)
{
    PassRefPtr<ImageData> bitmap;

    // Don't try to allocate image data bigger than this. This should be big
    // enough to accomodate a huge iScroll use case.
    // FIXME: This is a hack to work around a crash bug on maps.bing.com where
    // a (visually empty) layer becomes too big.
    static const int maximumImageDataSizeInBytes = 40 * 1024 * 1024;
    static const int bytesPerPixel = 4;
    if (dirtyRect.width() * dirtyRect.height() * bytesPerPixel > maximumImageDataSizeInBytes)
        return bitmap;

#if PLATFORM(SKIA)
    const SkBitmap* skiaBitmap = 0;
    OwnPtr<skia::PlatformCanvas> canvas;
    OwnPtr<PlatformContextSkia> skiaContext;
    OwnPtr<GraphicsContext> graphicsContext;

    if (drawsContent()) { // Layer contents must be drawn into a canvas.
        canvas.set(new skia::PlatformCanvas(dirtyRect.width(), dirtyRect.height(), false, true, false));
        skiaContext.set(new PlatformContextSkia(canvas.get()));

#if OS(WINDOWS)
        // This is needed to get text to show up correctly. Without it,
        // GDI renders with zero alpha and the text becomes invisible.
        // Unfortunately, setting this to true disables cleartype.
        // FIXME: Does this take us down a very slow text rendering path?
        skiaContext->setDrawingToImageBuffer(true);
#endif

        graphicsContext.set(new GraphicsContext(reinterpret_cast<PlatformGraphicsContext*>(skiaContext.get())));

        // Bring the canvas into the coordinate system of the paint rect.
        canvas->translate(static_cast<SkScalar>(-dirtyRect.x()), static_cast<SkScalar>(-dirtyRect.y()));

        m_owner->paintGraphicsLayerContents(*graphicsContext, dirtyRect);
        const SkBitmap& bitmap = canvas->getDevice()->accessBitmap(false);
        skiaBitmap = &bitmap;
    } else { // Layer is a container.
        // The layer contains an Image.
        NativeImageSkia* skiaImage = static_cast<NativeImageSkia*>(contents());
        skiaBitmap = skiaImage;
    }

    ASSERT(skiaBitmap);
    SkAutoLockPixels lock(*skiaBitmap);
    SkBitmap::Config skiaConfig = skiaBitmap->config();
    // FIXME: do we need to support more image configurations?
    if (skiaBitmap->getPixels() && skiaConfig == SkBitmap::kARGB_8888_Config) {
        bitmap = ImageData::create(skiaBitmap->width(), skiaBitmap->height());
        unsigned char* data = bitmap->data()->data()->data();
        memcpy(data, skiaBitmap->getPixels(), bitmap->data()->length());
    }
#else
    if (drawsContent()) { // Layer contents must be drawn into a canvas.
        OwnPtr<ImageBuffer> imageBuffer = ImageBuffer::create(dirtyRect.size());
        GraphicsContext* graphicsContext = imageBuffer->context();

        // Bring the canvas into the coordinate system of the paint rect.
        graphicsContext->translate(-dirtyRect.x(), -dirtyRect.y());

        m_owner->paintGraphicsLayerContents(*graphicsContext, dirtyRect);
        bitmap = imageBuffer->getPremultipliedImageData(IntRect(IntPoint(0, 0), imageBuffer->size()));
    } else { // Layer is a container.
        // The layer contains an Image.
        NativeImagePtr image = contents();
        OwnPtr<ImageBuffer> imageBuffer = ImageBuffer::create(image->size());
        GraphicsContext* graphicsContext = imageBuffer->context();
        RefPtr<BitmapImage> bitmapImage = BitmapImage::create(image, 0);
        graphicsContext->drawImage(bitmapImage.get(), ColorSpaceDeviceRGB, IntPoint(0, 0));
        bitmap = imageBuffer->getPremultipliedImageData(IntRect(IntPoint(0, 0), imageBuffer->size()));
    }
#endif

    return bitmap;
}

void LayerProxyGLES2::updateTextureContents()
{
    RenderLayerBacking* backing = static_cast<RenderLayerBacking*>(m_owner->client());

#if PLATFORM(BLACKBERRY) && OS(QNX)
    if (m_pluginView) {
        // The layer contains Flash, video, or other plugin contents.
        m_pluginView->updateBuffer(IntRect(IntPoint(0, 0), m_pluginView->size()));
    } else
#endif
    if (backing && !backing->paintingGoesToWindow()) {
        IntRect dirtyRect(m_dirtyRect);
        IntSize requiredTextureSize;

        if (drawsContent()) { // Layer contents must be drawn into a canvas.
            IntRect boundsRect(IntPoint(), m_bounds);
            requiredTextureSize = boundsRect.size();
            if (requiredTextureSize != m_allocatedTextureSize)
                dirtyRect = boundsRect;
            else {
                // Clip the dirtyRect to the size of the layer to avoid drawing
                // outside the bounds of the backing texture.
                dirtyRect.intersect(boundsRect);
            }
        } else if (contents()) { // Layer is a container.
            // The layer contains an Image.
            requiredTextureSize = IntSize(contents()->width(), contents()->height());
            dirtyRect = IntRect(IntPoint(), requiredTextureSize);
        }

        // Use PassRefPtr<> to guarantee sequential ref counting - pass
        // the one ref off to the compositing thread without ref count churn
        // to make sure that we don't get a race condition on the ref count value.
        PassRefPtr<ImageData> bitmap;

        if (!dirtyRect.isEmpty() && !requiredTextureSize.isEmpty()) {
            bitmap = createImageDataByPaintingContents(dirtyRect);
            // bitmap can return null here. Make requiredTextureSize empty so that we
            // will not try to update and draw the layer.
            if (!bitmap.get())
                requiredTextureSize = IntSize();
        }

        m_allocatedTextureSize = requiredTextureSize;
        m_impl->setNeedsDisplay(bitmap, dirtyRect, requiredTextureSize);

        // bitmap is now 0, if it wasn't already. We used a PassRefPtr
        // to get sequential ref counting even though two threads are
        // involved.
    }

    m_contentsDirty = false;
    m_dirtyRect.setSize(FloatSize());
}

void LayerProxyGLES2::setContents(NativeImagePtr contents)
{
    // Check if the image has changed.
    if (m_contents == contents)
        return;
    m_contents = contents;
    setNeedsTexture(this->contents() || drawsContent() || pluginView());

#if PLATFORM(SKIA)
    setLayerProgramShader(LayerProgramShaderBGRA);
#else
    setLayerProgramShader(LayerProgramShaderRGBA);
#endif

    if (m_contents)
        setNeedsDisplay();
    else
        setNeedsCommit();
}

void LayerProxyGLES2::setDrawsContent(bool drawsContent)
{
    m_drawsContent = drawsContent;
    setNeedsTexture(contents() || this->drawsContent() || pluginView());

#if PLATFORM(SKIA)
    setLayerProgramShader(LayerProgramShaderBGRA);
#else
    setLayerProgramShader(LayerProgramShaderRGBA);
#endif

    if (m_drawsContent)
        setNeedsDisplay();
    else
        setNeedsCommit();
}

void LayerProxyGLES2::setPluginView(PluginView* pluginView)
{
    m_pluginView = pluginView;
    setNeedsTexture(contents() || drawsContent() || this->pluginView());
    setLayerProgramShader(LayerProgramShaderRGBA);

    if (m_pluginView)
        setNeedsDisplay();
    else {
        // We can't afford to wait until the next commit
        // to set the m_pluginView to 0 in the impl, because it is
        // about to be destroyed.
        m_impl->setPluginView(0);
        setNeedsCommit();
    }
}

#if ENABLE(VIDEO)
void LayerProxyGLES2::setMediaPlayer(MediaPlayer* mediaPlayer)
{
    m_mediaPlayer = mediaPlayer;
    setNeedsTexture(contents() || drawsContent() || this->mediaPlayer());
    setLayerProgramShader(LayerProgramShaderBGRA);

    if (m_mediaPlayer)
        setNeedsDisplay();
    else {
        // We can't afford to wait until the next commit
        // to set the m_mediaPlayer to 0 in the impl, because it is
        // about to be destroyed.
        m_impl->setMediaPlayer(0);

        // clear hole punch rect
        setHolePunchRect(IntRect());
        setNeedsCommit();
    }
}
#endif

void LayerProxyGLES2::setNeedsCommit()
{
    // Call notifySyncRequired(), which in this implementation plumbs through to
    // call scheduleRootLayerCommit() on the WebView, which will cause us to commit
    // changes done on the WebKit thread for display on the Compositing thread.
    if (m_owner)
        m_owner->notifySyncRequired();
}

void LayerProxyGLES2::commitOnWebKitThread()
{
    if (m_impl->texturesWereDeleted()) {
        m_dirtyRect.setLocation(FloatPoint());
        m_dirtyRect.setSize(m_bounds);
        m_contentsDirty = true;
    }

    if (m_contentsDirty)
        updateTextureContents();

    size_t listSize = m_sublayers.size();
    for (size_t i = 0; i < listSize; i++)
        m_sublayers[i]->commitOnWebKitThread();
}

void LayerProxyGLES2::commitOnCompositingThread()
{
    FloatPoint oldPosition = m_position;
    m_position += m_absoluteOffset;
    // Copy the base variables from this object into m_impl
    replicate(m_impl.get());
    m_position = oldPosition;
    updateLayerHierarchy();

    size_t listSize = m_sublayers.size();
    for (size_t i = 0; i < listSize; i++)
        m_sublayers[i]->commitOnCompositingThread();
}

void LayerProxyGLES2::addSublayer(PassRefPtr<LayerProxyGLES2> sublayer)
{
    insertSublayer(sublayer, numSublayers());
}

void LayerProxyGLES2::insertSublayer(PassRefPtr<LayerProxyGLES2> sublayer, size_t index)
{
    index = min(index, m_sublayers.size());
    sublayer->removeFromSuperlayer();
    sublayer->setSuperlayer(this);
    m_sublayers.insert(index, sublayer);

    setNeedsCommit();
}

void LayerProxyGLES2::removeFromSuperlayer()
{
    if (m_superlayer)
        m_superlayer->removeSublayer(this);
}

void LayerProxyGLES2::removeSublayer(LayerProxyGLES2* sublayer)
{
    int foundIndex = indexOfSublayer(sublayer);
    if (foundIndex == -1)
        return;

    sublayer->setSuperlayer(0);
    m_sublayers.remove(foundIndex);

    setNeedsCommit();
}

void LayerProxyGLES2::replaceSublayer(LayerProxyGLES2* reference, PassRefPtr<LayerProxyGLES2> newLayer)
{
    ASSERT_ARG(reference, reference);
    ASSERT_ARG(reference, reference->superlayer() == this);

    if (reference == newLayer)
        return;

    int referenceIndex = indexOfSublayer(reference);
    if (referenceIndex == -1) {
        ASSERT_NOT_REACHED();
        return;
    }

    reference->removeFromSuperlayer();

    if (newLayer) {
        newLayer->removeFromSuperlayer();
        insertSublayer(newLayer, referenceIndex);
    }
}

int LayerProxyGLES2::indexOfSublayer(const LayerProxyGLES2* reference)
{
    for (size_t i = 0; i < m_sublayers.size(); i++) {
        if (m_sublayers[i] == reference)
            return i;
    }
    return -1;
}

void LayerProxyGLES2::setBounds(const IntSize& size)
{
    if (m_bounds == size)
        return;

    bool firstResize = !m_bounds.width() && !m_bounds.height() && size.width() && size.height();

    m_bounds = size;
    m_backingStoreSize = size;

    // if this layer is for video, the entire layer should be hole punched
#if ENABLE(VIDEO)
    if (mediaPlayer())
        setHolePunchRect(IntRect(0, 0, m_bounds.width(), m_bounds.height()));
#endif

    if (firstResize)
        setNeedsDisplay();
    else
        setNeedsCommit();
}

void LayerProxyGLES2::setFrame(const FloatRect& rect)
{
    if (rect == m_frame)
      return;

    m_frame = rect;
    setNeedsDisplay();
}

const LayerProxyGLES2* LayerProxyGLES2::rootLayer() const
{
    const LayerProxyGLES2* layer = this;
    LayerProxyGLES2* superlayer = layer->superlayer();

    while (superlayer) {
        layer = superlayer;
        superlayer = superlayer->superlayer();
    }
    return layer;
}

void LayerProxyGLES2::removeAllSublayers()
{
    while (m_sublayers.size()) {
        RefPtr<LayerProxyGLES2> layer = m_sublayers[0].get();
        ASSERT(layer->superlayer());
        layer->removeFromSuperlayer();
    }
    setNeedsCommit();
}

void LayerProxyGLES2::setSublayers(const Vector<RefPtr<LayerProxyGLES2> >& sublayers)
{
    if (sublayers == m_sublayers)
        return;

    removeAllSublayers();
    size_t listSize = sublayers.size();
    for (size_t i = 0; i < listSize; i++)
        addSublayer(sublayers[i]);
}

LayerProxyGLES2* LayerProxyGLES2::superlayer() const
{
    return m_superlayer;
}

void LayerProxyGLES2::setNeedsDisplay(const FloatRect& dirtyRect)
{
    // Simply mark the contents as dirty. The actual redraw will
    // happen when it's time to do the compositing.
    m_contentsDirty = true;

    m_dirtyRect.unite(dirtyRect);

    setNeedsCommit(); // FIXME: Replace this with a more targeted message for dirty rect handling with plugin content?
}

void LayerProxyGLES2::setNeedsDisplay()
{
    m_dirtyRect.setLocation(FloatPoint());
    m_dirtyRect.setSize(m_bounds);
    m_contentsDirty = true;

    setNeedsCommit(); // FIXME: Replace this with a more targeted message for dirty rect handling with plugin content?
}

void LayerProxyGLES2::updateLayerHierarchy()
{
    m_impl->setSuperlayer(superlayer() ? superlayer()->m_impl.get() : 0);

    Vector<RefPtr<LayerGLES2> > sublayers;
    size_t listSize = m_sublayers.size();
    for (size_t i = 0; i < listSize; i++)
        sublayers.append(m_sublayers[i]->m_impl.get());
    m_impl->setSublayers(sublayers);
}

}

#endif // USE(ACCELERATED_COMPOSITING)
