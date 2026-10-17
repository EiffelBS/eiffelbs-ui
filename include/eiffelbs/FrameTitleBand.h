// FrameTitleBand.h
// eiffelbs-ui - a left vertical title spine for a frame (EiffelBS design system).
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// A frame row is measured by WIDTH: every block gets the width its content asks
// for, so the only room a host has to give is HEIGHT - which is shared by the whole
// row. This component spends that height: it names a frame along its left edge with
// the text rotated bottom-to-top, clipped to the frame's own rounded path so its
// corners hug the curve of the parent instead of squaring them off.
//
// It takes a fixed WIDTH (ebs::kBandWidth) for every locale, because a longer
// translated name is paid for by shrinking the glyph size down to kBandMinFontSize,
// not by widening the band: a French "Detection de tonalite" and an English "Key &
// Scale Detection" cost the same column of pixels, which keeps a host's frame-width
// model locale-independent.
//
// It is a sibling of ebs::DisclosureChevron in shape (a strip inside a frame) and it
// shares ebs::frameCornerRadius with ebs::drawGroupFrame, so band corners, frame
// corners and group corners cannot drift apart.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "eiffelbs/Theme.h"
#include "eiffelbs/Fonts.h"
#include "eiffelbs/LookAndFeel.h"

namespace ebs
{
    /** Glyph size the band starts at, and the smallest it may shrink to. */
    inline constexpr float kBandFontSize = 13.0f;
    inline constexpr float kBandMinFontSize = 9.0f;

    /** The width a title band asks for - fixed across locales by design. */
    inline constexpr int kBandWidth = 26;

    /* Glyph padding kept clear at both ends of the spine: the fit check measures against
       the band height minus this much space top and bottom, never the bare height. */
    inline constexpr float kBandPad = 3.0f;

    /** Measured length of a title along the band, at one glyph size (in pixels). */
    inline int bandTextLength (const juce::String& text, float fontSize)
    {
        return juce::roundToInt (juce::GlyphArrangement::getStringWidth (createFont (fontSize, true), text));
    }

    /** A left vertical title spine for a frame.
        Place it at x = 0 of the parent frame, width ebs::FrameTitleBand::bandWidth(),
        and the FULL height of the row: the band is as tall as its frame, so the name
        reads upward along the whole left edge and gets exactly the room the host has.

        Call setTitle with the translated name; the paint picks a size between 13pt and
        9pt that fits, and clips to the parent's rounded corners. A band that says
        nothing paints only its tint - it is allowed to be silent in a locale where the
        frame label does not exist. */
    class FrameTitleBand : public juce::Component,
                           public juce::SettableTooltipClient
    {
    public:
        FrameTitleBand()
        {
            setInterceptsMouseClicks (false, false);   // a name is read, not clicked
            setWantsKeyboardFocus (false);
        }

        /** The width this band wants at its parent's left edge. */
        static constexpr int bandWidth() { return kBandWidth; }

        void setTitle (const juce::String& newTitle)
        {
            if (titleText != newTitle)
            {
                titleText = newTitle;
                repaint();
            }
        }

        const juce::String& title() const noexcept { return titleText; }

        void paint (juce::Graphics& g) override
        {
            auto strip = getLocalBounds();

            // The spine bolts to the left edge of a frame whose corners are rounded by the shared radius.
            // Fill row by row, tested against that same path: the strip takes exactly the room the curve
            // leaves at its corner, and never squares the corner off.
            const auto* parent = getParentComponent();
            juce::Path framePath;

            if (parent != nullptr)
                framePath.addRoundedRectangle (-float (strip.getX()), -float (strip.getY()),
                                               float (parent->getWidth()), float (parent->getHeight()),
                                               ebs::frameCornerRadius);

            g.setColour (ebs::accent().withAlpha (0.10f));

            if (parent == nullptr)
                g.fillRect (strip);
            else
                for (int row = 0; row < strip.getHeight(); ++row)
                {
                    int x = 0;
                    while (x < strip.getWidth()
                           && ! framePath.contains (float (strip.getX() + x) + 0.5f,
                                                   float (strip.getY() + row) + 0.5f))
                        ++x;

                    if (x < strip.getWidth())
                        g.fillRect (strip.getX() + x, strip.getY() + row, strip.getWidth() - x, 1);
                }

            // The inner edge of the spine is the frame's own trim: a solid pixel column, so its colour
            // reaches the test without antialiasing blending it into the strip tint.
            g.setColour (ebs::panelBorder());
            g.fillRect (strip.getRight() - 1, strip.getY(), 1, strip.getHeight());

            if (titleText.isEmpty())
                return;

            const auto w = float (strip.getWidth());
            const auto h = float (strip.getHeight());

            // Shrink the glyphs from 13pt until the name fits the band height. A locale
            // that runs long gets smaller text, never a wider column.
            float size = kBandFontSize;
            int length = bandTextLength (titleText, size);

            while (length > juce::roundToInt (h - 2.0f * kBandPad) && size > kBandMinFontSize)
            {
                size -= 1.0f;
                length = bandTextLength (titleText, size);
            }

            const auto font = createFont (size, true);
            juce::GlyphArrangement glyphs;
            glyphs.addLineOfText (font, titleText, 0.0f, 0.0f);

            const auto bb = glyphs.getBoundingBox (0, glyphs.getNumGlyphs(), false);   // measured, not estimated

            // Rotation by -90 carries the baseline point (t, 0) to device Y = -t: the name runs upward, so a
            // spine on the LEFT edge reads bottom-to-top with its glyphs facing left, like a book spine. The
            // glyph box is bb.getRight() - bb.getX() long along its baseline; after rotation that length
            // stands vertically and ty centres it. Across the band the box spans tx + bb.getY() to
            // tx + bb.getBottom(), and the matching midline centres it in the 26 px strip.
            const auto ty = h * 0.5f + (bb.getX() + bb.getRight()) * 0.5f;
            const auto tx = w * 0.5f - (bb.getY() + bb.getBottom()) * 0.5f;

            g.setColour (isEnabled() ? ebs::text() : ebs::textDim());
            glyphs.draw (g, juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi)
                                   .translated (tx, ty));
        }

        void resized() override {}   // the band reads its bounds at paint time, nothing to cache

    private:
        juce::String titleText;

        JUCE_LEAK_DETECTOR (FrameTitleBand)
    };
}
