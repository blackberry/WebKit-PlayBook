/*
 * Copyright (C) 2011 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "Texture.h"

#if USE(ACCELERATED_COMPOSITING)

#include "TextureCacheCompositingThread.h"

#include <GLES2/gl2.h>
#include <wtf/CurrentTime.h>
#include <wtf/OwnArrayPtr.h>
#include <wtf/PassOwnArrayPtr.h>

#define DEBUG_TEXTURE_UPLOADS 0

namespace WebCore {

static void copyImageData(unsigned char* dst, int dstStride, int dstX, int dstY,
                    const unsigned char* src, int srcStride, int srcX, int srcY, int width, int height)
{
    dst += (dstY * dstStride) + (Texture::bytesPerPixel() * dstX);
    src += (srcY * srcStride) + (Texture::bytesPerPixel() * srcX);
    int length = Texture::bytesPerPixel() * width;

    for (int y = 0; y < height; ++y) {
        memcpy(dst, src, length);
        dst += dstStride;
        src += srcStride;
    }
}

Texture::Texture(GLES2Context* context, bool isColor)
    : m_protectionCount(0)
    , m_textureId(0)
    , m_isColor(isColor)
    , m_isOpaque(false)
{
    textureCacheCompositingThread()->install(this, context);
}

Texture::~Texture()
{
    textureCacheCompositingThread()->textureDestroyed(this);
}

void Texture::updateContents(GLES2Context* context, const SkBitmap& contents, const IntRect& dirtyRect, const IntRect& tile, bool isOpaque)
{
    IntRect tileDirtyRect(dirtyRect);

    tileDirtyRect.intersect(tile);
    if (tileDirtyRect.isEmpty())
        return;

    m_isOpaque = isOpaque;

    int stride = bytesPerPixel() * contents.width();
    int x = tileDirtyRect.x() - dirtyRect.x();
    int y = tileDirtyRect.y() - dirtyRect.y();
    SkAutoLockPixels lock(contents);
    unsigned char* pixels = static_cast<unsigned char*>(contents.getPixels());
    pixels += (y * stride) + (bytesPerPixel() * x);
    if (!pixels)
        return;

    bool subImage = (tile.size() == m_size);
    IntSize size = subImage ? tileDirtyRect.size() : tile.size();
    IntSize contentsSize(IntSize(contents.width(), contents.height()));

#if DEBUG_TEXTURE_UPLOADS
    fprintf(stderr, "%s\n", subImage ? "SUBIMAGE" : "IMAGE");
    fprintf(stderr, "  tile = %s, m_size = %s\n", tile.toString().latin1().data(), m_size.toString().latin1().data());
    fprintf(stderr, "  tileDirtyRect = %s, contents.size() = %s\n", tileDirtyRect.toString().latin1().data(), contentsSize.toString().latin1().data());
    IntSize yeOldeSize = size;
#endif

    size = size.shrunkTo(contentsSize);

#if DEBUG_TEXTURE_UPLOADS
    if (size != yeOldeSize)
        fprintf(stderr, "  SHRUNK!!!!\n");
#endif

    if (!hasTexture()) {
        // We may have been evicted by the TextureCacheCompositingThread,
        // attempt to install us again.
        if (!textureCacheCompositingThread()->install(this, context))
            return;
    }

    glBindTexture(GL_TEXTURE_2D, m_textureId);

    ASSERT(size.width() >= 0);
    bool doesTileStrideEqualTextureStride = contents.width() == static_cast<unsigned>(size.width());

    OwnArrayPtr<unsigned char> tmp;
    if (!doesTileStrideEqualTextureStride) {
#if defined(GL_UNPACK_ROW_LENGTH)
        glPixelStorei(GL_UNPACK_ROW_LENGTH, contents.width());
#else
        tmp = adoptArrayPtr(new unsigned char[size.width() * size.height() * bytesPerPixel()]);
        copyImageData(tmp.get(), bytesPerPixel() * size.width(), 0, 0,
                      pixels, stride, 0, 0,
                      size.width(), size.height());
        pixels = tmp.get();
#endif
    }

    if (subImage) {
#if DEBUG_TEXTURE_UPLOADS
        fprintf(stderr, "glTexSubImage2D(%d, %d, %d, %d)\n", tileDirtyRect.x() - tile.x(), tileDirtyRect.y() - tile.y(),
                        size.width(), size.height());
        double t = currentTime();
#endif
        glTexSubImage2D(GL_TEXTURE_2D, 0 /*level*/,
                        tileDirtyRect.x() - tile.x(), tileDirtyRect.y() - tile.y(),
                        size.width(), size.height(),
                        GL_RGBA, GL_UNSIGNED_BYTE, pixels);
#if DEBUG_TEXTURE_UPLOADS
        t = currentTime() - t;
        fprintf(stderr, "    time = %f s (%d bytes)\n", t, size.width()*size.height()*4);
#endif
    } else {
#if DEBUG_TEXTURE_UPLOADS
        fprintf(stderr, "glTexImage2D(%d, %d)\n", size.width(), size.height());
        double t = currentTime();
#endif
        glTexImage2D(GL_TEXTURE_2D, 0 /*level*/, GL_RGBA /*internal*/,
                     size.width(), size.height(),
                     0 /*border*/, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
#if DEBUG_TEXTURE_UPLOADS
        t = currentTime() - t;
        fprintf(stderr, "    time = %f s (%d bytes)\n", t, size.width()*size.height()*4);
#endif
    }

#if defined(GL_UNPACK_ROW_LENGTH)
    if (!doesTileStrideEqualTextureStride)
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif

    IntSize oldSize = m_size;
    m_size = tile.size();
    if (m_size != oldSize)
        textureCacheCompositingThread()->textureResized(this, oldSize);
}

void Texture::setContentsToColor(GLES2Context*, const Color& color)
{
    m_isOpaque = !color.hasAlpha();
    RGBA32 rgba = color.rgb();
    glBindTexture(GL_TEXTURE_2D, m_textureId);
    glTexImage2D(GL_TEXTURE_2D, 0 /*level*/, GL_RGBA /*internal*/,
                 1, 1,
                 0 /*border*/, GL_RGBA, GL_UNSIGNED_BYTE, &rgba);

    IntSize oldSize = m_size;
    m_size = IntSize(1, 1);
    if (m_size != oldSize)
        textureCacheCompositingThread()->textureResized(this, oldSize);
}

bool Texture::protect(const IntSize& size)
{
    if (!hasTexture()) {
        // We may have been evicted by the TextureCacheCompositingThread,
        // attempt to install us again.
        if (!textureCacheCompositingThread()->install(this, /*context*/ 0))
            return false;
    }

    ++m_protectionCount;

    if (m_size == size)
        return true;

    IntSize oldSize = m_size;
    m_size = size;
    textureCacheCompositingThread()->textureResized(this, oldSize);

    glBindTexture(GL_TEXTURE_2D, m_textureId);
    glTexImage2D(GL_TEXTURE_2D, 0 /*level*/, GL_RGBA /*internal*/,
                 size.width(), size.height(),
                 0 /*border*/, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    return true;
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
