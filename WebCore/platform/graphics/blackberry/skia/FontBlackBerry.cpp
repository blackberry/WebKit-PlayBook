/*
 * Copyright (c) 2007, 2008, Google Inc. All rights reserved.
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
#include "Font.h"

#include "FloatRect.h"
#include "GlyphBuffer.h"
#include "GraphicsContext.h"
#include "HarfbuzzSkia.h"
#include "NotImplemented.h"
#include "PlatformContextSkia.h"
#include "SimpleFontData.h"

#include "SkCanvas.h"
#include "SkPaint.h"
#include "SkTemplates.h"
#include "SkTypeface.h"
#include "SkUtils.h"

#include <unicode/normlzr.h>
#include <unicode/uchar.h>
#include <wtf/OwnArrayPtr.h>
#include <wtf/OwnPtr.h>
#include <wtf/unicode/Unicode.h>
#include <wtf/Vector.h>

namespace WebCore {

bool Font::canReturnFallbackFontsForComplexText()
{
    return true;
}

static bool isCanvasMultiLayered(SkCanvas* canvas)
{
    SkCanvas::LayerIter layerIterator(canvas, false);
    layerIterator.next();
    return !layerIterator.done();
}

static void adjustTextRenderMode(SkPaint* paint, PlatformContextSkia* skiaContext)
{
    // Our layers only have a single alpha channel. This means that subpixel
    // rendered text cannot be compositied correctly when the layer is
    // collapsed. Therefore, subpixel text is disabled when we are drawing
    // onto a layer or when the compositor is being used.
    if (isCanvasMultiLayered(skiaContext->canvas()) || skiaContext->isDrawingToImageBuffer())
        paint->setLCDRenderText(false);
}

void Font::drawGlyphs(GraphicsContext* gc, const SimpleFontData* font,
                      const GlyphBuffer& glyphBuffer,  int from, int numGlyphs,
                      const FloatPoint& point) const {
    SkASSERT(sizeof(GlyphBufferGlyph) == sizeof(uint16_t)); // compile-time assert

    const GlyphBufferGlyph* glyphs = glyphBuffer.glyphs(from);
    SkScalar x = SkFloatToScalar(point.x());
    SkScalar y = SkFloatToScalar(point.y());

    // FIXME: text rendering speed:
    // Android has code in their WebCore fork to special case when the
    // GlyphBuffer has no advances other than the defaults. In that case the
    // text drawing can proceed faster. However, it's unclear when those
    // patches may be upstreamed to WebKit so we always use the slower path
    // here.
    const GlyphBufferAdvance* adv = glyphBuffer.advances(from);
    SkAutoSTMalloc<32, SkPoint> storage(numGlyphs);
    SkPoint* pos = storage.get();

    for (int i = 0; i < numGlyphs; i++) {
        pos[i].set(x, y);
        x += SkFloatToScalar(adv[i].width());
        y += SkFloatToScalar(adv[i].height());
    }

    gc->platformContext()->prepareForSoftwareDraw();

    SkCanvas* canvas = gc->platformContext()->canvas();
    int textMode = gc->platformContext()->getTextDrawingMode();

    // We draw text up to two times (once for fill, once for stroke).
    if (textMode & cTextFill) {
        SkPaint paint;
        gc->platformContext()->setupPaintForFilling(&paint);
        font->platformData().setupPaint(&paint);
        adjustTextRenderMode(&paint, gc->platformContext());
        paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);
        paint.setColor(gc->fillColor().rgb());
        canvas->drawPosText(glyphs, numGlyphs << 1, pos, paint);
    }

    if ((textMode & cTextStroke)
        && gc->platformContext()->getStrokeStyle() != NoStroke
        && gc->platformContext()->getStrokeThickness() > 0) {

        SkPaint paint;
        gc->platformContext()->setupPaintForStroking(&paint, 0, 0);
        font->platformData().setupPaint(&paint);
        adjustTextRenderMode(&paint, gc->platformContext());
        paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);
        paint.setColor(gc->strokeColor().rgb());

        if (textMode & cTextFill) {
            // If we also filled, we don't want to draw shadows twice.
            // See comment in FontChromiumWin.cpp::paintSkiaText() for more details.
            SkSafeUnref(paint.setLooper(0));
        }

        canvas->drawPosText(glyphs, numGlyphs << 1, pos, paint);
    }
}

// Harfbuzz uses 26.6 fixed point values for pixel offsets. However, we don't
// handle subpixel positioning so this function is used to truncate Harfbuzz
// values to a number of pixels.
static int truncateFixedPointToInteger(HB_Fixed value)
{
    return value >> 6;
}

// TextRunWalker walks a TextRun and presents each script run in sequence. A
// TextRun is a sequence of code-points with the same embedding level (i.e. they
// are all left-to-right or right-to-left). A script run is a subsequence where
// all the characters have the same script (e.g. Arabic, Thai etc). Shaping is
// only ever done with script runs since the shapers only know how to deal with
// a single script.
//
// Iteration is always in logical (aka reading) order.  For RTL text that means
// the rightmost part of the text will be first.
//
// Once you have setup the object, call |nextScriptRun| to get the first script
// run. This will return false when the iteration is complete. At any time you
// can call |reset| to start over again.
class TextRunWalker {
public:
    TextRunWalker(const TextRun&, unsigned, const Font*);
    ~TextRunWalker();

    bool isWordBreak(unsigned);
    int determineWordBreakSpacing(unsigned);
    // setPadding sets a number of pixels to be distributed across the TextRun.
    // WebKit uses this to justify text.
    void setPadding(int);
    void reset(unsigned offset);
    // Advance to the next script run, returning false when the end of the
    // TextRun has been reached.
    bool nextScriptRun();
    float widthOfFullRun();

    // setWordSpacingAdjustment sets a delta (in pixels) which is applied at
    // each word break in the TextRun.
    void setWordSpacingAdjustment(int wordSpacingAdjustment) { m_wordSpacingAdjustment = wordSpacingAdjustment; }

    // setLetterSpacingAdjustment sets an additional number of pixels that is
    // added to the advance after each output cluster. This matches the behaviour
    // of WidthIterator::advance.
    void setLetterSpacingAdjustment(int letterSpacingAdjustment) { m_letterSpacing = letterSpacingAdjustment; }
    int letterSpacing() const { return m_letterSpacing; }

    // Set the x offset for the next script run. This affects the values in
    // |xPositions|
    bool rtl() const { return m_run.rtl(); }
    const uint16_t* glyphs() const { return m_glyphs16; }

    // Return the length of the array returned by |glyphs|
    const unsigned length() const { return m_item.num_glyphs; }

    // Return the x offset for each of the glyphs. Note that this is translated
    // by the current x offset and that the x offset is updated for each script
    // run.
    const SkScalar* xPositions() const { return m_xPositions; }

    // Get the advances (widths) for each glyph.
    const HB_Fixed* advances() const { return m_item.advances; }

    // Return the width (in px) of the current script run.
    const unsigned width() const { return m_pixelWidth; }

    // Return the cluster log for the current script run. For example:
    //   script run: f i a n c é  (fi gets ligatured)
    //   log clutrs: 0 0 1 2 3 4
    // So, for each input code point, the log tells you which output glyph was
    // generated for it.
    const unsigned short* logClusters() const { return m_item.log_clusters; }

    // return the number of code points in the current script run
    const unsigned numCodePoints() const { return m_item.item.length; }

    // Return the current pixel position of the controller.
    const unsigned offsetX() const { return m_offsetX; }

    const FontPlatformData* fontPlatformDataForScriptRun() { return reinterpret_cast<FontPlatformData*>(m_item.font->userData); }

private:
    void setupFontForScriptRun();
    HB_FontRec* allocHarfbuzzFont();
    void deleteGlyphArrays();
    void createGlyphArrays(int);
    void resetGlyphArrays();
    void shapeGlyphs();
    void setGlyphXPositions(bool);

    static void normalizeSpacesAndMirrorChars(const UChar* source, bool rtl, UChar* destination, int length);
    static const TextRun& getNormalizedTextRun(const TextRun& originalRun, OwnPtr<TextRun>& normalizedRun, OwnArrayPtr<UChar>& normalizedBuffer);


    // This matches the logic in RenderBlock::findNextLineBreak
    static bool isCodepointSpace(HB_UChar16 c) { return c == ' ' || c == '\t'; }

    const Font* const m_font;
    const SimpleFontData* m_currentFontData;
    HB_ShaperItem m_item;
    uint16_t* m_glyphs16; // A vector of 16-bit glyph ids.
    SkScalar* m_xPositions; // A vector of x positions for each glyph.
    ssize_t m_indexOfNextScriptRun; // Indexes the script run in |m_run|.
    unsigned m_offsetX; // Offset in pixels to the start of the next script run.
    unsigned m_pixelWidth; // Width (in px) of the current script run.
    unsigned m_glyphsArrayCapacity; // Current size of all the Harfbuzz arrays.

    OwnPtr<TextRun> m_normalizedRun;
    OwnArrayPtr<UChar> m_normalizedBuffer; // A buffer for normalized run.
    const TextRun& m_run;
    int m_wordSpacingAdjustment; // delta adjustment (pixels) for each word break.
    float m_padding; // pixels to be distributed over the line at word breaks.
    float m_padPerWordBreak; // pixels to be added to each word break.
    float m_padError; // |m_padPerWordBreak| might have a fractional component.
                      // Since we only add a whole number of padding pixels at
                      // each word break we accumulate error. This is the
                      // number of pixels that we are behind so far.
    int m_letterSpacing; // pixels to be added after each glyph.
};


TextRunWalker::TextRunWalker(const TextRun& run, unsigned startingX, const Font* font)
    : m_font(font)
    , m_run(getNormalizedTextRun(run, m_normalizedRun, m_normalizedBuffer))
    , m_wordSpacingAdjustment(0)
    , m_padding(0)
    , m_padPerWordBreak(0)
    , m_padError(0)
    , m_letterSpacing(0)
{
    // Do not use |run| inside this constructor. Use |m_run| instead.

    memset(&m_item, 0, sizeof(m_item));
    // We cannot know, ahead of time, how many glyphs a given script run
    // will produce. We take a guess that script runs will not produce more
    // than twice as many glyphs as there are code points plus a bit of
    // padding and fallback if we find that we are wrong.
    createGlyphArrays((m_run.length() + 2) * 2);

    m_item.log_clusters = new unsigned short[m_run.length()];

    m_item.face = 0;
    m_item.font = allocHarfbuzzFont();

    m_item.item.bidiLevel = m_run.rtl();

    m_item.string = m_run.characters();
    m_item.stringLength = m_run.length();

    reset(startingX);
}

TextRunWalker::~TextRunWalker()
{
    fastFree(m_item.font);
    deleteGlyphArrays();
    delete[] m_item.log_clusters;
}

bool TextRunWalker::isWordBreak(unsigned index)
{
    return index && isCodepointSpace(m_item.string[index]) && !isCodepointSpace(m_item.string[index - 1]);
}

int TextRunWalker::determineWordBreakSpacing(unsigned logClustersIndex)
{
    int wordBreakSpacing = 0;
    // The first half of the conjunction works around the case where
    // output glyphs aren't associated with any codepoints by the
    // clusters log.
    if (logClustersIndex < m_item.item.length
        && isWordBreak(m_item.item.pos + logClustersIndex)) {
        wordBreakSpacing = m_wordSpacingAdjustment;

        if (m_padding > 0) {
            int toPad = roundf(m_padPerWordBreak + m_padError);
            m_padError += m_padPerWordBreak - toPad;

            if (m_padding < toPad)
                toPad = m_padding;
            m_padding -= toPad;
            wordBreakSpacing += toPad;
        }
    }
    return wordBreakSpacing;
}

// setPadding sets a number of pixels to be distributed across the TextRun.
// WebKit uses this to justify text.
void TextRunWalker::setPadding(int padding)
{
    m_padding = padding;
    if (!m_padding)
        return;

    // If we have padding to distribute, then we try to give an equal
    // amount to each space. The last space gets the smaller amount, if
    // any.
    unsigned numWordBreaks = 0;

    for (unsigned i = 0; i < m_item.stringLength; i++) {
        if (isWordBreak(i))
            numWordBreaks++;
    }

    if (numWordBreaks)
        m_padPerWordBreak = m_padding / numWordBreaks;
    else
        m_padPerWordBreak = 0;
}

void TextRunWalker::reset(unsigned offset)
{
    m_indexOfNextScriptRun = 0;
    m_offsetX = offset;
}

// Advance to the next script run, returning false when the end of the
// TextRun has been reached.
bool TextRunWalker::nextScriptRun()
{
    if (!hb_utf16_script_run_next(0, &m_item.item, m_run.characters(), m_run.length(), &m_indexOfNextScriptRun))
        return false;

    // It is actually wrong to consider script runs at all in this code.
    // Other WebKit code (e.g. Mac) segments complex text just by finding
    // the longest span of text covered by a single font.
    // But we currently need to call hb_utf16_script_run_next anyway to fill
    // in the harfbuzz data structures to e.g. pick the correct script's shaper.
    // So we allow that to run first, then do a second pass over the range it
    // found and take the largest subregion that stays within a single font.
    m_currentFontData = m_font->glyphDataForCharacter(m_item.string[m_item.item.pos], false).fontData;
    unsigned endOfRun;
    for (endOfRun = 1; endOfRun < m_item.item.length; ++endOfRun) {
        const SimpleFontData* nextFontData = m_font->glyphDataForCharacter(m_item.string[m_item.item.pos + endOfRun], false).fontData;
        if (nextFontData != m_currentFontData)
            break;
    }
    m_item.item.length = endOfRun;
    m_indexOfNextScriptRun = m_item.item.pos + endOfRun;

    setupFontForScriptRun();
    shapeGlyphs();
    setGlyphXPositions(rtl());

    return true;
}

float TextRunWalker::widthOfFullRun()
{
    float widthSum = 0;
    while (nextScriptRun())
        widthSum += width();

    return widthSum;
}

void TextRunWalker::setupFontForScriptRun()
{
    const FontData* fontData = m_font->glyphDataForCharacter(m_item.string[m_item.item.pos], false, false).fontData;
    const FontPlatformData& platformData = fontData->fontDataForCharacter(' ')->platformData();
    m_item.face = platformData.harfbuzzFace();
    void* opaquePlatformData = const_cast<FontPlatformData*>(&platformData);
    m_item.font->userData = opaquePlatformData;

    SkPaint paint;
    platformData.setupPaint(&paint);
    SkPaint::FontMetrics metrics;
    paint.getFontMetrics(&metrics);
    m_item.font->x_ppem = metrics.fXPPEm;
    m_item.font->y_ppem = metrics.fYPPEm;
    m_item.font->x_scale = metrics.fXScale;
    m_item.font->y_scale = metrics.fYScale;

}

HB_FontRec* TextRunWalker::allocHarfbuzzFont()
{
    HB_FontRec* font = reinterpret_cast<HB_FontRec*>(fastMalloc(sizeof(HB_FontRec)));
    memset(font, 0, sizeof(HB_FontRec));
    font->klass = &harfbuzzSkiaClass;
    font->userData = 0;

    return font;
}

void TextRunWalker::deleteGlyphArrays()
{
    delete[] m_item.glyphs;
    delete[] m_item.attributes;
    delete[] m_item.advances;
    delete[] m_item.offsets;
    delete[] m_glyphs16;
    delete[] m_xPositions;
}

void TextRunWalker::createGlyphArrays(int size)
{
    m_item.glyphs = new HB_Glyph[size];
    m_item.attributes = new HB_GlyphAttributes[size];
    m_item.advances = new HB_Fixed[size];
    m_item.offsets = new HB_FixedPoint[size];

    m_glyphs16 = new uint16_t[size];
    m_xPositions = new SkScalar[size];

    m_item.num_glyphs = size;
    m_glyphsArrayCapacity = size; // Save the GlyphArrays size.
    resetGlyphArrays();
}

void TextRunWalker::resetGlyphArrays()
{
    int size = m_item.num_glyphs;
    // All the types here don't have pointers. It is safe to reset to
    // zero unless Harfbuzz breaks the compatibility in the future.
    memset(m_item.glyphs, 0, size * sizeof(HB_Glyph));
    memset(m_item.attributes, 0, size * sizeof(HB_GlyphAttributes));
    memset(m_item.advances, 0, size * sizeof(HB_Fixed));
    memset(m_item.offsets, 0, size * sizeof(HB_FixedPoint));
    memset(m_glyphs16, 0, size * sizeof(uint16_t));
    memset(m_xPositions, 0, size * sizeof(SkScalar));
}

void TextRunWalker::shapeGlyphs()
{
    // HB_ShapeItem() resets m_item.num_glyphs. If the previous call to
    // HB_ShapeItem() used less space than was available, the capacity of
    // the array may be larger than the current value of m_item.num_glyphs. 
    // So, we need to reset the num_glyphs to the capacity of the array.
    m_item.num_glyphs = m_glyphsArrayCapacity;
    resetGlyphArrays();
    while (!HB_ShapeItem(&m_item)) {
        // We overflowed our arrays. Resize and retry.
        // HB_ShapeItem fills in m_item.num_glyphs with the needed size.
        deleteGlyphArrays();
        // The |+ 1| here is a workaround for a bug in Harfbuzz: the Khmer
        // shaper (at least) can fail because of insufficient glyph buffers
        // and request 0 additional glyphs: throwing us into an infinite
        // loop.
        createGlyphArrays(m_item.num_glyphs + 1);
    }
}

void TextRunWalker::setGlyphXPositions(bool isRTL)
{
    const double rtlFlip = isRTL ? -1 : 1;
    double position = 0;

    // logClustersIndex indexes logClusters for the first codepoint of the current glyph.
    // Each time we advance a glyph, we skip over all the codepoints that contributed to the current glyph.
    int logClustersIndex = 0;

    // Iterate through the glyphs in logical order, flipping for RTL where necessary.
    // Glyphs are positioned starting from m_offsetX; in RTL mode they go leftwards from there.
    for (size_t i = 0; i < m_item.num_glyphs; ++i) {
        while (static_cast<unsigned>(logClustersIndex) < m_item.item.length && logClusters()[logClustersIndex] < i)
            logClustersIndex++;

        // If the current glyph is just after a space, add in the word spacing.
        position += determineWordBreakSpacing(logClustersIndex);

        m_glyphs16[i] = m_item.glyphs[i];
        double offsetX = truncateFixedPointToInteger(m_item.offsets[i].x);
        double advance = truncateFixedPointToInteger(m_item.advances[i]);
        if (isRTL)
            offsetX -= advance;

        m_xPositions[i] = m_offsetX + (position * rtlFlip) + offsetX;

        //if (m_currentFontData->isZeroWidthSpaceGlyph(m_glyphs16[i]))
        //    continue;

        // At the end of each cluster, add in the letter spacing.
        if (i + 1 == m_item.num_glyphs || m_item.attributes[i + 1].clusterStart)
            position += m_letterSpacing;

        position += advance;
    }

    m_pixelWidth = std::max(position, 0.0);
    m_offsetX += m_pixelWidth * rtlFlip;
}

void TextRunWalker::normalizeSpacesAndMirrorChars(const UChar* source, bool rtl, UChar* destination, int length)
{
    int position = 0;
    bool error = false;
    // Iterate characters in source and mirror character if needed.
    while (position < length) {
        UChar32 character;
        int nextPosition = position;
        U16_NEXT(source, nextPosition, length, character);
        if (Font::treatAsSpace(character))
            character = ' ';
        else if (Font::treatAsZeroWidthSpace(character))
            character = zeroWidthSpace;
        else if (rtl)
            character = u_charMirror(character);
        U16_APPEND(destination, position, length, character, error);
        ASSERT(!error);
        position = nextPosition;
    }
}

const TextRun& TextRunWalker::getNormalizedTextRun(const TextRun& originalRun, OwnPtr<TextRun>& normalizedRun, OwnArrayPtr<UChar>& normalizedBuffer)
{
    // Normalize the text run in three ways:
    // 1) Convert the |originalRun| to NFC normalized form if combining diacritical marks
    // (U+0300..) are used in the run. This conversion is necessary since most OpenType
    // fonts (e.g., Arial) don't have substitution rules for the diacritical marks in
    // their GSUB tables.
    //
    // Note that we don't use the icu::Normalizer::isNormalized(UNORM_NFC) API here since
    // the API returns FALSE (= not normalized) for complex runs that don't require NFC
    // normalization (e.g., Arabic text). Unless the run contains the diacritical marks,
    // Harfbuzz will do the same thing for us using the GSUB table.
    // 2) Convert spacing characters into plain spaces, as some fonts will provide glyphs
    // for characters like '\n' otherwise.
    // 3) Convert mirrored characters such as parenthesis for rtl text.
 
    // Convert to NFC form if the text has diacritical marks.
    icu::UnicodeString normalizedString;
    UErrorCode error = U_ZERO_ERROR;

    for (int16_t i = 0; i < originalRun.length(); ++i) {
        UChar ch = originalRun[i];
        if (::ublock_getCode(ch) == UBLOCK_COMBINING_DIACRITICAL_MARKS) {
            icu::Normalizer::normalize(icu::UnicodeString(originalRun.characters(),
                                       originalRun.length()), UNORM_NFC, 0 /* no options */,
                                       normalizedString, error);
            if (U_FAILURE(error))
                return originalRun;
            break;
        }
    }

    // Normalize space and mirror parenthesis for rtl text.
    int normalizedBufferLength;
    const UChar* sourceText;
    if (normalizedString.isEmpty()) {
        normalizedBufferLength = originalRun.length();
        sourceText = originalRun.characters();
    } else {
        normalizedBufferLength = normalizedString.length();
        sourceText = normalizedString.getBuffer();
    }

    normalizedBuffer.set(new UChar[normalizedBufferLength + 1]);

    normalizeSpacesAndMirrorChars(sourceText, originalRun.rtl(), normalizedBuffer.get(), normalizedBufferLength);

    normalizedRun.set(new TextRun(originalRun));
    normalizedRun->setText(normalizedBuffer.get(), normalizedBufferLength);
    return *normalizedRun;
}

