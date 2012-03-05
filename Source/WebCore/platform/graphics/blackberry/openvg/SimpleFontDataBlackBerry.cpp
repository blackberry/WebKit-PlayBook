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
#include "SimpleFontData.h"

#include "FontDescription.h"
#include "NotImplemented.h"
#include "SurfaceOpenVG.h"
#include "VGUtils.h"

#if USE(EGL)
#include "EGLDisplayOpenVG.h"
#endif

#include <BlackBerryPlatformText.h>

namespace WebCore {

SimpleFontData* SimpleFontData::smallCapsFontData(const FontDescription& desc) const
{
    if (m_smallCapsFontData)
        return m_smallCapsFontData;

    FontDescription smallCapsDesc = desc;
    smallCapsDesc.setSmallCaps(true);

    FontPlatformData smallCapsPlatformFont(smallCapsDesc, platformData().fontFamily());
    m_smallCapsFontData = new SimpleFontData(smallCapsPlatformFont);
    return m_smallCapsFontData;
}

bool SimpleFontData::containsCharacters(const UChar*, int length) const
{
    notImplemented();
    return true;
}

void SimpleFontData::determinePitch()
{
    if (platformData().isValid())
        m_treatAsFixedPitch = platformData().isFixedPitch();
}

void SimpleFontData::platformInit()
{
    if (platformData().isValid()) {
        BlackBerry::Platform::Text::Font* font = platformData().font();
        const float scaleFactor = platformData().scaleFactor();
        ASSERT(font);

        BlackBerry::Platform::Text::FontMetrics fontMetrics;
        font->getFontMetrics(fontMetrics);

        m_ascent = fontMetrics.m_ascent * scaleFactor + 0.9;
        m_descent = fontMetrics.m_descent * scaleFactor + 0.9;
        m_lineGap = (fontMetrics.m_leadingAbove + fontMetrics.m_leadingBelow) * scaleFactor + 0.9;
        m_lineSpacing = m_ascent + m_descent + m_lineGap;
        m_maxCharWidth = fontMetrics.m_maxCharWidth * scaleFactor;
        m_unitsPerEm = fontMetrics.m_height * scaleFactor;

        static const BlackBerry::Platform::Text::Utf16Char characters[] = { 'x' };
        BlackBerry::Platform::Text::TextMetrics metrics;

#if USE(EGL) // FIXME: remove after Text API fixes shared context handling
        if (eglGetCurrentContext() == EGL_NO_CONTEXT)
            EGLDisplayOpenVG::current()->sharedPlatformSurface()->makeCurrent();
#endif
        FontPlatformData::engine()->drawText(0 /* no drawing, only measuring */,
            *font, characters, 1 /* number of characters */, 0 /*x*/, 0 /*y*/,
            0 /* no wrap */, 0 /* draw params */, &metrics);

        if (platformData().hasLineSpacingOverride()) {
            m_ascent = platformData().ascentOverride();
            m_descent = platformData().descentOverride();
            m_lineSpacing = platformData().lineSpacingOverride();
            m_lineGap = 0;
        }

        m_xHeight = (metrics.m_boundsBottom - metrics.m_boundsTop) * scaleFactor;
        m_avgCharWidth = metrics.m_linearAdvance * scaleFactor;
    }
}

void SimpleFontData::platformCharWidthInit()
{
}

void SimpleFontData::platformDestroy()
{
    delete m_smallCapsFontData;
    m_smallCapsFontData = 0;
}

float SimpleFontData::platformWidthForGlyph(Glyph glyph) const
{
    BlackBerry::Platform::Text::Font* font = platformData().font();
    ASSERT(font);

    const BlackBerry::Platform::Text::Utf16Char characters[] = { glyph };
    BlackBerry::Platform::Text::TextMetrics metrics;

#if USE(EGL) // FIXME: remove after Text API fixes shared context handling
    if (eglGetCurrentContext() == EGL_NO_CONTEXT)
        EGLDisplayOpenVG::current()->sharedPlatformSurface()->makeCurrent();
#endif
    FontPlatformData::engine()->drawText(0 /* no drawing, only measuring */,
        *font, characters, 1 /* number of characters */, 0 /*x*/, 0 /*y*/,
        0 /* no wrap */, 0 /* draw params */, &metrics);

    return metrics.m_linearAdvance * platformData().scaleFactor();
}

FloatRect SimpleFontData::platformBoundsForGlyph(Glyph glyph) const
{
    BlackBerry::Platform::Text::Font* font = platformData().font();
    ASSERT(font);

    const BlackBerry::Platform::Text::Utf16Char characters[] = { glyph };
    BlackBerry::Platform::Text::TextMetrics metrics;

#if USE(EGL) // FIXME: remove after Text API fixes shared context handling
    if (eglGetCurrentContext() == EGL_NO_CONTEXT)
        EGLDisplayOpenVG::current()->sharedPlatformSurface()->makeCurrent();
#endif
    FontPlatformData::engine()->drawText(0 /* no drawing, only measuring */,
        *font, characters, 1 /* number of characters */, 0 /*x*/, 0 /*y*/,
        0 /* no wrap */, 0 /* draw params */, &metrics);

    const float scaleFactor = platformData().scaleFactor();
    float boundLeft = metrics.m_boundsLeft * scaleFactor;
    float boundTop = metrics.m_boundsTop * scaleFactor;
    float boundRight = metrics.m_boundsRight * scaleFactor;
    float boundBottom = metrics.m_boundsBottom * scaleFactor;
    return FloatRect(boundLeft, boundTop, boundRight - boundLeft, boundBottom - boundTop);
}

}
