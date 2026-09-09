// ToggleSwitch.h
// eiffelbs-ui - iOS-style sliding switch (EiffelBS design system).
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// A rounded-pill track with a circular knob that slides to the right and
// turns the accent colour (blue) when engaged - the switch idiom expected
// in settings pages, as opposed to ebs::PowerToggle (power glyph, for
// "engine on/off" moments).
//
// Interaction is typing-replacing, like PowerToggle: declare ebs::ToggleSwitch,
// read getToggleState() / hook onClick - the component is a plain
// juce::ToggleButton under the hood.
//
// ColourIds follow the shared three-level contract:
//   trackOnColourId  - engaged track tint (built-in accent() = blue)
//   trackOffColourId - idle track tint (built-in text().withAlpha)
// Knob + text derive live from the palette, so a Dark/Light switch
// repaints correctly without bookkeeping. Disabled state dims everything.
//
// Animation: the knob glides between ends (~120 ms) instead of snapping.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include <cmath>

#include "eiffelbs/LookAndFeel.h"

namespace ebs
{
    class ToggleSwitch : public juce::ToggleButton,
                         private juce::Timer
    {
    public:
        enum ColourIds
        {
            trackOnColourId  = 0x2b7e1401,   ///< engaged track tint
            trackOffColourId = 0x2b7e1402    ///< idle track tint
        };

        ToggleSwitch() : juce::ToggleButton()
        {
            setColour (trackOnColourId,  ebs::accent());
            setColour (trackOffColourId, ebs::text().withAlpha (0.18f));
        }

        void paint (juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat();
            const bool isOn = getToggleState();

            // Pill = full bounds (caller sizes it, ~44x22 is the sweet spot).
            const float h     = bounds.getHeight();
            const float knobD = h - 6.0f;
            const float trackR = h * 0.5f;

            const juce::Colour trackOn  = widgetColour (*this, trackOnColourId,  ebs::accent());
            const juce::Colour trackOff = widgetColour (*this, trackOffColourId, ebs::text().withAlpha (0.18f));
            juce::Colour trackCol = isOn ? trackOn : trackOff;
            if (! isEnabled())
                trackCol = trackCol.withAlpha (trackCol.getAlpha() * 0.45f);

            g.setColour (trackCol);
            g.fillRoundedRectangle (bounds, trackR);

            // Knob: slides between ends; white/palette-bright disc with a
            // soft edge. Position follows the animation value.
            const float travel = bounds.getWidth() - knobD - 6.0f;
            const float knobX  = 3.0f + travel * animPos.load();
            const juce::Colour knobCol = isEnabled()
                ? juce::Colour (0xFFF2F2F5)
                : ebs::text().withAlpha (0.35f);
            const juce::Colour knobEdge = isOn && isEnabled()
                ? trackOn.darker (0.25f).withAlpha (0.35f)
                : ebs::text().withAlpha (0.15f);

            juce::Path knobPath;
            knobPath.addEllipse (knobX, 3.0f, knobD, knobD);
            g.setColour (knobEdge);
            g.strokePath (knobPath, juce::PathStrokeType (1.0f));
            g.setColour (knobCol);
            g.fillEllipse (knobX, 3.0f, knobD, knobD);
        }

        /** Any state change (click OR programmatic setToggleState) glides
            the knob to its new end. */
        void buttonStateChanged() override
        {
            startTimerHz (60);
        }

        void enablementChanged() override
        {
            repaint();
        }

    private:
        void timerCallback() override
        {
            const float target = getToggleState() ? 1.0f : 0.0f;
            float p = animPos.load();
            const float step = 0.12f;
            p = std::abs (target - p) <= step ? target : p + step * (target > p ? 1.0f : -1.0f);
            animPos = p;
            repaint();
            if (p == target)
                stopTimer();
        }

        std::atomic<float> animPos { 0.0f };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToggleSwitch)
    };
} // namespace ebs