static void setupForTextPainting(SkPaint* paint, SkColor color)
{
    paint->setTextEncoding(SkPaint::kGlyphID_TextEncoding);
    paint->setColor(color);
}

struct GlyphDataBlackBerry {
    Vector<unsigned short, 2048> m_glyphs;
    Vector<SkScalar, 2048> m_xPositions;
};

void collectGlyphDataByLogClusterIndex(const TextRunWalker& walker, int logClusterIndexFrom, int logClusterIndexTo, GlyphDataBlackBerry& glyphData)
{
    for (int i = logClusterIndexFrom; i < logClusterIndexTo; ++i) {
        int index = walker.logClusters()[i];
        glyphData.m_glyphs.append(walker.glyphs()[index]);
        glyphData.m_xPositions.append(walker.xPositions()[index]);
    }
}

void Font::drawComplexText(GraphicsContext* gc, const TextRun& run,
                           const FloatPoint& point, int from, int to) const
{
    if (!run.length())
        return;

    SkCanvas* canvas = gc->platformContext()->canvas();
    int textMode = gc->platformContext()->getTextDrawingMode();
    bool fill = textMode & cTextFill;
    bool stroke = (textMode & cTextStroke)
               && gc->platformContext()->getStrokeStyle() != NoStroke
               && gc->platformContext()->getStrokeThickness() > 0;

    if (!fill && !stroke)
        return;

    SkPaint strokePaint, fillPaint;
    if (fill) {
        gc->platformContext()->setupPaintForFilling(&fillPaint);
        setupForTextPainting(&fillPaint, gc->fillColor().rgb());
    }
    if (stroke) {
        gc->platformContext()->setupPaintForStroking(&strokePaint, 0, 0);
        setupForTextPainting(&strokePaint, gc->strokeColor().rgb());
    }

    TextRunWalker walker(run, point.x(), this);
    walker.setWordSpacingAdjustment(wordSpacing());
    walker.setLetterSpacingAdjustment(letterSpacing());
    walker.setPadding(run.padding());

    if (run.rtl()) {
        // FIXME: this causes us to shape the text twice -- once to compute the width and then again
        // below when actually rendering.  Change ComplexTextController to match platform/mac and
        // platform/chromium/win by having it store the shaped runs, so we can reuse the results.
        walker.reset(point.x() + walker.widthOfFullRun());
        // We need to set the padding again because ComplexTextController layout consumed the value.
        // Fixing the above problem would help here too.
        walker.setPadding(run.padding());
    }

    ASSERT(0 <= from && from < to);
    bool foundStart = false;
    int leftCodePoints = to - from;
    while (from >= 0 && leftCodePoints > 0 && walker.nextScriptRun()) {
        int fromInScript = 0;
        int toInScript = walker.numCodePoints();

        if (!foundStart) {
            if (static_cast<unsigned>(from) < walker.numCodePoints()) {
                foundStart = true;
                fromInScript = from;
            } else {
                from -= walker.numCodePoints();
                continue;
            }
        }

        if (toInScript - fromInScript > leftCodePoints)
            toInScript = fromInScript + leftCodePoints;

        if (fill || stroke) {
            GlyphDataBlackBerry glyphData;
            collectGlyphDataByLogClusterIndex(walker, fromInScript, toInScript, glyphData);
            if (fill) {
                walker.fontPlatformDataForScriptRun()->setupPaint(&fillPaint);
                adjustTextRenderMode(&fillPaint, gc->platformContext());
                canvas->drawPosTextH(glyphData.m_glyphs.data(), glyphData.m_glyphs.size() << 1, glyphData.m_xPositions.data(), point.y(), fillPaint);
            }
            if (stroke) {
                walker.fontPlatformDataForScriptRun()->setupPaint(&strokePaint);
                adjustTextRenderMode(&strokePaint, gc->platformContext());
                canvas->drawPosTextH(glyphData.m_glyphs.data(), glyphData.m_glyphs.size() << 1, glyphData.m_xPositions.data(), point.y(), fillPaint);
            }
        }

        leftCodePoints -= toInScript - fromInScript;
    }
}

