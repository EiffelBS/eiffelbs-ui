// LevelMeter.h
// eiffelbs-ui - compact output level meter: peak bar, gain-reduction segment, dB readouts.
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// Built for the tight slot under a knob inside an effects cell, so it costs no width:
// the host puts it in the SAME cell as the control it describes.
//
// The component is a display only. It takes three numbers - the smoothed output peak
// (linear), the gain reduction in dB, and the trim in dB - and never computes them, so
// the audio code stays the single source of truth. Feed it from an atomic snapshot
// written by the audio thread and read by the editor timer; nothing here paints from
// the audio thread.
//
// Captions are supplied by the host: translations belong to the application, not to
// this library.
//
// Tooltip support comes from SettableTooltipClient, which is a separate interface from
// Component: a plain component shows nothing until it carries this base, exactly like
// Slider and Label do.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <eiffelbs/Theme.h>
#include <eiffelbs/Fonts.h>

namespace ebs
{

class LevelMeter final : public juce::Component, public juce::SettableTooltipClient
{
public:
    LevelMeter()
    {
        setInterceptsMouseClicks (false, false);
    }

    /** Smoothed peak of the signal leaving the stage, linear 0 .. 1. Values outside
        that range are clamped, so a hot feed cannot draw past the bar. */
    void setPeak (float peakLinear)
    {
        peak = juce::jlimit (0.0f, 1.0f, peakLinear);
        repaint();
    }

    /** Gain reduction in dB, as a POSITIVE attenuation. 0 means the stage is not
        touching the gain, and no segment is drawn. */
    void setGainReduction (float grDb)
    {
        gr = (grDb > 0.0f) ? grDb : 0.0f;
        repaint();
    }

    /** The trim the user set, in dB, shown as a reading on the right of the text line. */
    void setTrim (float trimDb)
    {
        trim = trimDb;
        repaint();
    }

    /** Host-supplied captions, so the wording stays in the application's locale.
        Short ones read best: the component is designed for a 74 px cell. */
    void setCaptions (const juce::String& grCaption, const juce::String& trimCaption)
    {
        grCaptionText = grCaption;
        trimCaptionText = trimCaption;
        repaint();
    }

    /** Height of the bar in pixels; the rest of the component is the text line. */
    void setBarHeight (int h)
    {
        barHeight = juce::jlimit (2, 40, h);
        repaint();
    }

    float getPeak() const noexcept { return peak; }
    float getGainReduction() const noexcept { return gr; }
    float getTrim() const noexcept { return trim; }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds();
        if (area.isEmpty())
            return;

        const int barH = juce::jmin (barHeight, area.getHeight());
        const auto bar = area.removeFromTop (barH);

        g.setColour (vizBg());
        g.fillRect (bar);
        g.setColour (panelBorder());
        g.drawRect (bar, 1);

        const float w = static_cast<float> (bar.getWidth() - 2);
        const int left = bar.getX() + 1;
        if (w <= 0.0f)
            return;

        // Gain reduction first: the level the signal asked for, minus what it got.
        // The segment spans from the level that left the stage up to the level that
        // was caught, so a falling peak shows a shrinking red tail.
        if (gr > 0.0f)
        {
            const float wanted = juce::jlimit (0.0f, 1.0f, peak * std::pow (10.0f, gr / 20.0f));
            const int xPeak   = left + static_cast<int> (peak * w);
            const int xWanted = left + static_cast<int> (wanted * w);
            g.setColour (danger());
            g.fillRect (juce::Rectangle<int> (xPeak, bar.getY() + 1,
                                              juce::jmax (0, xWanted - xPeak), barH - 2));
        }

        // Then the level that actually left the stage.
        const int fillW = static_cast<int> (peak * w);
        if (fillW > 0)
        {
            g.setColour (outputColour());
            g.fillRect (juce::Rectangle<int> (left, bar.getY() + 1, fillW, barH - 2));
        }

        if (area.isEmpty())
            return;

        // One text line: gain reduction on the left, trim on the right.
        g.setFont (createFont (8.0f, false));
        g.setColour (gr > 0.0f ? danger() : textDim());
        g.drawText (grCaptionText + " " + juce::String (gr, 1),
                    area, juce::Justification::centredLeft, false);

        g.setColour (text());
        g.drawText (trimCaptionText + " " + juce::String (trim, 1),
                    area, juce::Justification::centredRight, false);
    }

private:
    float peak = 0.0f;
    float gr = 0.0f;
    float trim = 0.0f;
    int barHeight = 5;
    juce::String grCaptionText = "GR";
    juce::String trimCaptionText = "TRIM";

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelMeter)
};

} // namespace ebs
