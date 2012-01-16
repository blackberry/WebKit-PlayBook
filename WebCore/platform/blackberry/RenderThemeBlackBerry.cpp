/*
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 * Copyright (C) 2006, 2007 Apple Inc.
 * Copyright (C) 2009 Google Inc.
 */

#include "config.h"
#include "RenderThemeBlackBerry.h"

#include "ColorSpace.h"
#include "CSSValueKeywords.h"
#include "Frame.h"
#include "Gradient.h"
#include "GraphicsContext.h"
#include "HTMLMediaElement.h"
#include "MediaControlElements.h"
#include "MediaPlayerPrivateMMrenderer.h"
#include "Path.h"
#include "RenderSlider.h"
#include "RenderView.h"
#include "UserAgentStyleSheets.h"

namespace WebCore {

// Sizes (px)
const unsigned smallRadius = 1;
const unsigned largeRadius = 3;
const unsigned lineWidth = 1;
const int marginSize = 4;
const int mediaSliderThumbWidth = 40;
const int mediaSliderThumbHeight = 13;
const int mediaSliderThumbRadius = 5;
const int sliderThumbWidth = 15;
const int sliderThumbHeight = 25;

// Checkbox check scalers
const float checkboxLeftX = 7 / 40.0;
const float checkboxLeftY = 1 / 2.0;
const float checkboxMiddleX = 19 / 50.0;
const float checkboxMiddleY = 7 / 25.0;
const float checkboxRightX = 33 / 40.0;
const float checkboxRightY = 1 / 5.0;
const float checkboxStrokeThickness = 6.5;

// Radio button scaler
const float radioButtonCheckStateScaler = 7 / 30.0;

// Multipliers
const unsigned paddingDivisor = 5;

// Colors
const RGBA32 caretBottom = 0xff2163bf;
const RGBA32 caretTop = 0xff69a5fa;

const RGBA32 regularBottom = 0xffdcdee4;
const RGBA32 regularTop = 0xfff7f2ee;
const RGBA32 hoverBottom = 0xffb5d3fc;
const RGBA32 hoverTop = 0xffcceaff;
const RGBA32 depressedBottom = 0xff3388ff;
const RGBA32 depressedTop = 0xff66a0f2;
const RGBA32 disabledBottom = 0xffe7e7e7;
const RGBA32 disabledTop = 0xffefefef;

const RGBA32 regularBottomOutline = 0xff6e7073;
const RGBA32 regularTopOutline = 0xffb9b8b8;
const RGBA32 hoverBottomOutline = 0xff2163bf;
const RGBA32 hoverTopOutline = 0xff69befa;
const RGBA32 depressedBottomOutline = 0xff0c3d81;
const RGBA32 depressedTopOutline = 0xff1d4d70;
const RGBA32 disabledOutline = 0xffd5d9de;

const RGBA32 rangeSliderRegularBottom = 0xfff6f2ee;
const RGBA32 rangeSliderRegularTop = 0xffdee0e5;
const RGBA32 rangeSliderRollBottom = 0xffc9e8fe;
const RGBA32 rangeSliderRollTop = 0xffb5d3fc;

const RGBA32 rangeSliderRegularBottomOutline = 0xffb9babd;
const RGBA32 rangeSliderRegularTopOutline = 0xffb7b7b7;
const RGBA32 rangeSliderRollBottomOutline = 0xff67abe0;
const RGBA32 rangeSliderRollTopOutline = 0xff69adf9;

const RGBA32 dragRegularLight = 0xfffdfdfd;
const RGBA32 dragRegularDark = 0xffbababa;
const RGBA32 dragRollLight = 0xfff2f2f2;
const RGBA32 dragRollDark = 0xff69a8ff;

const RGBA32 selection = 0xff2b8fff;

const RGBA32 blackPen = Color::black;
const RGBA32 focusRingPen = 0xffa3c8fe;

// We aim to match IE here.
// -IE uses a font based on the encoding as the default font for form controls.
// -Gecko uses MS Shell Dlg (actually calls GetStockObject(DEFAULT_GUI_FONT),
// which returns MS Shell Dlg)
// -Safari uses Lucida Grande.
//
// FIXME: The only case where we know we don't match IE is for ANSI encodings.
// IE uses MS Shell Dlg there, which we render incorrectly at certain pixel
// sizes (e.g. 15px). So, for now we just use Arial.
const String& RenderThemeBlackBerry::defaultGUIFont()
{
    DEFINE_STATIC_LOCAL(String, fontFace, ("Arial"));
    return fontFace;
}

float RenderThemeBlackBerry::defaultFontSize = 16.0;

static PassRefPtr<Gradient> createLinearGradient(RGBA32 top, RGBA32 bottom, const IntPoint& a, const IntPoint& b)
{
    RefPtr<Gradient> gradient = Gradient::create(a, b);
    gradient->addColorStop(0.0, Color(top));
    gradient->addColorStop(1.0, Color(bottom));
    return gradient.release();
}

PassRefPtr<RenderTheme> RenderTheme::themeForPage(Page* page)
{
    static RenderTheme* theme = RenderThemeBlackBerry::create().releaseRef();
    return theme;
}

PassRefPtr<RenderTheme> RenderThemeBlackBerry::create()
{
    return adoptRef(new RenderThemeBlackBerry());
}

RenderThemeBlackBerry::RenderThemeBlackBerry()
    : RenderTheme()
#if ENABLE(BLACKBERRY_CARET_APPEARANCE)
    , m_shouldRepaintVerticalCaret(false)
    , m_caretHighlightStyle(Olympia::Platform::HighlightCharacterCaret)
#endif
{
}

RenderThemeBlackBerry::~RenderThemeBlackBerry()
{
}

String RenderThemeBlackBerry::extraDefaultStyleSheet()
{
    return String(themeBlackBerryUserAgentStyleSheet, sizeof(themeBlackBerryUserAgentStyleSheet));
}

#if ENABLE(VIDEO)
String RenderThemeBlackBerry::extraMediaControlsStyleSheet()
{
    return String(mediaControlsBlackBerryUserAgentStyleSheet, sizeof(mediaControlsBlackBerryUserAgentStyleSheet));
}
#endif

double RenderThemeBlackBerry::caretBlinkInterval() const
{
    return 0.0; // Turn off caret blinking.
}

#if ENABLE(BLACKBERRY_CARET_APPEARANCE)
void RenderThemeBlackBerry::adjustTextColorForCaretMarker(Color& color) const
{
    if (m_caretHighlightStyle == Olympia::Platform::HighlightCharacterCaret)
        color = Color::white;
}

static PassRefPtr<Range> computeCaretCharacterRange(Frame* frame)
{
    // We want to select the next character with respect to the start of the current selection.
    VisibleSelection startOfCurrentSelection(frame->selection()->start(), frame->selection()->affinity());

    SelectionController trialSelection;
    trialSelection.setSelection(startOfCurrentSelection);
    trialSelection.modify(SelectionController::AlterationExtend, SelectionController::DirectionForward, CharacterGranularity);
    return trialSelection.toNormalizedRange();
}

static inline float computeVerticalCaretWidth(const Font& font)
{
    const TextRun spaceRun(" ");
    return font.floatWidth(spaceRun);
}

static void addCaretMarkerAndRepaint(const Frame* frame, Range* caretTextRange)
{
    ASSERT(caretTextRange->endOffset() - caretTextRange->startOffset() <= 1); // The caret marker highlights at most one character.

    ExceptionCode ec = 0;
    caretTextRange->startContainer(ec)->document()->markers()->addMarker(caretTextRange, DocumentMarker::Caret);
    ASSERT(!ec);
    if (frame->selection()->caretRenderer())
        frame->selection()->caretRenderer()->repaint();
}

static void paintSolidCaret(GraphicsContext* context, const FloatRect& caretRect)
{
    IntRect caretIntRect(caretRect); // We do not need to be precise when creating the gradient.
    context->save();
    context->setFillGradient(createLinearGradient(caretTop, caretBottom, caretIntRect.topRight(), caretIntRect.bottomRight()));
    context->fillRect(caretRect);
    context->restore();
}

static void paintOutlineCaret(GraphicsContext* context, const FloatRect& caretRect)
{
    context->save();
    context->setStrokeStyle(SolidStroke);

    // A stroke thickness of 0.25 seems to give a reasonable outline while not occluding narrow characters.
    context->setStrokeThickness(0.25);

    context->setStrokeColor(Color::black, ColorSpaceDeviceRGB);
    context->strokeRect(caretRect);
    context->restore();
}

static bool shouldRepaintVerticalCaret(Range* caretTextRange)
{
    // We repaint the vertical caret for line breaks so that we match the BlackBerry OS.
    ExceptionCode ec = 0;
    String nextCharacter = caretTextRange->toString(ec); // Will be empty for a <br/> in a content-editable HTML element.
    ASSERT(!ec);
    return caretTextRange->startOffset() == caretTextRange->endOffset() || nextCharacter.isEmpty() || nextCharacter[0] == '\n';
}

void RenderThemeBlackBerry::paintCaret(GraphicsContext* context, const IntRect& caretRect, const Element* rootEditableElement)
{
    if (m_caretHighlightStyle == Olympia::Platform::NoCaret)
        return; // The caret does not have a visual appearance.

    Frame* frame = rootEditableElement && rootEditableElement->document() ? rootEditableElement->document()->frame() : 0;
    if (!frame)
        return;

    SelectionController* selection = frame->selection();
    if (!selection || !selection->start().node())
        return;

    RefPtr<Range> caretTextRange = computeCaretCharacterRange(frame);
    if (!caretTextRange)
        return;

    // Because RenderThemeBlackBerry::repaintCaret may cause a repaint of a node (such as when we
    // add the caret marker) we may get called even though the caret marker was already added and
    // painted (i.e. absCaretTextBoundingBox == m_oldAbsoluteCaretTextBoundingBox). So, we do not
    // need to add a caret marker/paint it again.
    IntRect absCaretTextBoundingBox = selection->caretRenderer() ? selection->caretRenderer()->localToAbsoluteQuad(FloatRect(caretTextRange->boundingBox())).enclosingBoundingBox() : IntRect();
    bool canAddCaretMarker = absCaretTextBoundingBox != m_oldAbsoluteCaretTextBoundingBox;

    ExceptionCode ec = 0;
    m_oldAbsoluteCaretTextBoundingBox = absCaretTextBoundingBox;
    ASSERT(!ec);

    if (shouldRepaintVerticalCaret(caretTextRange.get())) {
        // Paint the vertical caret.
        if (rootEditableElement->renderer()) {
            Font caretFont = rootEditableElement->renderer()->style()->font();
            FloatRect verticalCaretRect(caretRect);
            verticalCaretRect.setWidth(computeVerticalCaretWidth(caretFont));
            paintSolidCaret(context, verticalCaretRect);
            m_shouldRepaintVerticalCaret = true;
        }
    } else if (m_caretHighlightStyle == Olympia::Platform::ThinVerticalCaret) {
        paintSolidCaret(context, caretRect); // caretRect is 1 px wide.
        m_shouldRepaintVerticalCaret = true;
    } else if (canAddCaretMarker)
        addCaretMarkerAndRepaint(frame, caretTextRange.get());
}

void RenderThemeBlackBerry::repaintCaret(RenderView* view, const IntRect& caretRect, CaretVisibility caretVisibility)
{
    // Notice, this method is always called twice by SelectionController on a repaint. The first time
    // is to clear the old caret rectangle. The second time is to paint the new caret rectangle.
    Frame* frame = view && view->frameView() ? view->frameView()->frame() : 0;
    if (!frame)
        return;

    // FIXME: We only need to clear the caret marker when either we switch to the vertical caret or
    // caretVisibility == RenderTheme::CaretHidden. For the case where the caret marker is moved we
    // can compute the move and update the caret marker's position, instead of removing it then
    // adding a new marker.
    if (frame->document())
        frame->document()->markers()->removeMarkers(DocumentMarker::Caret);

    SelectionController* selection = frame->selection();
    if (!selection || !selection->start().node())
        return;

    RefPtr<Range> caretTextRange = computeCaretCharacterRange(frame);
    if (!caretTextRange)
        return;

    ExceptionCode ec = 0;
    m_oldAbsoluteCaretTextBoundingBox = selection->caretRenderer() ? selection->caretRenderer()->localToAbsoluteQuad(FloatRect(caretTextRange->boundingBox())).enclosingBoundingBox() : IntRect();
    ASSERT(!ec);

    bool doesCaretHaveAppearance = m_caretHighlightStyle != Olympia::Platform::NoCaret;

    // The order of the following if-statements have been explicitly chosen so that we 1) either repaint
    // the vertical/thin vertical caret or the caret marker (but not both) and 2) always repaint the
    // vertical/thin vertical caret rectangle so as to clear it should we switch to the caret marker
    // (indicated by m_shouldRepaintVerticalCaret).
    bool willRepaintVerticalCaret = shouldRepaintVerticalCaret(caretTextRange.get());
    // FIXME: We should rename m_shouldRepaintVerticalCaret to m_lastCaretWasVerticalCaret.
    if (!selection->isCaret() || m_shouldRepaintVerticalCaret || willRepaintVerticalCaret) {
        // Repaint the vertical caret. When !selection->isCaret() then the vertical caret must be cleared
        // because we may have started a selection from the end of the text field.
        Element* rootEditableElement = selection->rootEditableElement();
        if (rootEditableElement && rootEditableElement->renderer()) {
            IntRect verticalCaretRepaintRect = caretRect;
            if (doesCaretHaveAppearance && m_caretHighlightStyle != Olympia::Platform::ThinVerticalCaret) {
                // Paint our wider-style vertical caret.
                // We need to re-compute the repaint rect for the caret since the width is larger than the standard WebKit caret.
                int verticalCaretWidth = lroundf(computeVerticalCaretWidth(rootEditableElement->renderer()->style()->font()));
                verticalCaretRepaintRect.setWidth(verticalCaretWidth);
                verticalCaretRepaintRect = SelectionController::repaintRectForCaret(verticalCaretRepaintRect);
            }
            view->repaintViewRectangle(verticalCaretRepaintRect, false);
        }
        m_shouldRepaintVerticalCaret = false;
    }

    if (!selection->isCaret())
        return;

    if (m_shouldRepaintVerticalCaret || willRepaintVerticalCaret)
        return;

    if (doesCaretHaveAppearance && caretVisibility == RenderTheme::CaretVisible)
        addCaretMarkerAndRepaint(frame, caretTextRange.get());
}

// Called by InlineTextBox::paintCaretMarker.
void RenderThemeBlackBerry::paintCaretMarker(GraphicsContext* context, const FloatRect& caretRect, const Font&)
{
    switch (m_caretHighlightStyle) {
    case Olympia::Platform::NoCaret:
        break;
    case Olympia::Platform::HighlightCharacterCaret:
        paintSolidCaret(context, caretRect);
        break;
    case Olympia::Platform::OutlineCharacterCaret:
        paintOutlineCaret(context, caretRect);
        break;
    case Olympia::Platform::ThinVerticalCaret:
        break; // The thin vertical caret does not stylize the caret marker text.
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}
#endif

void RenderThemeBlackBerry::systemFont(int propId, FontDescription& fontDescription) const
{
    float fontSize = defaultFontSize;

    switch (propId) {
    case CSSValueWebkitMiniControl:
    case CSSValueWebkitSmallControl:
    case CSSValueWebkitControl:
        // Why 2 points smaller? Because that's what Gecko does. Note that we
        // are assuming a 96dpi screen, which is the default that we use on
        // Windows.
        static const float pointsPerInch = 72.0f;
        static const float pixelsPerInch = 96.0f;
        fontSize -= (2.0f / pointsPerInch) * pixelsPerInch;
        break;
    }

    fontDescription.firstFamily().setFamily(defaultGUIFont());
    fontDescription.setSpecifiedSize(fontSize);
    fontDescription.setIsAbsoluteSize(true);
    fontDescription.setGenericFamily(FontDescription::NoFamily);
    fontDescription.setWeight(FontWeightNormal);
    fontDescription.setItalic(false);
}

void RenderThemeBlackBerry::setButtonStyle(RenderStyle* style) const
{
    Length vertPadding(int(style->fontSize() / paddingDivisor), Fixed);
    style->setPaddingTop(vertPadding);
    style->setPaddingBottom(vertPadding);
}

void RenderThemeBlackBerry::adjustButtonStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    setButtonStyle(style);
    style->setCursor(CURSOR_WEBKIT_GRAB);
}

void RenderThemeBlackBerry::adjustTextAreaStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    setButtonStyle(style);
}