float Font::floatWidthForComplexText(const TextRun& run, HashSet<const SimpleFontData*>* /* fallbackFonts */, GlyphOverflow* /* glyphOverflow */) const
{
    TextRunWalker walker(run, 0, this);
    walker.setWordSpacingAdjustment(wordSpacing());
    walker.setLetterSpacingAdjustment(letterSpacing());
    walker.setPadding(run.padding());
    return walker.widthOfFullRun();
}

static int glyphIndexForXPositionInScriptRun(const TextRunWalker& walker, int targetX)
{
    // Iterate through the glyphs in logical order, seeing whether targetX falls between the previous
    // position and halfway through the current glyph.
    // FIXME: this code probably belongs in ComplexTextController.
    int lastX = walker.offsetX() - (walker.rtl() ? -walker.width() : walker.width());
    for (int glyphIndex = 0; static_cast<unsigned>(glyphIndex) < walker.length(); ++glyphIndex) {
        int advance = truncateFixedPointToInteger(walker.advances()[glyphIndex]);
        int nextX = static_cast<int>(walker.xPositions()[glyphIndex]) + advance / 2;
        if (std::min(nextX, lastX) <= targetX && targetX <= std::max(nextX, lastX))
            return glyphIndex;
        lastX = nextX;
    }

    return walker.length() - 1;
}

