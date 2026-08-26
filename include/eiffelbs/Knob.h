// Knob.h
// eiffelbs-ui - rotary knob component (EiffelBS design system).
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
// Adapted from OpenVoxTuner Source/ui/LookAndFeel.{h,cpp}
// (original copyright (C) 2026 EiffelBS).
//
// A light strongly-typed wrapper over juce::Slider. The rendering itself
// lives in the shared LookAndFeel (see ebs::LookAndFeel::drawRotarySlider
// -> ebs::drawKnob routing), so ANY juce::Slider in an app using
// ebs::LookAndFeel picks up the EiffelBS knob painting; this class exists
// so editors can declare intent (`ebs::Knob`) and gain the small helpers
// below while keeping every layout/configuration call unchanged.
//
// Rendering summary (identical pixels to the OpenVoxTuner origin):
//   - dark rounded arc track, accent value arc (pivot-centred optionally),
//   - vertical-gradient knob cap with soft black outline,
//   - thin white pointer line.
//
// Colours resolve through the standard EiffelBS three-level contract on
// the STANDARD juce::Slider rotary ids (rotarySliderFillColourId /
// rotarySliderOutlineColourId):
//   1. per-instance Component::setColour(),
//   2. theme hook ebs::LookAndFeel::widgetThemeColour(),
//   3. built-in palette (accent() / accentSoft()), evaluated live so a
//      Dark/Light switch is picked up on the next repaint without any
//      bookkeeping call.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "eiffelbs/LookAndFeel.h"

namespace ebs
{
    class Knob : public juce::Slider
    {
    public:
        /** No layout/style side effects: configure the knob exactly like
            any plain juce::Slider (range, text box, size...). */
        Knob() : juce::Slider() {}

        /** Fill-from-centre mode: the value arc grows symmetrically from
            the range midpoint towards the thumb instead of from the range
            start. Intended for centred ranges (formant -5..+5 semitones,
            blend -1..+1, ...). Equivalent to setting the legacy DynamicObject
            property "centred", which remains honoured by the painter. */
        void setCentredFill (bool shouldFillFromCentre)
        {
            getProperties().set ("centred", shouldFillFromCentre);
        }

        bool isCentredFill() const
        {
            const auto& props = getProperties();
            return props.contains ("centred") && (bool) props["centred"];
        }
    };
} // namespace ebs