bool RenderThemeBlackBerry::paintTextArea(RenderObject* object, const PaintInfo& info, const IntRect& rect)
{
    return paintTextFieldOrTextAreaOrSearchField(object, info, rect);
}

void RenderThemeBlackBerry::adjustTextFieldStyle(CSSStyleSelector* css, RenderStyle* style, Element* e) const
{
    setButtonStyle(style);
}

bool RenderThemeBlackBerry::paintTextFieldOrTextAreaOrSearchField(RenderObject* object, const PaintInfo& info, const IntRect& rect)
{
    ASSERT(info.context);
    GraphicsContext* context = info.context;

    context->save();
    context->setStrokeStyle(SolidStroke);
    context->setStrokeThickness(lineWidth);
    if (!isEnabled(object))
        context->setStrokeColor(disabledOutline, ColorSpaceDeviceRGB);
    else if (isPressed(object))
        info.context->setStrokeGradient(createLinearGradient(depressedTopOutline, depressedBottomOutline, rect.topRight(), rect.bottomRight()));
    else if (isHovered(object) || isFocused(object))
        context->setStrokeGradient(createLinearGradient(hoverTopOutline, hoverBottomOutline, rect.topRight(), rect.bottomRight()));
    else
        context->setStrokeGradient(createLinearGradient(regularTopOutline, regularBottomOutline, rect.topRight(), rect.bottomRight()));

    context->beginPath();
    Path textFieldRoundedRectangle;
    textFieldRoundedRectangle.addRoundedRect(rect, FloatSize(largeRadius, largeRadius));
    context->addPath(textFieldRoundedRectangle);
    if (object->style()->appearance() == SearchFieldPart) {
        // We force the fill color to White so as to match the background color of the search cancel button graphic.
        context->setFillColor(Color::white, ColorSpaceDeviceRGB);
        context->drawPath();
    } else
        context->strokePath();
    context->restore();
    return false;
}