// Return the code point index for the given |x| offset into the text run.
int Font::offsetForPositionForComplexText(const TextRun& run, float xFloat,
                                          bool includePartialGlyphs) const
{
    // FIXME: This truncation is not a problem for HTML, but only affects SVG, which passes floating-point numbers
    // to Font::offsetForPosition(). Bug http://webkit.org/b/40673 tracks fixing this problem.
    int targetX = static_cast<int>(xFloat);

    // (Mac code ignores includePartialGlyphs, and they don't know what it's
    // supposed to do, so we just ignore it as well.)
    TextRunWalker walker(run, 0, this);
    walker.setWordSpacingAdjustment(wordSpacing());
    walker.setLetterSpacingAdjustment(letterSpacing());
    walker.setPadding(run.padding());
    if (run.rtl()) {
        // See FIXME in drawComplexText.
        walker.reset(walker.widthOfFullRun());
        walker.setPadding(run.padding());

    }

    unsigned basePosition = 0;

    int x = walker.offsetX();
    while (walker.nextScriptRun()) {
        int nextX = walker.offsetX();

        if (std::min(x, nextX) <= targetX && targetX <= std::max(x, nextX)) {
            // The x value in question is within this script run.
            const int glyphIndex = glyphIndexForXPositionInScriptRun(walker, targetX);

            // Now that we have a glyph index, we have to turn that into a
            // code-point index. Because of ligatures, several code-points may
            // have gone into a single glyph. We iterate over the clusters log
            // and find the first code-point which contributed to the glyph.

            // Some shapers (i.e. Khmer) will produce cluster logs which report
            // that /no/ code points contributed to certain glyphs. Because of
            // this, we take any code point which contributed to the glyph in
            // question, or any subsequent glyph. If we run off the end, then
            // we take the last code point.
            const unsigned short* log = walker.logClusters();
            for (unsigned j = 0; j < walker.numCodePoints(); ++j) {
                if (log[j] >= glyphIndex)
                    return basePosition + j;
            }

            return basePosition + walker.numCodePoints() - 1;
        }

        basePosition += walker.numCodePoints();
    }

    return basePosition;
}

