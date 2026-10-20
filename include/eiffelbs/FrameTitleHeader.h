// FrameTitleHeader.h
// eiffelbs-ui - a TOP title band for a frame: the same caption voice as the left
// spine, laid along the top edge instead of up it.
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// WHY A SECOND BAND AND NOT A ROTATED ONE
//
// The left spine reads a frame name bottom-to-top. That is right for a frame that is
// TALL and narrow: the name spends the height the frame already has, and its width is
// fixed by a measured constant so every locale pays the same column of pixels. A
// DIALOG is the opposite shape - wide, short, one per window - and a spine there eats
// width from the content and puts the name where nobody looks for a window title. The
// top band keeps the typography identical (UPPERCASE, bold, tracked apart at half
// strength, accents dropped) and spends WIDTH instead of height, which is the room a
// wide frame has to give. It is additive: nothing in FrameTitleBand changed, and a
// host that already names its frames on the left keeps doing exactly that.
//
// THE HEIGHT IS FIXED BY MEASUREMENT, NOT PER TITLE
//
// The spine fixes its WIDTH because tracking stretches only along the spine. Here the
// band's thickness is what must stay fixed across locales: glyphs shrink when a long
// translated name does not fit the width, and smaller glyphs are THINNER, so the same
// measured height clears every caption. The number comes from the glyph box of capitals
// at the largest size the band may pick (13pt), plus clear space above and below - the
// same measurement logic that produced ebs::kBandWidth.
//
// It shares ebs::frameCornerRadius with ebs::drawGroupFrame, fills row-column by
// row-column against its parent's rounded path so its corners hug the curve, and puts
// a solid hairline on its OWN bottom edge: that line is where the band stops being the
// frame name and the content begins.
//
// WHAT A BAND NAMES, AND WHAT IT MUST NOT NAME
//
// A band names ONE FRAME. In a window that has its own title bar, the bar already says
// the window's name; laying a strip under it to say the same thing spends pixels on a
// sentence the reader finished reading one line above - and every group inside the
// window is left unnamed, which is what a band exists to fix. Put bands on the frames,
// where the reader has to tell four sections apart. A top level with NO title bar (a
// fold, an embedded panel) has nothing else to name it, so there the band is its name.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "eiffelbs/Theme.h"
#include "eiffelbs/Fonts.h"
#include "eiffelbs/LookAndFeel.h"
#include "eiffelbs/FrameTitleBand.h"   // bandCaps, tracking, size ladder, measurements

namespace ebs
{
    /** Clear space kept above and below the caption inside the band. Measured against
        this, never against the bare height, exactly like kBandPad on the spine. */
    inline constexpr float kBandHeaderPad = 4.0f;

    /** Capitals used to MEASURE the fixed band height: letters whose glyph box is as
        tall as any caption gets, so the number is a floor for every locale's name, not
        an estimate for one title. */
    inline constexpr const char* kBandHeaderSample = "ABCDEFGHKMNPRSTUVWXYZ";

    /** The height a top title band asks for - fixed by measurement across locales.
        A long translated name shrinks its glyphs and therefore their thickness, so the
        same number clears every caption; it never asks for a taller strip. */
    inline int bandHeaderHeight()
    {
        static const int h = juce::roundToInt (bandGlyphThickness (juce::String (kBandHeaderSample),
                                                                  kBandFontSize))
                           + 2 * juce::roundToInt (kBandHeaderPad);
        return h;
    }