bool RenderThemeBlackBerry::paintTextField(RenderObject* object, const PaintInfo& info, const IntRect& rect)
{
    return paintTextFieldOrTextAreaOrSearchField(object, info, rect);
}

void RenderThemeBlackBerry::adjustSearchFieldStyle(CSSStyleSelector* css, RenderStyle* style, Element* e) const
{
    setButtonStyle(style);
}

void RenderThemeBlackBerry::adjustSearchFieldCancelButtonStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    static const float defaultControlFontPixelSize = 13;
    static const float defaultCancelButtonSize = 9;
    static const float minCancelButtonSize = 5;
    static const float maxCancelButtonSize = 21;

    // Scale the button size based on the font size
    float fontScale = style->fontSize() / defaultControlFontPixelSize;
    int cancelButtonSize = lroundf(std::min(std::max(minCancelButtonSize, defaultCancelButtonSize * fontScale), maxCancelButtonSize));
    style->setWidth(Length(cancelButtonSize, Fixed));
    style->setHeight(Length(cancelButtonSize, Fixed));
}

bool RenderThemeBlackBerry::paintSearchField(RenderObject* object, const PaintInfo& info, const IntRect& rect)
{
    return paintTextFieldOrTextAreaOrSearchField(object, info, rect);
}

bool RenderThemeBlackBerry::paintSearchFieldCancelButton(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
    IntRect bounds = rect;
    ASSERT(object->parent());
    if (!object->parent() || !object->parent()->isBox())
        return false;

    RenderBox* parentRenderBox = toRenderBox(object->parent());

    IntRect parentBox = parentRenderBox->absoluteContentBox();

    // Make sure the scaled button stays square and will fit in its parent's box
    bounds.setHeight(std::min(parentBox.width(), std::min(parentBox.height(), bounds.height())));
    bounds.setWidth(bounds.height());

    // Center the button vertically.  Round up though, so if it has to be one pixel off-center, it will
    // be one pixel closer to the bottom of the field.  This tends to look better with the text.
    bounds.setY(parentBox.y() + (parentBox.height() - bounds.height() + 1) / 2);

    static Image* cancelImage = Image::loadPlatformResource("searchCancel").releaseRef();
    static Image* cancelPressedImage = Image::loadPlatformResource("searchCancelPressed").releaseRef();
    paintInfo.context->drawImage(isPressed(object) ? cancelPressedImage : cancelImage, object->style()->colorSpace(), bounds);
    return false;
}

