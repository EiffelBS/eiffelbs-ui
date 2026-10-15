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
            const float arm = 5.0f, half = 3.6f;
            const auto c = b.getCentre();
            juce::Path v;
            if (expanded)
            {
                v.startNewSubPath (c.x + arm * 0.5f, c.y - half);
                v.lineTo (c.x - arm * 0.5f, c.y);
                v.lineTo (c.x + arm * 0.5f, c.y + half);
            }
            else
            {
                v.startNewSubPath (c.x - arm * 0.5f, c.y - half);
                v.lineTo (c.x + arm * 0.5f, c.y);
                v.lineTo (c.x - arm * 0.5f, c.y + half);
            }
            g.strokePath (v, juce::PathStrokeType (1.8f,
                                                   juce::PathStrokeType::mitered,
                                                   juce::PathStrokeType::rounded));
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