    /** A top title band for a frame: the name laid horizontally along its top edge.
        Place it at y = 0 of the parent frame, full width, height ebs::bandHeaderHeight();
        call setTitle with the translated name and the paint picks a size between 13pt
        and 9pt that fits the width, clipping to the parent's rounded corners. A band
        with an empty title paints only its tint - it is allowed to be silent in a locale
        where the frame has no name, exactly like the spine. */
    class FrameTitleHeader : public juce::Component,
                             public juce::SettableTooltipClient
    {
    public:
        FrameTitleHeader()
        {
            setInterceptsMouseClicks (false, false);   // a name is read, not clicked
            setWantsKeyboardFocus (false);
        }

        /** The height this band wants at its parent's top edge: the measured number,
         *  never a constant picked by eye (see ebs::bandHeaderHeight above). */
        static int bandHeight() { return ebs::bandHeaderHeight(); }

        void setTitle (const juce::String& newTitle)
        {
            if (titleText != newTitle)
            {
                titleText = newTitle;
                repaint();
            }
        }

        const juce::String& title() const noexcept { return titleText; }

        /** The caption's strength, in alpha. A window's own name is the first thing read,
            so a host may strengthen it above the spine's half strength; 1.0 keeps the
            text at full palette strength and 0 dims it to nothing. */
        void setCaptionStrength (float alpha)
        {
            const auto a = juce::jlimit (0.0f, 1.0f, alpha);

            if (strength != a)
            {
                strength = a;
                repaint();
            }
        }

        float captionStrength() const noexcept { return strength; }

        void paint (juce::Graphics& g) override
        {
            auto strip = getLocalBounds();

            // The band bolts to the top edge of a frame rounded by the shared radius. Fill
            // column by column, tested against that same path: the strip takes exactly the
            // room the curve leaves at its corners and never squares them off - the spine's
            // row-by-row rule with the axes swapped.
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
                for (int col = 0; col < strip.getWidth(); ++col)
                {
                    int y = 0;
                    while (y < strip.getHeight()
                           && ! framePath.contains (float (strip.getX() + col) + 0.5f,
                                                    float (strip.getY() + y) + 0.5f))
                        ++y;

                    if (y < strip.getHeight())
                        g.fillRect (strip.getX() + col, strip.getY() + y, 1, strip.getHeight() - y);
                }

            // The band's own bottom edge is the frame's trim: a solid pixel row, so the line
            // that separates the name from the content reaches the test without antialiasing
            // blending it into the tint.
            g.setColour (ebs::panelBorder());
            g.fillRect (strip.getX(), strip.getBottom() - 1, strip.getWidth(), 1);

            if (titleText.isEmpty())
                return;

            const auto w = float (strip.getWidth());
            const auto h = float (strip.getHeight());

            // Same caption as the spine: UPPERCASE, accents dropped (bandCaps). Upper casing a CJK
            // string is a no-op and dropping Latin marks follows all-caps typography, so every locale
            // the host ships keeps its letters.
            const auto caps = bandCaps (titleText);

            // Shrink from 13pt until the name fits the WIDTH. A long name gets smaller text, never a
            // taller strip - and smaller glyphs are thinner, which is what keeps the fixed height
            // cleared at every size (see kBandHeaderPad).
            float size = kBandFontSize;
            int length = bandTextLength (caps, size);

            while (length > juce::roundToInt (w - 2.0f * kBandHeaderPad) && size > kBandMinFontSize)
            {
                size -= 1.0f;
                length = bandTextLength (caps, size);
            }

            const auto font = createFont (size, true);
            juce::GlyphArrangement glyphs;

            // TRACKED capitals, one glyph per call: JUCE's addLineOfText applies only kerning, so the
            // gap between letters is added by hand - and bandTextLength (the fit check above) sums the
            // same advances + gaps, which is why the size picked in that loop fits exactly what draws.
            float cursor = 0.0f;

            for (int i = 0; i < caps.length(); ++i)
            {
                const auto ch = caps.substring (i, i + 1);
                glyphs.addLineOfText (font, ch, cursor, 0.0f);   // baseline at y = 0

                if (i < caps.length() - 1)
                    cursor += font.getStringWidth (ch) + kBandLetterSpacing;
            }

            const auto bb = glyphs.getBoundingBox (0, glyphs.getNumGlyphs(), false);   // measured, not estimated

            // Centred in the strip: the glyph box is bb.getRight() - bb.getX() long, so tx puts its
            // middle on the strip's centre line, and ty centres it between the pads. No rotation: the
            // name runs left-to-right, the way a window title is read.
            const auto tx = (w - (bb.getRight() - bb.getX())) * 0.5f - bb.getX();
            const auto ty = h * 0.5f - (bb.getY() + bb.getBottom()) * 0.5f;

            g.setColour (isEnabled() ? ebs::text().withMultipliedAlpha (strength)
                                     : ebs::textDim());
            glyphs.draw (g, juce::AffineTransform::translation (tx, ty));
        }

        void resized() override {}   // the band reads its bounds at paint time, nothing to cache

    private:
        juce::String titleText;
        float strength = kBandTextAlpha;   // the spine's half strength is the default voice

        JUCE_LEAK_DETECTOR (FrameTitleHeader)
    };
} // namespace ebs
