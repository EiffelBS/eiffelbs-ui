// WrappedText.h
// eiffelbs-ui - a text block that word-wraps inside the width it is given, and says
// how tall it needs before it is placed.
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// WHY NOT A juce::Label
//
// A Label has NO word wrap in this JUCE build: `setMultiLine` and `setWordWrapText` do not exist, so a host
// that wants paragraphs is left with two bad options - break lines by hand (a caption written for one window
// width stops fitting the moment the window shrinks) or let the text run past the edge and be clipped in
// silence. Both are lies about what the reader sees. This component wraps words for real, on the same
// mechanism a chat bubble uses (`juce::AttributedString` + `juce::TextLayout`), so lines come from font
// metrics rather than counted characters, and accents, CJK and long translated sentences behave.
//
// MEASURED BEFORE PLACED
//
// The height depends on the width, exactly like ChatBubble's `requiredHeightForWidth()`: a host measures, sets
// bounds, and nothing is clipped. A report block inside a viewport does the same - it grows with its own text,
// so "the last sentence was cut off" cannot happen in silence.
//
// TRUNCATION IS LOUD
//
// `setMaxLines(n)` keeps n lines and puts an ellipsis on the last one: a caption kept short for the layout says
// it is short. With 0 (the default) nothing is truncated, because text that vanishes with no trace is a bug
// report waiting to be written. The prefix is built by measuring word widths against the width, so the lines
// the host sees are exactly the lines this component measured - the ellipsis never pushes a line past the edge.
//
// WHAT THE LAYOUT DOES NOT OFFER IN THIS JUCE BUILD
//
// `TextLayout` has ONE draw signature (`draw (g, area)`) and takes vertical position from the paragraph
// justification set on its AttributedString; `AttributedString` has no line-count or ellipsis API. So
// truncation is measured here by hand, with the same font metrics the layout uses.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "eiffelbs/Theme.h"
#include "eiffelbs/Fonts.h"

namespace ebs
{
    /** Extra space between wrapped lines, in pixels. Zero makes a multi-line caption read as one dense block;
        this is the gap that keeps a caption's lines apart without turning it into a list. */
    inline constexpr float kTextLeading = 2.0f;