void RenderThemeBlackBerry::adjustMenuListButtonStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    const int paddingLeft = 8; // From observation, seems to produce a reasonable result
    const int paddingRight = 4; // From observation, seems to produce a reasonable result
    int height = 0;
    const int minHeight = style->fontSize() * 2;

    style->resetPadding();
    style->setMinHeight(Length(minHeight, Fixed));

    if (style->height().isFixed() && (height = style->height().value()) > minHeight)
        style->setPaddingRight(Length(height + paddingRight, Fixed)); 
    else {
        style->setMaxHeight(Length(minHeight, Fixed));
        style->setPaddingRight(Length(minHeight + paddingRight, Fixed));
    }

    style->setPaddingLeft(Length(paddingLeft, Fixed));
    style->setCursor(CURSOR_WEBKIT_GRAB);
}

void RenderThemeBlackBerry::calculateButtonSize(RenderStyle* style) const
{
    int size = style->fontSize();
    if (style->appearance() == CheckboxPart || style->appearance() == RadioPart) {
        style->setWidth(Length(size, Fixed));
        style->setHeight(Length(size, Fixed));
        return;
    }

    // If the width and height are both specified, then we have nothing to do.
    if (!style->width().isIntrinsicOrAuto() && !style->height().isAuto())
        return;

    if (style->width().isIntrinsicOrAuto())
        style->setWidth(Length(size, Fixed));

    if (style->height().isAuto())
        style->setHeight(Length(size, Fixed));
}

