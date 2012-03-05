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

#ifndef LayerTileData_h
#define LayerTileData_h

#if USE(ACCELERATED_COMPOSITING)

#include "FloatRect.h"
#include "IntRect.h"
#include "IntSize.h"
#include "Texture.h"

#include <SkBitmap.h>
#include <wtf/Noncopyable.h>

namespace WebCore {

class GLES2Context;

class LayerTileData {
public:
    LayerTileData()
        : m_visible(false)
    {
    }

    bool isVisible() const { return m_visible; }

protected:
    bool m_visible;
};

}

#endif // USE(ACCELERATED_COMPOSITING)

#endif // LayerTileData_h
