// ActivityKnob.h
// eiffelbs-ui - a rotary knob that carries its own activity dot at its centre.
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// A component that owns exactly ONE knob has nowhere to put an activity dot: the cell is
// 74 px wide, its power toggle already spends 18 px on the tick well, and a dot beside the
// label needs 80 to 94 px in the shipped languages (measured by OpenVoxTuner's
// EffectsCellLayoutTest). A strip under the cell costs 12 px of height and reads as a
// second, separate object.
//
// This component answers that: the dot IS the knob's hub. The dial is drawn by the shared
// LookAndFeel exactly as every other ebs::Knob, and the disc goes on top of its centre -
// which is free by construction, because the pointer line starts 8 px from the centre and
// grows outward. The dot is 9 px across by default, so it clears the pointer with air to
// spare, and it never takes a click: the knob still drags from anywhere on its face.
//
// The level contract is ActivityDot's, literally: ebs::ActivityState holds the level, the
// dB mapping and the release ramp, and ebs::paintActivityDisc draws it, so a dot in a knob
// and a dot on a panel can never drift apart. Feed setActivityLevel() from the host's timer
// at ~30 Hz, never from the audio thread.
//
// The dot never covers the pointer line: ebs::drawKnob runs that line from 8 px inside the
// rim towards the middle and stops it 0.6 x radius - 5.6 px short of the centre, so the disc
// is capped to stay inside that clearance. A dial too small for a readable dot shows none -
// the smallest knob in OpenVoxTuner's Effects row (31 px) is exactly that case.
//
// No slot background by default: on the knob's dark cap a dark disc would read as a hole,
// so the unlit state is the ring and its dim pip, and the lit state is the accent disc.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "eiffelbs/ActivityDot.h"
#include "eiffelbs/Knob.h"

namespace ebs
{

// No SettableTooltipClient base here: juce::Slider already carries that interface, and inheriting
// it a second time would make setTooltip()/getTooltip() ambiguous at every call site.
class ActivityKnob final : public Knob
{
public:
    ActivityKnob()
    {
        // The dot is an indicator, and the knob under it must keep every click: the component
        // is a control first, so nothing here changes hit-testing or dragging.
        setActivityDotDiameter (9);
    }

    /** The freshest peak the knob's path produced, linear 0 .. 1, clamped. Call it once per UI
        tick: the release ramp lives in the shared state, so a host that skips the call while its
        snapshot is still simply holds the dot. Repaints only when the disc actually moved. */
    void setActivityLevel (float peakLinear)
    {
        if (state.setLevel (peakLinear))
            repaint();
    }

    /** The last feed, clamped - not the smoothed disc. */
    float getActivityLevel() const noexcept { return state.level; }

    /** The size of the disc as drawn, 0 .. 1: 0 is a dark ring, 1 is a full disc. */
    float getActivityFill() const noexcept { return state.fill; }

    /** Straight back to silence, ramps skipped, for a host that hides the knob. */
    void resetActivity()
    {
        state.reset();
        repaint();
    }

    /** Diameter of the dot in pixels, clamped to the dot's own design range. The knob's face
        decides the final size: a small dial draws a smaller dot rather than a disc that spills
        over its cap. */
    void setActivityDotDiameter (int px)
    {
        activityDotDiameter = juce::jlimit (ActivityState::minDiameter, ActivityState::maxDiameter, px);
        repaint();
    }

    int getActivityDotDiameter() const noexcept { return activityDotDiameter; }

    /** Paint the slot background behind the disc. OFF by default: the knob's cap is dark, and a
        dark slot on it reads as a hole rather than as an indicator. */
    void setActivityDrawsSlotBackground (bool shouldDraw)
    {
        activityDrawsSlotBackground = shouldDraw;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        // The dial first, through the shared LookAndFeel, exactly as a plain ebs::Knob draws it.
        Knob::paint (g);

        // Then the dot ON TOP, at the centre of the bounds - which is the dial's own centre
        // (ebs::drawKnob centres the cap and starts the pointer 8 px out from there).
        const auto area = getLocalBounds();
        if (area.getWidth() < 4 || area.getHeight() < 4)
            return;

        // How far the pointer line stops short of the centre: ebs::drawKnob runs it from
        // (-radius + 8) towards the middle for 40 % of (radius - 6), so its tip sits
        // 0.6 x radius - 5.6 px from the centre. The dot must stay INSIDE that, or it would
        // paint over the very indicator that says what the knob is set to - and a knob too
        // small for a readable dot simply shows none, because an indicator that hides its own
        // control is worse than no indicator.
        const float radius       = static_cast<float> (juce::jmin (area.getWidth(), area.getHeight())) * 0.5f - 4.0f;
        const float pointerInner = 0.6f * radius - 5.6f;
        if (pointerInner <= 3.0f)
            return;

        const float dotD = juce::jmin (static_cast<float> (activityDotDiameter),
                                       static_cast<float> (juce::jmax (6, activityDotDiameter)),
                                       2.0f * (pointerInner - 1.0f));
        if (dotD < 6.0f)
            return;

        paintActivityDisc (g, static_cast<float> (area.getCentreX()),
                           static_cast<float> (area.getCentreY()),
                           dotD, state.fill, activityDrawsSlotBackground);
    }

private:
    ActivityState state;
    int activityDotDiameter = 9;
    bool activityDrawsSlotBackground = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ActivityKnob)
};

} // namespace ebs