bool RenderThemeBlackBerry::paintCheckbox(RenderObject* object, const PaintInfo& info, const IntRect& rect)
{
    return paintButton(object, info, rect);
}

void RenderThemeBlackBerry::setCheckboxSize(RenderStyle* style) const
{
    calculateButtonSize(style);
}

bool RenderThemeBlackBerry::paintRadio(RenderObject* object, const PaintInfo& info, const IntRect& rect)
{
    return paintButton(object, info, rect);
}

void RenderThemeBlackBerry::setRadioSize(RenderStyle* style) const
{
    calculateButtonSize(style);
}

// If this function returns false, WebCore assumes the button is fully decorated
bool RenderThemeBlackBerry::paintButton(RenderObject* object, const PaintInfo& info, const IntRect& rect)
{
    Path path;
    Color check(blackPen);

    ASSERT(info.context);
    info.context->save();

    info.context->setStrokeStyle(SolidStroke);
    info.context->setStrokeThickness(lineWidth);

    if (!isEnabled(object)) {
        info.context->setFillGradient(createLinearGradient(disabledTop, disabledBottom, rect.topRight(), rect.bottomRight()));
        info.context->setStrokeColor(disabledOutline, ColorSpaceDeviceRGB);
        check = Color(blackPen);
    } else if (isPressed(object)) {
        info.context->setFillGradient(createLinearGradient(depressedTop, depressedBottom, rect.topRight(), rect.bottomRight()));
        info.context->setStrokeGradient(createLinearGradient(depressedTopOutline, depressedBottomOutline, rect.topRight(), rect.bottomRight()));
    } else if (isHovered(object)) {
        info.context->setFillGradient(createLinearGradient(hoverTop, hoverBottom, rect.topRight(), rect.bottomRight()));
        info.context->setStrokeGradient(createLinearGradient(hoverTopOutline, hoverBottomOutline, rect.topRight(), rect.bottomRight()));
    } else {
        info.context->setFillGradient(createLinearGradient(regularTop, regularBottom, rect.topRight(), rect.bottomRight()));
        info.context->setStrokeGradient(createLinearGradient(regularTopOutline, regularBottomOutline, rect.topRight(), rect.bottomRight()));
    }

    ControlPart part = object->style()->appearance();
    switch (part) {
    case CheckboxPart:
            {
                FloatSize smallCorner(smallRadius, smallRadius);
                info.context->beginPath();
                Path path;
                path.addRoundedRect(rect, smallCorner);
                info.context->addPath(path);
                info.context->drawPath();

                if (isChecked(object)) {
                    Path checkPath;
                    IntRect rect2 = rect;
                    rect2.inflate(-1);
                    checkPath.moveTo(FloatPoint(rect2.x() + rect2.width() * checkboxLeftX, rect2.y() + rect2.height() * checkboxLeftY));
                    checkPath.addLineTo(FloatPoint(rect2.x() + rect2.width() * checkboxMiddleX, rect2.bottom() - rect2.height() * checkboxMiddleY));
                    checkPath.addLineTo(FloatPoint(rect2.x() + rect2.width() * checkboxRightX, rect2.y() + rect2.height() * checkboxRightY));
                    info.context->setLineCap(RoundCap);
                    info.context->setStrokeColor(blackPen, ColorSpaceDeviceRGB);
                    info.context->setStrokeThickness(rect2.width() / checkboxStrokeThickness);
                    info.context->beginPath();
                    info.context->addPath(checkPath);
                    info.context->drawPath();
                }
            }
            break;
    case RadioPart:
            info.context->drawEllipse(rect);
            if (isChecked(object)) {
                IntRect rect2 = rect;
                rect2.inflate(-rect.width() * radioButtonCheckStateScaler);
                info.context->setFillColor(check, ColorSpaceDeviceRGB);
                info.context->setStrokeColor(check, ColorSpaceDeviceRGB);
                info.context->drawEllipse(rect2);
            }
            break;
    case ButtonPart:
    case PushButtonPart:
            {
                FloatSize largeCorner(largeRadius, largeRadius);
                info.context->beginPath();
                Path path;
                path.addRoundedRect(rect, largeCorner);
                info.context->addPath(path);
                info.context->drawPath();

                break;
            }
    default:
            info.context->restore();
            return true;
    }

    info.context->restore();
    return false;
}

void RenderThemeBlackBerry::adjustMenuListStyle(CSSStyleSelector* css, RenderStyle* style, Element* element) const
{
    adjustMenuListButtonStyle(css, style, element);
}

void RenderThemeBlackBerry::adjustCheckboxStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    setCheckboxSize(style);
    style->setBoxShadow(0);
    Length margin(marginSize, Fixed);
    style->setMarginBottom(margin);
    style->setMarginRight(margin);
    style->setCursor(CURSOR_WEBKIT_GRAB);
}

void RenderThemeBlackBerry::adjustRadioStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    setRadioSize(style);
    style->setBoxShadow(0);
    Length margin(marginSize, Fixed);
    style->setMarginBottom(margin);
    style->setMarginRight(margin);
    style->setCursor(CURSOR_WEBKIT_GRAB);
}