// Return the rectangle for selecting the given range of code-points in the TextRun.
FloatRect Font::selectionRectForComplexText(const TextRun& run,
                                            const FloatPoint& point, int height,
                                            int from, int to) const
{
    int fromX = -1, toX = -1;
    TextRunWalker walker(run, 0, this);
    walker.setWordSpacingAdjustment(wordSpacing());
    walker.setLetterSpacingAdjustment(letterSpacing());
    walker.setPadding(run.padding());
    if (run.rtl()) {
        // See FIXME in drawComplexText.
        walker.reset(walker.widthOfFullRun());
        walker.setPadding(run.padding());
    }

    // Iterate through the script runs in logical order, searching for the run covering the positions of interest.
    while (walker.nextScriptRun() && (fromX == -1 || toX == -1)) {
        if (fromX == -1 && from >= 0 && static_cast<unsigned>(from) < walker.numCodePoints()) {
            // |from| is within this script run. So we index the clusters log to
            // find which glyph this code-point contributed to and find its x
            // position.
            int glyph = walker.logClusters()[from];
            fromX = walker.xPositions()[glyph];
            if (walker.rtl())
                fromX += truncateFixedPointToInteger(walker.advances()[glyph]);
        } else
            from -= walker.numCodePoints();

        if (toX == -1 && to >= 0 && static_cast<unsigned>(to) < walker.numCodePoints()) {
            int glyph = walker.logClusters()[to];
            toX = walker.xPositions()[glyph];
            if (walker.rtl())
                toX += truncateFixedPointToInteger(walker.advances()[glyph]);
        } else
            to -= walker.numCodePoints();
    }

    // The position in question might be just after the text.
    if (fromX == -1)
        fromX = walker.offsetX();
    if (toX == -1)
        toX = walker.offsetX();

    ASSERT(fromX != -1 && toX != -1);

    if (fromX < toX)
        return FloatRect(point.x() + fromX, point.y(), toX - fromX, height);

    return FloatRect(point.x() + toX, point.y(), fromX - toX, height);
}

} // namespace WebCore
