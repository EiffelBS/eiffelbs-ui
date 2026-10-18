// DisclosureChevron.h
// eiffelbs-ui - shared disclosure strip: a chevron that folds a zone
// open or closed.
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// A vertical strip that folds a zone open or closed with ONE click.
//
// A CHEVRON, not a triangle: a filled triangle is a STATE, a chevron is a
// DIRECTION - it points the way the zone is about to move. Folded, the
// zone opens to the right and the chevron points right; open, a click
// folds it back to the left and the chevron points left. A reader who has
// to decode a glyph to know what a click will do is reading a decoration,
// not a control.
//
// The DIRECTION follows the AXIS the zone grows on, never the strip's own
// place. A frame that grows TALLER (a short, wide engine frame in a bottom
// band, where a horizontal strip would spend height the frame does not
// have) keeps the strip on its right edge but draws a DOWN chevron when
// folded and an UP one when open: the strip's position says where the
// control is, the glyph says where the content goes. See
// DisclosureChevron::setVerticalFold.
//
// Transparent at rest with a quiet accent hover (grey reads as disabled),
// and the chevron is two thin strokes - the same language as
// MenuChevronButton's V, which is what makes it read as "this opens
// something" rather than "this is a spinner".

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

#include "eiffelbs/Theme.h"

namespace ebs
{
    /** The four ways a chevron may point. Most folds are HORIZONTAL (a zone
     *  opens sideways) and use Left / Right; a fold that grows downward uses
     *  Down when folded and Up when open. */
    enum class ChevronDirection { Left, Right, Up, Down };

    /** The shared chevron glyph, drawn inside an area. One tip direction per
     *  call, three users: the fold strip, FrameBand's scroll cues and a frame
     *  that grows on the vertical axis. The caller sets the stroke colour;
     *  the glyph itself never carries a colour. */
    inline void drawChevronGlyph (juce::Graphics& g,
                                  juce::Rectangle<float> area,
                                  ChevronDirection dir)
    {
        const float arm = 5.0f, half = 3.6f;
        const auto c = area.getCentre();
        juce::Path v;

        switch (dir)
        {
            case ChevronDirection::Left:
                v.startNewSubPath (c.x + arm * 0.5f, c.y - half);
                v.lineTo (c.x - arm * 0.5f, c.y);
                v.lineTo (c.x + arm * 0.5f, c.y + half);
                break;

            case ChevronDirection::Right:
                v.startNewSubPath (c.x - arm * 0.5f, c.y - half);
                v.lineTo (c.x + arm * 0.5f, c.y);
                v.lineTo (c.x - arm * 0.5f, c.y + half);
                break;

            case ChevronDirection::Up:
                v.startNewSubPath (c.x - half, c.y + arm * 0.5f);
                v.lineTo (c.x, c.y - arm * 0.5f);
                v.lineTo (c.x + half, c.y + arm * 0.5f);
                break;

            case ChevronDirection::Down:
                v.startNewSubPath (c.x - half, c.y - arm * 0.5f);
                v.lineTo (c.x, c.y + arm * 0.5f);
                v.lineTo (c.x + half, c.y - arm * 0.5f);
                break;
        }

        g.strokePath (v, juce::PathStrokeType (1.8f,
                                               juce::PathStrokeType::mitered,
                                               juce::PathStrokeType::rounded));
    }

    /** The horizontal pair, as the boolean the original callers pass
     *  ("pointsLeft" is the fold's own `expanded` flag). */
    inline void drawChevronGlyph (juce::Graphics& g, juce::Rectangle<float> area, bool pointsLeft)
    {
        drawChevronGlyph (g, area,
                          pointsLeft ? ChevronDirection::Left : ChevronDirection::Right);
    }

    //==============================================================================
    /** A small left/right chevron strip that reports a toggle.
     *
     *  It is the fold control used by ExpansibleFrame, and the same glyph the band
     *  scroll cues draw. The host sets its tooltip: this class sets none.
     */
    class DisclosureChevron : public juce::Component,
                              public juce::SettableTooltipClient
    {
    public:
        DisclosureChevron()
        {
            // The strip owns its own clicks and nothing else, and takes no
            // keyboard focus: it is a disclosure, not a tab stop.
            setInterceptsMouseClicks (true, false);
            setWantsKeyboardFocus (false);
        }

        /** Called on every click. The host flips its own flag and re-lays
         *  out - the strip knows nothing about what it folds. */
        std::function<void()> onToggle;

        /** True when the zone is OPEN (the chevron points LEFT, back). */
        bool expanded = false;

        /** Which axis the zone grows on. Horizontal (the default) keeps the
         *  strip's own reading - a sideways fold, chevron left / right.
         *  Vertical means the zone grows DOWNWARD: the strip stays where it
         *  is (a right edge is the only place a short, wide frame can afford
         *  to put it) but the glyph points down when folded and up when open,
         *  because that is what the click will do. */
        void setVerticalFold (bool vertical)
        {
            if (verticalFold == vertical)
                return;
            verticalFold = vertical;
            repaint();
        }
        bool foldIsVertical() const noexcept { return verticalFold; }

        /** The way the glyph points for the current fold state and axis. */
        ChevronDirection chevronDirection() const noexcept
        {
            if (verticalFold)
                return expanded ? ChevronDirection::Up : ChevronDirection::Down;
            return expanded ? ChevronDirection::Left : ChevronDirection::Right;
        }

        /** Set the fold state AND repaint. Writing `expanded` directly
         *  changes the flag only: a host that moves the strip gets its
         *  repaint from setBounds, a host that keeps it in place gets none. */
        void setExpanded (bool isOpen)
        {
            if (expanded == isOpen)
                return;
            expanded = isOpen;
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            const auto b = getLocalBounds().toFloat();
            if (b.isEmpty())
                return;
            const bool hot = hovered && isEnabled();
            if (hot)
            {
                g.setColour (ebs::accent().withAlpha (0.10f));
                g.fillRoundedRectangle (b.reduced (1.0f), 4.0f);
            }
            g.setColour (hot ? ebs::accent().brighter (0.20f) : ebs::accent());
            drawChevronGlyph (g, b, chevronDirection());
        }

        void mouseEnter (const juce::MouseEvent&) override { setHovered (true); }
        void mouseMove  (const juce::MouseEvent&) override { setHovered (true); }
        void mouseExit  (const juce::MouseEvent&) override { setHovered (false); }

        void mouseUp (const juce::MouseEvent&) override
        {
            // A disclosure, not a drag: one click, one toggle.
            if (isEnabled() && onToggle != nullptr)
                onToggle();
        }

    private:
        bool hovered = false;
        bool verticalFold = false;

        void setHovered (bool h)
        {
            if (hovered == h)
                return;
            hovered = h;
            setMouseCursor (h && isEnabled()
                ? juce::MouseCursor::PointingHandCursor
                : juce::MouseCursor::NormalCursor);
            repaint();
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DisclosureChevron)
    };
} // namespace ebs