void RenderThemeBlackBerry::paintMenuListButtonGradientAndArrow(GraphicsContext* context, RenderObject* object, IntRect buttonRect, const Path& clipPath)
{
    ASSERT(context);
    context->save();
    if (!isEnabled(object))
        context->setFillGradient(createLinearGradient(disabledTop, disabledBottom, buttonRect.topRight(), buttonRect.bottomRight()));
    else if (isPressed(object))
        context->setFillGradient(createLinearGradient(depressedTop, depressedBottom, buttonRect.topRight(), buttonRect.bottomRight()));
    else if (isHovered(object))
        context->setFillGradient(createLinearGradient(hoverTop, hoverBottom, buttonRect.topRight(), buttonRect.bottomRight()));
    else
        context->setFillGradient(createLinearGradient(regularTop, regularBottom, buttonRect.topRight(),buttonRect.bottomRight()));

    // 1. Paint the background of the button that will contain the arrow.
    context->clip(clipPath);
    context->drawRect(buttonRect);
    context->restore();

    // 2. Paint the button arrow.
    buttonRect.inflate(-buttonRect.width() / 3);
    buttonRect.move(0, buttonRect.height() * 7 / 20);
    Path path;
    path.moveTo(FloatPoint(buttonRect.x(), buttonRect.y()));
    path.addLineTo(FloatPoint(buttonRect.x() + buttonRect.width(), buttonRect.y()));
    path.addLineTo(FloatPoint(buttonRect.x() + buttonRect.width() / 2.0, buttonRect.y() + buttonRect.height() / 2.0));
    path.closeSubpath();

    context->save();
    context->setStrokeStyle(SolidStroke);
    context->setStrokeThickness(lineWidth);
    context->setStrokeColor(Color::black, ColorSpaceDeviceRGB);
    context->setFillColor(Color::black, ColorSpaceDeviceRGB);
    context->setLineJoin(BevelJoin);
    context->beginPath();
    context->addPath(path);
    context->drawPath();
    context->restore();
}

static IntRect computeMenuListArrowButtonRect(const IntRect& rect)
{
    // FIXME: The menu list arrow button should have a minimum and maximum width (to ensure usability) or
    // scale with respect to the font size used in the menu list control or some combination of both.
    return IntRect(IntPoint(rect.right() - rect.height(), rect.y()), IntSize(rect.height(), rect.height()));
}

static void paintMenuListBackground(GraphicsContext* context, const Path& menuListPath, const Color& backgroundColor)
{
    ASSERT(context);
    context->save();
    context->setFillColor(backgroundColor, ColorSpaceDeviceRGB);
    context->beginPath();
    context->addPath(menuListPath);
    context->fillPath();
    context->restore();
}

bool RenderThemeBlackBerry::paintMenuList(RenderObject* object, const PaintInfo& info, const IntRect& rect)
{
    // Note, this method is not called if the menu list explicitly specifies either a border or background color.
    // Instead, RenderThemeBlackBerry::paintMenuListButton is called. Therefore, when this method is called, we don't
    // have to adjust rect with respect to the border dimensions.

    ASSERT(info.context);
    GraphicsContext* context = info.context;

    Path menuListRoundedRectangle;
    menuListRoundedRectangle.addRoundedRect(rect, FloatSize(largeRadius, largeRadius));

    // 1. Paint the background of the entire control.
    paintMenuListBackground(context, menuListRoundedRectangle, Color::white);

    // 2. Paint the background of the button and its arrow.
    IntRect arrowButtonRectangle = computeMenuListArrowButtonRect(rect);
    paintMenuListButtonGradientAndArrow(context, object, arrowButtonRectangle, menuListRoundedRectangle);

    // 4. Stroke an outline around the entire control.
    context->save();
    context->setStrokeStyle(SolidStroke);
    context->setStrokeThickness(lineWidth);
    if (!isEnabled(object))
        context->setStrokeColor(disabledOutline, ColorSpaceDeviceRGB);
    else if (isPressed(object))
        context->setStrokeGradient(createLinearGradient(depressedTopOutline, depressedBottomOutline, rect.topRight(), rect.bottomRight()));
    else if (isHovered(object))
        context->setStrokeGradient(createLinearGradient(hoverTopOutline, hoverBottomOutline, rect.topRight(), rect.bottomRight()));
    else
        context->setStrokeGradient(createLinearGradient(regularTopOutline, regularBottomOutline, rect.topRight(), rect.bottomRight()));

    context->beginPath();
    context->addPath(menuListRoundedRectangle);
    context->strokePath();
    context->restore();
    return false;
}

bool RenderThemeBlackBerry::paintMenuListButton(RenderObject* object, const PaintInfo& info, const IntRect& rect)
{
    // Note, this method is only called if the menu list explicitly specifies either a border or background color.
    // Otherwise, RenderThemeBlackBerry::paintMenuList is called. We need to fit the arrow button with the border box
    // of the menu-list so as to not occlude the custom border.

    // We compute menuListRoundedRectangle with respect to the dimensions of the entire menu-list control (i.e. rect) and
    // its border radius so that we clip the contour of the arrow button (when we paint it below) to match the contour of
    // the control.
    IntSize topLeftRadius;
    IntSize topRightRadius;
    IntSize bottomLeftRadius;
    IntSize bottomRightRadius;
    object->style()->getBorderRadiiForRect(rect, topLeftRadius, topRightRadius, bottomLeftRadius, bottomRightRadius);
    Path menuListRoundedRectangle;
    menuListRoundedRectangle.addRoundedRect(rect, topLeftRadius, topRightRadius, bottomLeftRadius, bottomRightRadius);

    // 1. Paint the background of the entire control.
    Color fillColor = object->style()->visitedDependentColor(CSSPropertyBackgroundColor);
    if (!fillColor.isValid())
        fillColor = Color::white;
    paintMenuListBackground(info.context, menuListRoundedRectangle, fillColor);

    // 2. Paint the background of the button and its arrow.
    IntRect bounds = IntRect(rect.x() + object->style()->borderLeftWidth(),
                         rect.y() + object->style()->borderTopWidth(),
                         rect.width() - object->style()->borderLeftWidth() - object->style()->borderRightWidth(),
                         rect.height() - object->style()->borderTopWidth() - object->style()->borderBottomWidth());

    IntRect arrowButtonRectangle = computeMenuListArrowButtonRect(bounds); // Fit the arrow button within the border box of the menu-list.
    paintMenuListButtonGradientAndArrow(info.context, object, arrowButtonRectangle, menuListRoundedRectangle);
    return false;
}

