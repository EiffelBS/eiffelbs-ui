// ActivityDot.h
// eiffelbs-ui - one small dot that says whether a signal path is carrying anything.
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// A row of processors usually splits into several signal paths, and the user cannot
// see which of them is alive. A knob at 0 % and a path that carries nothing look
// exactly alike, so "no sound" is diagnosed by guessing. This component is the
// smallest honest answer to that question: a dot sized by the level the PATH
// carries, read from the audio the path actually produced - never from the value a
// control was set to.
//
// The component is a display only. It takes one number - a peak (linear 0 .. 1) -
// and never measures anything itself, so the audio code stays the single source of
// truth. Feed it from an atomic snapshot written by the audio thread and read by a
// timer at roughly 30 Hz; nothing here paints from the audio thread.
//
// Levels are read on a dB scale, because a linear level spends almost all of its
// travel near the top: silence below -45 dBFS leaves the dot unlit, full scale fills
// it. The dot ATTACKS in one call and RELEASES over about twenty, so a syllable
// between two notes reads as continuous activity instead of a flicker. Calls are
// cheap when nothing moves: the component repaints only when the disc it would draw
// actually changed, which is what its sibling LevelMeter promises too.
//
// Captions are supplied by the host: what a path is called belongs to the
// application and its locales, never to this library. The dot is deliberately
// wordless - it is placed next to a name the host already paints.
//
// Tooltip support comes from SettableTooltipClient, which is a separate interface
// from Component: a plain component shows nothing until it carries this base,
// exactly like Slider and Label do.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <eiffelbs/Theme.h>

namespace ebs
{

class ActivityDot final : public juce::Component, public juce::SettableTooltipClient
{
public:
    ActivityDot()
    {
        // A dot is an indicator, never a control: it must not swallow the clicks
        // meant for what it describes.
        setInterceptsMouseClicks (false, false);
    }

    /** The freshest peak the path produced, linear 0 .. 1, clamped. Call it once per
        UI tick with whatever the snapshot holds: the release ramp lives here, so a
        host that skips the call while its snapshot is still simply holds the dot. */
    void setLevel (float peakLinear)
    {
        const float feed = juce::jlimit (0.0f, 1.0f, peakLinear);
        level = feed;

        const float target = fillForLevel (feed);

        // Attack in one call, release by a fixed step: the dot answers "did this path
        // carry signal", so a peak it just saw must stay readable for a moment.
        const float before = fill;
        if (target > fill)
            fill = target;
        else
            fill = juce::jmax (target, fill - releaseStep);

        if (std::abs (fill - before) >= repaintEpsilon)
            repaint();
    }

    /** The last feed, clamped - not the smoothed disc. Hosts and tests read this to
        know what the snapshot said without asking the audio thread again. */
    float getLevel() const noexcept { return level; }

    /** The size of the disc as drawn, 0 .. 1: 0 is a dark slot, 1 is a full disc. */
    float getFill() const noexcept { return fill; }

    /** Drop straight back to silence, ramps skipped. A host calls this when the dot
        stops being on screen, so a level frozen in the snapshot cannot be painted by
        a later layout. */
    void reset()
    {
        level = 0.0f;
        fill = 0.0f;
        repaint();
    }

    /** Diameter of the disc in pixels. The dot is designed for a compact strip; the
        slot it is given decides the final size, so this is a cap, never a demand. */
    void setDotDiameter (int px)
    {
        dotDiameter = juce::jlimit (minDiameter, maxDiameter, px);
        repaint();
    }

    int getDotDiameter() const noexcept { return dotDiameter; }

    /** Opt out of the painted background, for a host that put the dot on a surface of
        its own colour. The disc and its ring are always drawn. */
    void setDrawsSlotBackground (bool shouldDraw)
    {
        drawSlotBackground = shouldDraw;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds();
        if (area.isEmpty() || area.getWidth() < 2 || area.getHeight() < 2)
            return;

        // The slot is the largest circle the host's rectangle can hold; nothing is
        // drawn outside it, so a tight strip never lets the dot touch its neighbours.
        const int slotD = juce::jmin (dotDiameter, juce::jmin (area.getWidth(), area.getHeight()));
        if (slotD < 2)
            return;

        const auto slot = area.withSizeKeepingCentre (slotD, slotD).toFloat();

        if (drawSlotBackground)
        {
            g.setColour (vizBg());
            g.fillEllipse (slot);
        }

        // The ring is always painted: an unlit dot must still read as a slot that
        // exists, not as a hole in the panel.
        g.setColour (panelBorder());
        g.drawEllipse (slot.reduced (0.5f), 1.0f);

        // Then the activity itself. The inner radius is what level fills, so a quiet
        // path shows a small disc and a loud one covers the slot.
        const float innerD = static_cast<float> (slotD) - 4.0f;
        if (innerD <= 0.0f)
            return;

        if (fill <= 0.0f)
        {
            // Unlit: one dim pip keeps the centre from looking broken.
            g.setColour (textDim().withAlpha (0.45f));
            g.fillEllipse (slot.withSizeKeepingCentre (2.0f, 2.0f));
            return;
        }

        const float discD = juce::jmax (2.0f, innerD * fill);
        g.setColour (outputColour());
        g.fillEllipse (slot.withSizeKeepingCentre (discD, discD));
    }

    /** The linear level the dB floor corresponds to: under any audible programme
        material and over the noise a stopped path leaves behind. */
    static float silenceFloorLinear() { return silenceFloorGain; }

private:
    /** Map a linear peak onto the disc's travel: the floor is 0, full scale is 1.
        The dB arithmetic is done here with <cmath> on purpose: juce::Decibels lives
        in juce_audio_basics, and a UI library must not drag an audio module into
        every translation unit that includes it. */
    static float fillForLevel (float peakLinear)
    {
        if (peakLinear <= silenceFloorGain)
            return 0.0f;

        const float db = 20.0f * std::log10 (peakLinear);
        return juce::jlimit (0.0f, 1.0f, (db - silenceFloorDb) / (0.0f - silenceFloorDb));
    }

    static constexpr float silenceFloorDb   = -45.0f;
    static constexpr float silenceFloorGain = 0.0056234133f;   // 10 ^ (-45 / 20)
    static constexpr float releaseStep     = 0.05f;   // ~20 ticks, i.e. ~0.6 s at 30 Hz
    static constexpr float repaintEpsilon  = 0.004f;  // below this the disc did not move
    static constexpr int   minDiameter     = 6;
    static constexpr int   maxDiameter     = 24;

    float level = 0.0f;   // the last feed, as handed in (clamped)
    float fill  = 0.0f;   // the smoothed disc, what paint() draws
    int dotDiameter = 10;
    bool drawSlotBackground = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ActivityDot)
};

} // namespace ebs
