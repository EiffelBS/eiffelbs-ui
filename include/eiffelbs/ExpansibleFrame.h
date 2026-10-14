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
// sizes the frame (wider when open - that is what pushes the neighbour),
// lays its own widgets into interior(), and is told through
// onExpandedChanged when the fold flips so it can re-lay out. Children
// added to the frame clip and paint inside it, so a folded zone is a
// setVisible() away from gone, never an overwritten one.

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
            chevron.expanded = open;
            if (onExpandedChanged != nullptr)
                onExpandedChanged();
        }

        /** The disclosure strip itself, for a host that wants to set its
         *  tooltip (the default says what the click does). */
        ebs::DisclosureChevron& disclosure() noexcept { return chevron; }

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