void RenderThemeBlackBerry::adjustSliderThumbSize(RenderObject* o) const
{
    ControlPart part = o->style()->appearance();
    if (part == MediaVolumeSliderThumbPart || part == SliderThumbHorizontalPart || part == SliderThumbVerticalPart) {
        o->style()->setWidth(Length(sliderThumbWidth, Fixed));
        o->style()->setHeight(Length(sliderThumbHeight, Fixed));
    } else if (part == MediaSliderThumbPart) {
        o->style()->setWidth(Length(mediaSliderThumbWidth, Fixed));
        o->style()->setHeight(Length(mediaSliderThumbHeight, Fixed));
    }
}

bool RenderThemeBlackBerry::paintSliderTrack(RenderObject* object, const PaintInfo& info, const IntRect& rect)
{
    const static int SliderTrackHeight = 5;
    IntRect rect2;
    if (object->style()->appearance() == SliderHorizontalPart) {
        rect2.setHeight(SliderTrackHeight);
        rect2.setWidth(rect.width());
        rect2.setX(rect.x());
        rect2.setY(rect.y() + (rect.height() - SliderTrackHeight) / 2);
    } else {
        rect2.setHeight(rect.height());
        rect2.setWidth(SliderTrackHeight);
        rect2.setX(rect.x() + (rect.width() - SliderTrackHeight) / 2);
        rect2.setY(rect.y());
    }
    return paintSliderTrackRect(object, info, rect2);
}

bool RenderThemeBlackBerry::paintSliderTrackRect(RenderObject* object, const PaintInfo& info, const IntRect& rect2) {
    return paintSliderTrackRect(object, info, rect2, rangeSliderRegularTopOutline, rangeSliderRegularBottomOutline,
                rangeSliderRegularTop, rangeSliderRegularBottom);
}

bool RenderThemeBlackBerry::paintSliderTrackRect(RenderObject* object, const PaintInfo& info, const IntRect& rect2,
        RGBA32 strokeColorStart, RGBA32 strokeColorEnd, RGBA32 fillColorStart, RGBA32 fillColorEnd)
{
    FloatSize smallCorner(smallRadius, smallRadius);

    info.context->save();
    info.context->setStrokeStyle(SolidStroke);
    info.context->setStrokeThickness(lineWidth);

    info.context->setStrokeGradient(createLinearGradient(strokeColorStart, strokeColorEnd, rect2.topRight(), rect2. bottomRight()));
    info.context->setFillGradient(createLinearGradient(fillColorStart, fillColorEnd, rect2.topRight(), rect2.bottomRight()));

    info.context->beginPath();
    Path path;
    path.addRoundedRect(rect2, smallCorner);
    info.context->addPath(path);
    info.context->drawPath();

    info.context->restore();
    return false;
}

bool RenderThemeBlackBerry::paintSliderThumb(RenderObject* object, const PaintInfo& info, const IntRect& rect)
{
    FloatSize largeCorner(largeRadius, largeRadius);

    info.context->save();
    info.context->setStrokeStyle(SolidStroke);
    info.context->setStrokeThickness(lineWidth);

    if (isPressed(object) || isHovered(object)) {
        info.context->setStrokeGradient(createLinearGradient(hoverTopOutline, hoverBottomOutline, rect.topRight(), rect. bottomRight()));
        info.context->setFillGradient(createLinearGradient(hoverTop, hoverBottom, rect.topRight(), rect.bottomRight()));
    } else {
        info.context->setStrokeGradient(createLinearGradient(regularTopOutline, regularBottomOutline, rect.topRight(), rect. bottomRight()));
        info.context->setFillGradient(createLinearGradient(regularTop, regularBottom, rect.topRight(), rect.bottomRight()));
    }

    info.context->beginPath();
    Path path;
    path.addRoundedRect(rect, largeCorner);
    info.context->addPath(path);
    info.context->drawPath();

    IntPoint startPoint(rect.x() + 4, rect.y() + 5);
    IntPoint endPoint(rect.x() + 4, rect.y() + 20);

    for (int i =0; i < 3; i++, startPoint.setX(startPoint.x() + 2), endPoint.setX(endPoint.x() + 2)) {
        if (isPressed(object) || isHovered(object))
            info.context->setStrokeColor(dragRollLight, ColorSpaceDeviceRGB);
        else
            info.context->setStrokeColor(dragRegularLight, ColorSpaceDeviceRGB);
        info.context->drawLine(startPoint, endPoint);

        startPoint.setX(startPoint.x() + 1);
        endPoint.setX(endPoint.x() + 1);
        if (isPressed(object) || isHovered(object))
            info.context->setStrokeColor(dragRollDark, ColorSpaceDeviceRGB);
        else
            info.context->setStrokeColor(dragRegularDark, ColorSpaceDeviceRGB);
        info.context->drawLine(startPoint, endPoint);
    }
    info.context->restore();
    return false;
}

static bool paintMediaButton(GraphicsContext* context, const IntRect& rect, Image* image)
{
    context->drawImage(image, ColorSpaceDeviceRGB, rect);
    return false;
}

