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

#ifndef LayerTile_h
#define LayerTile_h

#if USE(ACCELERATED_COMPOSITING)

#include "Color.h"
#include "FloatRect.h"
#include "IntRect.h"
#include "IntSize.h"
#include "LayerTileData.h"
#include "LayerTileIndex.h"
#include "Texture.h"

#include <SkBitmap.h>

namespace WebCore {

class GLES2Context;

class LayerTile : public LayerTileData {
    WTF_MAKE_FAST_ALLOCATED;
public:
    LayerTile();
    ~LayerTile();

    Texture* texture() const { return m_texture.get(); }

    bool isVisible() const { return m_visible; }
    void setVisible(bool);

    bool isDirty() const { return m_contentsDirty || !m_texture || m_texture->isDirty(); }

    bool hasTexture() const { return m_texture && m_texture->hasTexture(); }

    void setContents(GLES2Context*, const SkBitmap& contents, const IntRect& tileRect, const TileIndex&, bool isOpaque);
    void setContentsToColor(GLES2Context*, const Color&);
    void updateContents(GLES2Context*, const SkBitmap& contents, const IntRect& dirtyRect, const IntRect& tileRect);
    void discardContents();

    // The current texture is an accurate preview of this layer, but a more
    // detailed texture could be obtained by repainting the layer. Used when
    // zoom level changes.
    void setContentsDirty() { m_contentsDirty = true; }

private:
    void setTexture(PassRefPtr<Texture>);

    RefPtr<Texture> m_texture; // Never assign to m_texture directly, use setTexture() above
    bool m_contentsDirty;
};

}

#endif // USE(ACCELERATED_COMPOSITING)

#endif // Texture_h
