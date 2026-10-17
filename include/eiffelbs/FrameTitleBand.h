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
// The label is drawn UPPERCASE, bold and tracked apart at ebs::kBandTextAlpha (50 %): a frame name
// is a caption, not an instrument. Tracked capitals at small size stay readable where tight text
// would blur into a single grey stroke, and 50 % keeps the caption under the knobs' attention.
// The caption is also drawn WITHOUT accents: French typography sets all-caps words without accent
// marks, so "Réverbération" reads REVERBERATION on the spine (see ebs::bandCaps). Casing a Chinese
// or Japanese title changes nothing and ideographs have no marks to drop, so the same rule serves
// every locale; upper casing is also what the width constant was measured against (uppercase glyphs
// are the widest that ever stand in this column). Tracking only ADDS length along the spine, never
// thickness across it, so the fixed band width is cleared by the same measurement at every size.
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

    /** The width a title band asks for - fixed across locales by design.
        Measured, not guessed: uppercase bold glyphs at the largest size the band can pick
        (13 pt) are 13 px thick, so the spine is that thickness plus two pixels clear at each
        edge = 17 px, and nothing more. A long translated name shrinks its glyphs (and therefore
        its thickness), it never asks for a wider column. */
    inline constexpr int kBandWidth = 17;

    /* Glyph padding kept clear at both ends of the spine: the fit check measures against
       the band height minus this much space top and bottom, never the bare height. */
    inline constexpr float kBandPad = 3.0f;

    /** Text strength on the spine. A frame name is read, not looked at: 50 % alpha keeps it
        legible against the strip tint without pulling the eye off the knobs. */
    inline constexpr float kBandTextAlpha = 0.50f;

    /** Extra space put BETWEEN the letters of the caption, in pixels along the spine. Tracked
        capitals read as a caption at small size where tight text blurs into one stroke; the value
        is applied after every glyph except the last, so measured length = glyph widths + spacing * (n-1).
        Tracking stretches the name ALONG the spine only: thickness across the spine is untouched,
        which is what keeps kBandWidth a constant measured from glyphs. */
    inline constexpr float kBandLetterSpacing = 1.2f;

    /** The caption the spine draws from a title given in its own locale's casing: UPPERCASE, with the
        accents dropped. French typography sets all-caps words WITHOUT accent marks - "Reverb" is written
        REVERBERATION in an all-caps heading - and a spine read from the corner of an eye follows that
        convention; accented capitals are heavier glyphs that blur at 9 pt. Only single-codepoint Latin
        diacritics map to their base letter, so CJK, Cyrillic, Greek and ligatures (OE, AE) pass through
        untouched: no locale loses a letter it needs to be readable. */
    inline juce::String bandCaps (const juce::String& text)
    {
        const auto upper = text.toUpperCase();
        juce::String out;

        // A kept letter is appended as the UTF-8 bytes of its own codepoint, written out by hand. This
        // avoids every JUCE single-character String constructor (they are the source of the mojibake this
        // helper was first built to fix: one overload turned a codepoint into a run of digit glyphs).
        auto appendCode = [&out] (int c)
        {
            char buf[4] = { 0, 0, 0, 0 };

            if (c < 0x80)
                buf[0] = static_cast<char> (c);
            else if (c < 0x800)
            {
                buf[0] = static_cast<char> (0xC0 | (c >> 6));
                buf[1] = static_cast<char> (0x80 | (c & 0x3F));
            }
            else
            {
                buf[0] = static_cast<char> (0xE0 | (c >> 12));
                buf[1] = static_cast<char> (0x80 | ((c >> 6) & 0x3F));
                buf[2] = static_cast<char> (0x80 | (c & 0x3F));
            }

            out << juce::String (juce::CharPointer_UTF8 (buf));
        };

        // Walk the UPPERCASED string with its own UTF-8 pointer: each step is one whole codepoint,
        // so a two-byte accent never gets split into two half-letters. Indexing by position instead
        // (substring on byte offsets) produced exactly that - mojibake glyphs in the drawn caption.
        for (auto ptr = upper.getCharPointer(); ! ptr.isEmpty(); ++ptr)
        {
            const int code = static_cast<int> (*ptr);

            switch (code)
            {
                // JUCE toUpperCase() only casing-changes ASCII letters, so an accented letter comes out of it
                // UNCHANGED. French typography wants REVERBERATION from "Réverbération": the map covers BOTH cases.
                case 0x00C0: case 0x00C1: case 0x00C2: case 0x00C3: case 0x00C4: case 0x00C5:
                case 0x00E0: case 0x00E1: case 0x00E2: case 0x00E3: case 0x00E4: case 0x00E5: out << "A"; break;
                case 0x00C8: case 0x00C9: case 0x00CA: case 0x00CB:
                case 0x00E8: case 0x00E9: case 0x00EA: case 0x00EB: out << "E"; break;
                case 0x00CC: case 0x00CD: case 0x00CE: case 0x00CF:
                case 0x00EC: case 0x00ED: case 0x00EE: case 0x00EF: out << "I"; break;
                case 0x00D2: case 0x00D3: case 0x00D4: case 0x00D5: case 0x00D6:
                case 0x00F2: case 0x00F3: case 0x00F4: case 0x00F5: case 0x00F6: out << "O"; break;
                case 0x00D9: case 0x00DA: case 0x00DB: case 0x00DC:
                case 0x00F9: case 0x00FA: case 0x00FB: case 0x00FC: out << "U"; break;
                case 0x00C7: case 0x00E7: out << "C"; break;
                case 0x00D1: case 0x00F1: out << "N"; break;
                case 0x1E9E: case 0x00DF: out << "SS"; break;   // capital sharp s, and the lower-case one JUCE leaves as it is
                default: appendCode (code); break;   // everything else, kept as written
            }
        }

        return out;
    }

    /** Measured length a title takes ALONG the band at one glyph size, tracking included (in pixels).
        This is the number the fit check compares against the spine height, and exactly what gets drawn. */
    inline int bandTextLength (const juce::String& text, float fontSize)
    {
        const auto font = createFont (fontSize, true);
        float total = 0.0f;

        for (int i = 0; i < text.length(); ++i)
        {
            total += font.getStringWidth (text.substring (i, i + 1));   // the advance JUCE gives this glyph

            if (i < text.length() - 1)
                total += kBandLetterSpacing;   // between letters only, never after the last one
        }

        return juce::roundToInt (total);
    }

    /** Measured thickness a title needs ACROSS the band at one glyph size (in pixels): the
        bounding box of the drawn line, uppercase, bold. This is what a spine width has to clear. */
    inline int bandGlyphThickness (const juce::String& text, float fontSize)
    {
        const auto font = createFont (fontSize, true);
        const auto caps = bandCaps (text);   // the caption as it will be drawn, accents dropped

        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText (font, caps, 0.0f, 0.0f);
        return juce::roundToInt (glyphs.getBoundingBox (0, glyphs.getNumGlyphs(), false).getHeight());
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

            // The name is set in its own locale's casing, but a spine is read at a glance from the corner of
            // an eye: draw it UPPERCASE, and without accents. Upper casing a CJK string is a no-op and dropping
            // Latin marks follows the all-caps typographic convention (bandCaps), so this is safe for every
            // locale the host ships - and the fit check below measures exactly the text that gets drawn here.
            const auto caps = bandCaps (titleText);

            // Shrink the glyphs from 13pt until the name fits the band height. A locale
            // that runs long gets smaller text, never a wider column - and smaller glyphs are
            // thinner across the spine, so the fixed width stays cleared at every size.
            float size = kBandFontSize;
            int length = bandTextLength (caps, size);

            while (length > juce::roundToInt (h - 2.0f * kBandPad) && size > kBandMinFontSize)
            {
                size -= 1.0f;
                length = bandTextLength (caps, size);
            }

            const auto font = createFont (size, true);
            juce::GlyphArrangement glyphs;

            // The caption is TRACKED: every glyph is placed at its own advance plus the extra space between
            // letters. JUCE's addLineOfText applies only the font's kerning, with no gap, so tracking is added
            // by hand here - and bandTextLength (the fit check above) sums the same advances + gaps, which is
            // why the size chosen in that loop fits exactly what this draws. bb then measures the real extent.
            float cursor = 0.0f;

            for (int i = 0; i < caps.length(); ++i)
            {
                const auto ch = caps.substring (i, i + 1);
                glyphs.addLineOfText (font, ch, cursor, 0.0f);   // one glyph per call, baseline at y = 0

                if (i < caps.length() - 1)
                    cursor += font.getStringWidth (ch) + kBandLetterSpacing;   // gap AFTER each interior glyph
            }

            const auto bb = glyphs.getBoundingBox (0, glyphs.getNumGlyphs(), false);   // measured, not estimated

            // Rotation by -90 carries the baseline point (t, 0) to device Y = -t: the name runs upward, so a
            // spine on the LEFT edge reads bottom-to-top with its glyphs facing left, like a book spine. The
            // glyph box is bb.getRight() - bb.getX() long along its baseline; after rotation that length
            // stands vertically and ty centres it. Across the band the box spans tx + bb.getY() to
            // tx + bb.getBottom(), and the matching midline centres it in the spine width.
            const auto ty = h * 0.5f + (bb.getX() + bb.getRight()) * 0.5f;
            const auto tx = w * 0.5f - (bb.getY() + bb.getBottom()) * 0.5f;

            // 50 % alpha, tracked capitals: a name is read, not looked at. At full strength the spine competes
            // with the knobs, and tight small text blurs; tracking + half strength keeps it legible and quiet.
            g.setColour (isEnabled() ? ebs::text().withMultipliedAlpha (kBandTextAlpha)
                                     : ebs::textDim());
            glyphs.draw (g, juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi)
                                   .translated (tx, ty));
        }

        void resized() override {}   // the band reads its bounds at paint time, nothing to cache

    private:
        juce::String titleText;

        JUCE_LEAK_DETECTOR (FrameTitleBand)
    };
}