bool RenderThemeBlackBerry::paintMediaPlayButton(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
#if ENABLE(VIDEO)
    HTMLMediaElement* mediaElement = toParentMediaElement(object);

    if (!mediaElement)
        return false;

    static Image* mediaPlay = Image::loadPlatformResource("play").releaseRef();
    static Image* mediaPause = Image::loadPlatformResource("pause").releaseRef();

    return paintMediaButton(paintInfo.context, rect, mediaElement->canPlay() ? mediaPlay : mediaPause);
#else
    return false;
#endif
}

bool RenderThemeBlackBerry::paintMediaMuteButton(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
#if ENABLE(VIDEO)
    HTMLMediaElement* mediaElement = toParentMediaElement(object);

    if (!mediaElement)
        return false;

    static Image* mediaMute = Image::loadPlatformResource("speaker").releaseRef();
    static Image* mediaUnmute = Image::loadPlatformResource("speaker_mute").releaseRef();

    return paintMediaButton(paintInfo.context, rect, mediaElement->muted() || mediaElement->volume() == 0.0 ? mediaUnmute : mediaMute);
#else
    return false;
#endif
}

bool RenderThemeBlackBerry::paintMediaFullscreenButton(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
#if ENABLE(VIDEO)
    HTMLMediaElement* mediaElement = toParentMediaElement(object);

    if (!mediaElement)
        return false;

    static Image* mediaFullscreen = Image::loadPlatformResource("fullscreen").releaseRef();

    return paintMediaButton(paintInfo.context, rect, mediaFullscreen);
#else
    return false;
#endif
}

bool RenderThemeBlackBerry::paintMediaSliderTrack(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
#if ENABLE(VIDEO)
    IntRect rect2(rect.x() + 3, rect.y() + 14, rect.width() - 6, 2);

    HTMLMediaElement* mediaElement = toParentMediaElement(object);

    if (!mediaElement)
        return false;

    float loaded = 0;
//  FIXME: replace loaded with commented out one when buffer bug is fixed (see comment in
//  MediaPlayerPrivateMMrenderer::percentLoaded).
//  loaded = mediaElement->percentLoaded();
    if (mediaElement->player())
        loaded = static_cast<MediaPlayerPrivate *>(mediaElement->player()->implementation())->percentLoaded();
    float position = mediaElement->duration() > 0 ? (mediaElement->currentTime() / mediaElement->duration()) : 0;

    int x = rect.x() + 3;
    int y = rect.y() + 14;
    int w = rect.width() - 6;
    int h = 2;

    int wPlayed = (w * position);
    int wLoaded = (w - mediaSliderThumbWidth) * loaded + mediaSliderThumbWidth;

    IntRect played(x, y, wPlayed, h);
    IntRect buffered(x, y, wLoaded, h);

    // paint main slider bar
    bool result = paintSliderTrackRect(object, paintInfo, rect2);

    if (loaded > 0 || position > 0) {
        // paint buffered bar
        paintSliderTrackRect(object, paintInfo, buffered, Color::darkGray, Color::darkGray, Color::darkGray, Color::darkGray);

        // paint played part of bar (left of slider thumb) using selection color
        paintSliderTrackRect(object, paintInfo, played, selection, selection, selection, selection);
    }
    return result;

#else
    return false;
#endif
}

bool RenderThemeBlackBerry::paintMediaSliderThumb(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
#if ENABLE(VIDEO)
    if (!object->parent())
        return false;

    RenderSlider *slider = toRenderSlider(object->parent());
    if (!slider)
        return false;

    paintInfo.context->save();
    paintInfo.context->beginPath();
    Path mediaThumbRoundedRectangle;
    mediaThumbRoundedRectangle.addRoundedRect(rect, FloatSize(mediaSliderThumbRadius, mediaSliderThumbRadius));
    paintInfo.context->addPath(mediaThumbRoundedRectangle);
    paintInfo.context->setStrokeStyle(SolidStroke);
    paintInfo.context->setStrokeThickness(0.5);
    paintInfo.context->setStrokeColor(Color::black, ColorSpaceDeviceRGB);

    if (isPressed(object) || isHovered(object) || slider->inDragMode()) {
        paintInfo.context->setFillGradient(createLinearGradient(selection, Color(selection).dark().rgb(),
                rect.topRight(), rect.bottomRight()));
    } else {
        paintInfo.context->setFillGradient(createLinearGradient(Color::white, Color(Color::white).dark().rgb(),
                rect.topRight(), rect.bottomRight()));
    }
    paintInfo.context->drawPath();
    paintInfo.context->restore();

    return true;
#else
    return false;
#endif
}

bool RenderThemeBlackBerry::paintMediaVolumeSliderTrack(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
#if ENABLE(VIDEO)
    float pad = rect.width() * 0.45;
    float x = rect.x() + pad;
    float y = rect.y() + pad;
    float width = rect.width() * 0.1;
    float height = rect.height() - (2.0 * pad);

    IntRect rect2(x, y, width, height);

    return paintSliderTrackRect(object, paintInfo, rect2);
#else
    return false;
#endif
}

bool RenderThemeBlackBerry::paintMediaVolumeSliderThumb(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
#if ENABLE(VIDEO)
    static Image* mediaVolumeThumb = Image::loadPlatformResource("volume_thumb").releaseRef();

    return paintMediaButton(paintInfo.context, rect, mediaVolumeThumb);
#else
    return false;
#endif
}

Color RenderThemeBlackBerry::platformFocusRingColor() const
{
    return focusRingPen;
}

Color RenderThemeBlackBerry::platformActiveSelectionBackgroundColor() const
{
    return Color(selection);
}

}
