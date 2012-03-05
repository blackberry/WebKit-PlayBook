/*
 * Copyright (C) 2009, 2010, 2011 Research In Motion Limited. All rights reserved.
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
#include "GlyphPageTreeNode.h"

#include "FontPlatformData.h"
#include "SimpleFontData.h"

namespace WebCore {

bool GlyphPage::fill(unsigned offset, unsigned length, UChar* charBuffer, unsigned bufferLength, const SimpleFontData* fontData)
{
    const bool isUtf16 = (bufferLength != length);

    for (unsigned i = 0; i < length; ++i) {
        Glyph character = isUtf16
            ? U16_GET_SUPPLEMENTARY(charBuffer[i*2], charBuffer[i*2 + 1])
            : charBuffer[i];

        setGlyphDataForIndex(offset + i, character, fontData);
    }
    return true;
}

}
