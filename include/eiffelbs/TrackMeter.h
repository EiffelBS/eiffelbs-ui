// TrackMeter.h
// eiffelbs-ui - peak level meter for mixer channels and track headers.
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// Display only: the caller hands in a linear peak (already smoothed or held
// by its own ballistics) and the meter maps it onto a -60..0 dBFS scale with
// green / amber / red zones. Nothing here computes audio, and nothing here
// paints from the audio thread.
//
// Two entry points:
//   - ebs::TrackMeter, a component (vertical or horizontal) for a fixed slot;
//   - ebs::TrackMeter::paint(), a static painter for meters drawn inside a
//     parent's paint() (a lane header that has no child component).

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>

#include "eiffelbs/Theme.h"

namespace ebs
{
    class TrackMeter : public juce::Component
    {
    public:
        enum class Orientation { horizontal, vertical };

        explicit TrackMeter (Orientation orientationIn = Orientation::vertical)
            : orientation (orientationIn)
        {
        }

        /** Linear peak, 1.0 = 0 dBFS. */
        void setLevel (float linearPeak)
        {
            if (linearPeak == level)
                return;
            level = linearPeak;
            repaint();
        }

        void setOrientation (Orientation orientationIn)
        {
            orientation = orientationIn;
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            paint (g, getLocalBounds(), level, orientation);
        }

        /** Fraction of the scale filled by a linear peak: 0 at -60 dBFS or
            below, 1 at 0 dBFS and above. */
        static float fraction (float linearPeak) noexcept
        {
            const auto db = 20.0f * std::log10 (juce::jmax (linearPeak, 1.0e-6f));
            return juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
        }

        static void paint (juce::Graphics& g, juce::Rectangle<int> bounds,
                           float linearPeak, Orientation orientation)
        {
            const auto area = bounds.toFloat();
            g.setColour (bgDark().brighter (0.12f));
            g.fillRoundedRectangle (area, 2.0f);

            const auto amount = fraction (linearPeak);
            if (amount <= 0.0f)
                return;

            // Zones on the fill: green up to -12 dB, amber up to -3 dB, red above.
            const auto zoneColour = [] (float fillFraction)
            {
                if (fillFraction > 0.9f)
                    return danger();
                if (fillFraction > 0.75f)
                    return juce::Colour (0xffe8c34a);
                return juce::Colour (0xff47b79c);
            };

            if (orientation == Orientation::vertical)
            {
                const auto filled = area.withTop (area.getBottom() - area.getHeight() * amount);
                g.setColour (zoneColour (amount));
                g.fillRoundedRectangle (filled, 2.0f);
            }
            else
            {
                const auto filled = area.withWidth (area.getWidth() * amount);
                g.setColour (zoneColour (amount));
                g.fillRoundedRectangle (filled, 2.0f);
            }
        }

    private:
        Orientation orientation;
        float level = 0.0f;
    };
}
