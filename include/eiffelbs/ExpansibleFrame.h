// ExpansibleFrame.h
// eiffelbs-ui - a framed body that folds a zone open or closed.
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// The karaoke player's lower band needed a frame whose width GROWS when a
// zone opens, pushing its neighbour aside, and a strip that says so with
// one click. A host that copies that pair owns three things that drift:
// the frame paint, the chevron's placement, the fold flag and the
// chevron's direction.
//
// This component owns exactly those three. The HOST owns the layout: it
// sizes the frame (wider when open on the default HORIZONTAL axis - that
// is what pushes the neighbour - taller when open on the VERTICAL one,
// see setFoldVertical), lays its own widgets into interior(), and is told
// through onExpandedChanged when the fold flips so it can re-lay out.
// Children added to the frame clip and paint inside it, so a folded zone
// is a setVisible() away from gone, never an overwritten one.
// One thing the host must not forget: when the frame MOVES, JUCE repaints
// only the rectangles the frame's own widgets vacated. A host that paints
// its background behind the frame - blocks drawn in paint(), not components
// - has to repaint those regions itself in answer to onExpandedChanged.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

#include "eiffelbs/DisclosureChevron.h"
#include "eiffelbs/LookAndFeel.h"   // drawFramePlain: the shared frame body

namespace ebs
{
    class ExpansibleFrame : public juce::Component
    {
    public:
        ExpansibleFrame()
        {
            addAndMakeVisible (chevron);
            chevron.onToggle = [this] { setExpanded (! expanded()); };
        }

        /** Called after the fold flips, so the host can re-lay out: the
         *  frame usually GROWS, and a neighbour is usually pushed. */
        std::function<void()> onExpandedChanged;

        bool expanded() const noexcept { return open; }

        void setExpanded (bool shouldOpen)
        {
            if (open == shouldOpen)
                return;
            open = shouldOpen;
            chevron.setExpanded (open);
            if (onExpandedChanged != nullptr)
                onExpandedChanged();
        }

        /** The disclosure strip itself, for a host that wants to set its
         *  tooltip (the default says what the click does). */
        ebs::DisclosureChevron& disclosure() noexcept { return chevron; }

        /** The axis the folded zone grows on. Horizontal (the DEFAULT, and
         *  unchanged for every existing host) is the sideways fold: the frame
         *  gets WIDER, and that is what pushes its neighbour. Vertical means
         *  the zone grows DOWNWARD and the frame gets TALLER - what a short,
         *  wide frame in a bottom band needs, since it has no width to spare
         *  and all the room below it.
         *  The strip stays on the RIGHT edge in both cases (a horizontal
         *  strip would spend the very height such a frame is short of);
         *  only the GLYPH follows the axis - down when folded, up when open -
         *  because a chevron pointing right at a zone that opens downward is
         *  exactly the decoration this class exists to refuse.
         *  The HOST still owns the sizing: it returns the taller
         *  `preferredHeight()` from onExpandedChanged (see the note below). */
        void setFoldVertical (bool vertical)
        {
            chevron.setVerticalFold (vertical);
        }
        bool foldIsVertical() const noexcept { return chevron.foldIsVertical(); }

        /** The content area, the chevron slot excluded. The host applies
         *  its own padding to it. */
        juce::Rectangle<int> interior() const
        {
            auto r = getLocalBounds();
            r.removeFromRight (chevronWidth() + chevronGap());
            return r;
        }

        static constexpr int chevronWidth() { return 22; }
        static constexpr int chevronGap()   { return 6; }

        void paint (juce::Graphics& g) override
        {
            // The shared frame language: continuous rounded body + soft
            // accent border. Children are added after, so they paint over.
            drawFramePlain (g, getLocalBounds().toFloat());
        }

        void resized() override
        {
            // The strip rides the RIGHT edge, inside the frame, inset a
            // little top and bottom so it reads as part of the frame.
            chevron.setBounds (getLocalBounds()
                                   .removeFromRight (chevronWidth())
                                   .reduced (0, 8));
        }

    private:
        ebs::DisclosureChevron chevron;
        bool open = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExpansibleFrame)
    };
} // namespace ebs
