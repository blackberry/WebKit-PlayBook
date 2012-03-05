/*
 * Copyright (C) 2010, 2011 Research In Motion Limited. All rights reserved.
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
#include "FontCustomPlatformData.h"

#include "AtomicString.h"
#include "FontDescription.h"
#include "FontPlatformData.h"
#include "SharedBuffer.h"
#include "WOFFFileFormat.h"

#include <string.h> // for memcpy()

namespace WebCore {

static int sharedBufferStreamGetLength(BlackBerry::Platform::Text::Stream& stream)
{
    return static_cast<SharedBuffer*>(stream.m_data)->size();
}

static int sharedBufferStreamRead(BlackBerry::Platform::Text::Stream& stream, int offset, unsigned char* dstBuffer, int count)
{
    const unsigned char* srcBuffer = reinterpret_cast<const unsigned char*>(
        static_cast<SharedBuffer*>(stream.m_data)->data());

    if (offset + count > sharedBufferStreamGetLength(stream))
        count = sharedBufferStreamGetLength(stream) - offset;

    memcpy(dstBuffer, srcBuffer + offset, count);

    return count; // FIXME: is that the correct return value?
}

static void sharedBufferStreamClose(BlackBerry::Platform::Text::Stream& stream)
{
}


FontCustomPlatformData::~FontCustomPlatformData()
{
    BlackBerry::Platform::Text::ReturnCode error = FontPlatformData::engine()->unloadFontData(m_fontDataId);
    ASSERT(!error);
}

FontPlatformData FontCustomPlatformData::fontPlatformData(int size, bool bold, bool italic, FontRenderingMode mode)
{
    String familyName("@CSS");
    familyName.append(String::number(m_id));

    FontDescription desc;
    desc.setSpecifiedSize(size);
    desc.setComputedSize(size);
    desc.setWeight(bold ? FontWeightBold : FontWeightNormal);
    desc.setItalic(italic ? true : false);

    return FontPlatformData(desc, familyName);
}

FontCustomPlatformData* createFontCustomPlatformData(SharedBuffer* buffer)
{
    static int customFontId = 1;

    RefPtr<SharedBuffer> sfntBuffer;
    if (isWOFF(buffer)) {
        Vector<char> sfnt;
        if (!convertWOFFToSfnt(buffer, sfnt))
            return 0;

        sfntBuffer = SharedBuffer::adoptVector(sfnt);
        buffer = sfntBuffer.get();
    }

    BlackBerry::Platform::Text::Stream stream;
    stream.m_data = buffer;
    stream.m_read = sharedBufferStreamRead;
    stream.m_close = sharedBufferStreamClose;
    stream.m_getLength = sharedBufferStreamGetLength;

    BlackBerry::Platform::Text::FontDataId fontDataId;
    BlackBerry::Platform::Text::ReturnCode error;
    String familyName("@CSS");
    familyName.append(String::number(customFontId));
    familyName.append('\0');

    error = FontPlatformData::engine()->loadFontData(fontDataId, stream, familyName.characters());
    if (error)
        return 0;

    FontCustomPlatformData* fcpd = new FontCustomPlatformData();
    fcpd->m_fontDataId = fontDataId;
    fcpd->m_id = customFontId;
    ++customFontId;

    return fcpd;
}

bool FontCustomPlatformData::supportsFormat(const String& format)
{
    return equalIgnoringCase(format, "truetype") || equalIgnoringCase(format, "opentype") || equalIgnoringCase(format, "woff");
}


}
