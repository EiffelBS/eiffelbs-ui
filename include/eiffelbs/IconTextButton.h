// IconTextButton.h
// eiffelbs-ui - shared icon + text label button (EiffelBS design system).
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// An IconButton that also carries a TEXT label. IconButton draws a glyph alone,
// and juce::DrawableButton centres its label and its image on top of each other,
// so neither produces "icon at the left, text glued to its right" without an
// override. That layout is what an action needs: a row of icon-only buttons is
// a guess, and one labelled button among guesses is the one a reader looks for.
//
// The glyph is drawn from the SAME path builder IconButton exposes, so a shape
// looks pixel-identical at the same size whichever of the two buttons renders
// it. Everything else - the colours, the hover, the framed chrome - follows the
// shared colour language: bgDark fill, panelBorder outline, accent on hover,
// one 5 px corner radius.
//
// A candidate for IconButton itself once a second project needs it; kept
// separate so IconButton's own surface (which several panels measure by shape)
// does not move under them.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

#include "eiffelbs/Theme.h"
#include "eiffelbs/LookAndFeel.h"
#include "eiffelbs/IconButton.h"
#include "eiffelbs/PillPainter.h"

namespace ebs
{

    class IconTextButton : public juce::Button
    {
    public:
        /** `shape` is any IconButton::Shape: the glyph is built by the SAME
         *  path builder, so a shape means exactly what it means there.
         *  `text` is drawn to the RIGHT of the glyph, on one line. */
        /** `text` defaults to empty: a kebab-only mini-options button carries
         *  no label and asking for one in braces would read as a typo. */
        IconTextButton (IconButton::Shape shape,
                        const juce::String& text = juce::String())
            : juce::Button (text), shape (shape)
        {
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }

        /** Glyph side in pixels (the same 10 px IconButton uses), and the
         *  gap between the glyph and the label. */
        void setIconSide (float px)
        {
            if (juce::exactlyEqual (iconSide, px))
                return;
            iconSide = px;
            repaint();
        }

        /** FRAMED mode: draws a real button chrome around the icon+text - rounded
         *  bgDark fill + panelBorder outline, brighter on hover (accent 40%
         *  outline), darker when down, pointing-hand cursor. For standalone
         *  mini action buttons. */
        void setFramed (bool shouldBeFramed)
        {
            if (framed == shouldBeFramed)
                return;
            framed = shouldBeFramed;
            if (framed)
                setMouseCursor (juce::MouseCursor::PointingHandCursor);
            repaint();
        }

        /** The label's font. Defaults to fontHint(), the size the button was
         *  designed at; a denser host (the Settings window) passes its own.
         *  widthForLabel() measures from the SAME font, so a layout that
         *  sizes the button to its label cannot disagree with the paint. */
        void setFont (const juce::Font& f)
        {
            font = f;
            repaint();
        }

        /** Test seam: paint the SAME path with an explicit hover state. */
        void paintForTest (juce::Graphics& g, bool over, bool down)
        {
            paintButton (g, over, down);
        }

        void paintButton (juce::Graphics& g, bool over, bool down) override
        {
            // ONE pill language for the whole app (PillPainter.h): uniform
            // chrome at rest, an unmistakable hover, and a destructive glyph
            // turning red under the pointer. The author asked for every icon
            // button of Settings to read like the Models row pills.
            paintPillButton (g, getLocalBounds(), shape, getButtonText(), font,
                             iconSide, PillTone::neutral, over, down,
                             isEnabled());
        }

        /** The width the button needs for its own label at the current font.
         *  A layout that sizes this button to a guess either cuts the text
         *  mid-word or leaves a dead band the pointer crosses before the click
         *  counts, so the width is measured, never assumed. */
        int widthForLabel() const
        {
            const int labelW = (int) std::ceil (
                font.getStringWidth (getButtonText()));
            return 7 + (int) (iconSide * 1.30f) + gap_ + labelW + 7;
        }

    private:
        IconButton::Shape shape;
        float iconSide = 10.0f;
        int   gap_     = 4;
        bool  framed   = false;
        juce::Font font = fontHint();

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IconTextButton)
    };

} // namespace ebs