    /** Word-wrapped text inside the width given to it, measured before placement. A display: no clicks,
        keyboard focus, or editing; it carries its tooltip through `SettableTooltipClient` like every other
        EiffelBS caption. */
    class WrappedText : public juce::Component,
                        public juce::SettableTooltipClient
    {
    public:
        WrappedText()
        {
            setInterceptsMouseClicks (false, false);   // read, not clicked
            setWantsKeyboardFocus (false);
        }

        void setText (const juce::String& newText)
        {
            if (text != newText)
            {
                text = newText;
                layoutWidth = -1;   // the lines this width produced are dead
                repaint();
            }
        }

        const juce::String& getText() const noexcept { return text; }

        void setFont (const juce::Font& newFont)
        {
            if (font != newFont)
            {
                font = newFont;
                layoutWidth = -1;
                repaint();
            }
        }

        const juce::Font& getFont() const noexcept { return font; }

        /** The fill. Meaning stays in the colour, as everywhere else in the library: a warning keeps its own
            accent here, and `setTextColour` is how the host says so. */
        void setTextColour (juce::Colour c)
        {
            if (colour != c)
            {
                colour = c;
                repaint();
            }
        }

        /** 0 (the default) shows every line. Any other value keeps that many lines and marks the cut with an
            ellipsis, so a shortened caption is visibly short. */
        void setMaxLines (int n)
        {
            const int wanted = juce::jmax (0, n);

            if (maxLines != wanted)
            {
                maxLines = wanted;
                layoutWidth = -1;
                repaint();
            }
        }

        /** Alignment of the wrapped lines inside the block: the horizontal part is each line's justification,
            and the top/bottom flags position the whole block inside its area (the two are read by the layout,
            not passed to draw). */
        void setTextAlignment (juce::Justification a)
        {
            if (alignment != a)
            {
                alignment = a;
                repaint();
            }
        }

        /** Lines that fit the given width, counted - exactly the lines that will paint. Empty text counts 0. */
        int linesForWidth (int w) const          { return buildLayout (w).getNumLines(); }

        /** The height this text needs at the given width: measure, then set bounds. A truncated block reports
            the height of its kept lines, not the height of all its text. */
        int requiredHeightForWidth (int w) const
        {
            return juce::roundToInt (buildLayout (w).getHeight());
        }

        /** The words that will be visible at width `w`: every line this component measured inside that width, with
            the truncation mark where a caption is cut. A host that builds its own tooltip for a wrapped caption
            shows what the block can actually say, and nothing else is invented. */
        juce::String textForWidth (int w) const
        {
            return visibleText (w);
        }

        void resized() override
        {
            buildLayout (getWidth());   // the lines painted at this width, cached for paint()
        }

        void paint (juce::Graphics& g) override
        {
            if (text.isEmpty())
                return;

            buildLayout (getWidth()).draw (g, getLocalBounds().toFloat());
        }

    private:
        juce::String text;
        juce::Font font = ebs::fontBody();
        juce::Colour colour = ebs::text();
        int maxLines = 0;
        juce::Justification alignment = juce::Justification::topLeft;

        mutable int   layoutWidth = -1;
        mutable juce::int64 layoutColourKey = -1;
        mutable int   layoutMaxLines = -1;
        mutable juce::String layoutText, layoutFontName;
        mutable bool  layoutEnabled = false;
        mutable juce::TextLayout layout;

        /** The text the layout is built from: `text` when nothing is truncated, and at most `maxLines` measured
            lines ending in an ellipsis when it is. Words are accumulated against the width with this font's own
            widths, so every kept line fits before the layout sees it; an explicit `\n` is a hard break (a report
            joins its entries that way), and runs of spaces collapse to one - a wrapped caption never keeps a gap
            it cannot measure. */
        juce::String visibleText (int w) const
        {
            if (maxLines <= 0 || text.isEmpty())
                return text;

            const auto ellipsis = juce::String::charToString (0x2026);   // one glyph, U+2026
            const double spaceW = font.getStringWidth (" ");
            juce::String out;
            int lines = 0;

            for (const auto& forced : juce::StringArray::fromTokens (text, "\n", {}))
            {
                juce::String line;
                double lineW = 0.0;

                for (const auto& word : juce::StringArray::fromTokens (forced, " \t", {}))
                {
                    const double wordW = font.getStringWidth (word);

                    if (line.isEmpty())
                    {
                        line = word;              // first word on the line: no space to measure
                        lineW = wordW;
                    }
                    else if (lineW + spaceW + wordW <= w)
                    {
                        line += " ";
                        line += word;            // the gap the layout will measure as one space
                        lineW += spaceW + wordW;
                    }
                    else
                    {
                        out += (lines > 0 ? "\n" : "");
                        out += line;             // the line is full: close it
                        ++lines;

                        if (lines == maxLines)    // the next word has no line left to go on
                            return markTruncated (out, ellipsis, w);

                        line = word;
                        lineW = wordW;
                    }
                }

                out += (lines > 0 ? "\n" : "");
                out += line;
                ++lines;

                if (lines >= maxLines)
                    return out;   // the text ended on the last allowed line: nothing was cut
            }

            return out;
        }

        /** Words rejoined with single spaces - the same rule `visibleText` measures with, so a line measured here
            is exactly the line drawn. */
        juce::String joinWords (const juce::StringArray& words) const
        {
            juce::String s;

            for (int i = 0; i < words.size(); ++i)
                s += (i > 0 ? " " : "") + words[i];

            return s;
        }

        /** The lines of a block rejoined with the hard breaks they were written with. */
        juce::String joinLines (const juce::StringArray& lines) const
        {
            juce::String s;

            for (int i = 0; i < lines.size(); ++i)
                s += (i > 0 ? "\n" : "") + lines[i];

            return s;
        }

        /** The cut is marked, and the mark must fit: whole words are dropped from the kept tail until the ellipsis
            lands inside the width. Measured with this font's own widths, so a truncated caption never runs past the
            edge it was told to keep - and when one word cannot share its line with the mark, that line shows the
            mark alone, which says "this caption is shorter than what was written" as loudly as a glyph can. */
        juce::String markTruncated (const juce::String& out, const juce::String& ellipsis, int w) const
        {
            auto lines = juce::StringArray::fromTokens (out, "\n", {});

            if (lines.isEmpty())
                return ellipsis;

            auto words = juce::StringArray::fromTokens (lines[lines.size() - 1], " ", {});

            while (! words.isEmpty() && font.getStringWidth (joinWords (words) + ellipsis) > w)
                words.remove (words.size() - 1);   // drop the tail word: the mark has to fit inside the width

            lines.set (lines.size() - 1, joinWords (words) + ellipsis);
            return joinLines (lines);
        }

        /** Layout for `w` pixels wide, rebuilt only when something that changes the lines moved: width, text, font,
            colour, enabled state, truncation. A resize fires this once per real change, so a report inside a
            viewport costs one layout per relayout, not one per repaint. */
        const juce::TextLayout& buildLayout (int w) const
        {
            const auto key = static_cast<juce::int64> (colour.getARGB());   // the whole colour as one number, so a cache hit is exact
            const bool on = isEnabled();

            if (w == layoutWidth && text == layoutText
                && font.getTypefaceName() == layoutFontName
                && key == layoutColourKey && maxLines == layoutMaxLines && on == layoutEnabled
                && ! text.isEmpty())
                return layout;

            juce::AttributedString as (visibleText (w));   // sanitise at the edge is the host's rule, as for every component
            as.setFont (font);
            as.setColour (on ? colour : ebs::textDim());   // disabled dims, as everywhere in the library
            as.setWordWrap (juce::AttributedString::WordWrap::byWord);
            as.setJustification (alignment);
            as.setLineSpacing (kTextLeading);

            layout.createLayout (as, static_cast<float> (juce::jmax (1, w)));
            layoutWidth = w;
            layoutColourKey = key;
            layoutMaxLines = maxLines;
            layoutEnabled = on;
            layoutText = text;
            layoutFontName = font.getTypefaceName();
            return layout;
        }

        JUCE_LEAK_DETECTOR (WrappedText)
    };

} // namespace ebs
