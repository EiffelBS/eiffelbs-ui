// HelpBubble.h
// eiffelbs-ui - shared "?" help bubble (EiffelBS design system).
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
// Adapted from OpenTimbre source/ui/HelpBubble.h (original copyright
// (C) 2026 EiffelBS), where it is used by the Generation and TTS panels.
//
// A tiny 14x14 accent circle with a "?" glyph; hovering shows the tooltip
// (the host window owns the shared TooltipWindow). Section instructions
// live in the bubble instead of being glued to titles: every "?" uses the
// exact same look across panels and projects. Pair it with a label and
// set the tooltip via SettableTooltipClient::setTooltip.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <eiffelbs/Theme.h>
#include <eiffelbs/Fonts.h>

namespace ebs
{

struct HelpBubble : public juce::Component,
                    public juce::SettableTooltipClient
{
    HelpBubble() { setSize (14, 14); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (ebs::accent().withAlpha (0.15f));
        g.fillEllipse (r);
        g.setColour (ebs::accent());
        g.drawEllipse (r, 1.0f);
        g.setFont (ebs::fontSectionLabel().withHeight (10.0f));
        g.drawText ("?", getLocalBounds(), juce::Justification::centred);
    }
};

} // namespace ebs
