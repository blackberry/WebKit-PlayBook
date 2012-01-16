/*
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 */

#ifndef RenderThemeBlackBerry_H
#define RenderThemeBlackBerry_H

#include "RenderTheme.h"
#include "BlackBerryPlatformInputEvents.h"

namespace WebCore {
    class Gradient;

    class RenderThemeBlackBerry : public RenderTheme {
    public:
        static PassRefPtr<RenderTheme> create();
        virtual ~RenderThemeBlackBerry();

        virtual String extraDefaultStyleSheet();

#if ENABLE(VIDEO)
        virtual String extraMediaControlsStyleSheet();
#endif
        virtual bool supportsHover(const RenderStyle*) const { return true; }

        virtual double caretBlinkInterval() const;

#if ENABLE(BLACKBERRY_CARET_APPEARANCE)
        virtual void paintCaret(GraphicsContext*, const IntRect& caretRect, const Element* rootEditableElement);
        virtual void repaintCaret(RenderView*, const IntRect& caretRect, CaretVisibility);
        virtual void paintCaretMarker(GraphicsContext*, const FloatRect& caretRect, const Font&);
        virtual void adjustTextColorForCaretMarker(Color&) const;
        void setCaretHighlightStyle(Olympia::Platform::CaretHighlightStyle caretHighlightStyle) { m_caretHighlightStyle = caretHighlightStyle; }
#endif

        virtual void systemFont(int cssValueId, FontDescription&) const;
        virtual bool paintCheckbox(RenderObject*, const PaintInfo&, const IntRect&);
        virtual void setCheckboxSize(RenderStyle*) const;
        virtual bool paintRadio(RenderObject*, const PaintInfo&, const IntRect&);
        virtual void setRadioSize(RenderStyle*) const;
        virtual bool paintButton(RenderObject*, const PaintInfo&, const IntRect&);
        void calculateButtonSize(RenderStyle*) const;
        virtual void adjustMenuListStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
        virtual bool paintMenuListButton(RenderObject*, const PaintInfo&, const IntRect&);
        virtual void adjustSliderThumbSize(RenderObject* o) const;
        virtual bool paintSliderTrack(RenderObject*, const PaintInfo&, const IntRect&);
        virtual bool paintSliderThumb(RenderObject*, const PaintInfo&, const IntRect&);

        virtual Color platformFocusRingColor() const;
        virtual bool supportsFocusRing(const RenderStyle* style) const { return style->hasAppearance(); }

        virtual void adjustButtonStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
        virtual void adjustTextFieldStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
        virtual bool paintTextField(RenderObject*, const PaintInfo&, const IntRect&);

        virtual void adjustTextAreaStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
        virtual bool paintTextArea(RenderObject*, const PaintInfo&, const IntRect&);

        virtual void adjustSearchFieldStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
        virtual void adjustSearchFieldCancelButtonStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
        virtual bool paintSearchField(RenderObject*, const PaintInfo&, const IntRect&);
        virtual bool paintSearchFieldCancelButton(RenderObject*, const PaintInfo&, const IntRect&);

        virtual void adjustMenuListButtonStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
        virtual void adjustCheckboxStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
        virtual void adjustRadioStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
        virtual bool paintMenuList(RenderObject*, const PaintInfo&, const IntRect&);

        virtual bool paintMediaFullscreenButton(RenderObject*, const PaintInfo&, const IntRect&);
        virtual bool paintMediaSliderTrack(RenderObject*, const PaintInfo&, const IntRect&);
        virtual bool paintMediaVolumeSliderTrack(RenderObject*, const PaintInfo&, const IntRect&);
        virtual bool paintMediaSliderThumb(RenderObject*, const PaintInfo&, const IntRect&);
        virtual bool paintMediaVolumeSliderThumb(RenderObject*, const PaintInfo&, const IntRect&);
        virtual bool paintMediaPlayButton(RenderObject*, const PaintInfo&, const IntRect&);
        virtual bool paintMediaMuteButton(RenderObject*, const PaintInfo&, const IntRect&);

        virtual Color platformActiveSelectionBackgroundColor() const;

    private:
        static const String& defaultGUIFont();

        // The default variable-width font size.  We use this as the default font
        // size for the "system font", and as a base size (which we then shrink) for
        // form control fonts.
        static float defaultFontSize;

        RenderThemeBlackBerry();
        void setButtonStyle(RenderStyle* style) const;

        void paintMenuListButtonGradientAndArrow(GraphicsContext*, RenderObject*, IntRect buttonRect, const Path& clipPath);
        bool paintTextFieldOrTextAreaOrSearchField(RenderObject*, const PaintInfo&, const IntRect&);
        bool paintSliderTrackRect(RenderObject*, const PaintInfo&, const IntRect&);
        bool paintSliderTrackRect(RenderObject* object, const PaintInfo& info, const IntRect& rect2, RGBA32 strokeColorStart,
                RGBA32 strokeColorEnd, RGBA32 fillColorStart, RGBA32 fillColorEnd);

#if ENABLE(BLACKBERRY_CARET_APPEARANCE)
        bool m_shouldRepaintVerticalCaret;
        Olympia::Platform::CaretHighlightStyle m_caretHighlightStyle;
        IntRect m_oldAbsoluteCaretTextBoundingBox; // Used to determine whether we may have already added/painted the caret marker.
#endif
    };

}
#endif // RenderThemeBlackBerry_H
