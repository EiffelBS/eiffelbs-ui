// MorphSlider.h
// eiffelbs-ui - pivot-centred horizontal slider (EiffelBS design system).
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
// Adapted from OpenVoxTuner Source/ui/LookAndFeel.cpp drawLinearSlider
// "Morph" branch (original copyright (C) 2026 EiffelBS).
//
// A slim 4 px track whose filled part grows symmetrically from the
// horizontal CENTRE towards the thumb (A/B crossfade look) instead of
// from the track start - ideal for 0..1 blend/crossfade parameters.
//
// Typing replaces dispatch: the origin app routed by Component name
// ("Morph"); consumers just declare ebs::MorphSlider and keep every other
// call (range, attachment, listeners...) unchanged.
//
// Colours resolve through the STANDARD juce::Slider ids with the usual
// instance-set wins first semantics. Built-ins mirror the origin palette
// (bgPanel() track bed, accent().withAlpha(0.7f) fill, accent() thumb),
// so a bare widget is themed correctly without any setColour call.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "eiffelbs/Theme.h"

namespace ebs
{
    class MorphSlider : public juce::Slider
    {
    public:
        MorphSlider()
        {
            setSliderStyle (juce::Slider::LinearHorizontal);
            setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);

            setColour (juce::Slider::backgroundColourId, bgPanel());
            setColour (juce::Slider::trackColourId,      accent().withAlpha (0.7f));
            setColour (juce::Slider::thumbColourId,      accent());
        }

        void paint (juce::Graphics& g) override
        {
            constexpr float trackHeight = 4.0f;
            constexpr float thumbWidth  = 12.0f;

            const auto b          = getLocalBounds().toFloat();
            const auto trackTop   = b.getY() + (b.getHeight() - trackHeight) * 0.5f;
            const auto trackLeft  = b.getX() + thumbWidth * 0.5f;
            const auto trackRight = b.getRight() - thumbWidth * 0.5f;
            const auto trackWidth = trackRight - trackLeft;

            // Proportional position (skew-aware) mapped to pixels.
            const double norm     = getNormalisableRange().convertTo0to1 (getValue());
            const float sliderPos = trackLeft + trackWidth * (float) norm;

            const juce::Rectangle<float> trackBounds (trackLeft, trackTop,
                                                      trackWidth, trackHeight);

            // Full track bed, slightly brighter than the caller colour.
            g.setColour (findColour (juce::Slider::backgroundColourId).brighter (0.15f));
            g.fillRoundedRectangle (trackBounds, trackHeight * 0.5f);

            // Pivot-centred fill: grows from the central value towards the
            // thumb, both directions.
            const float pivotCentre = trackLeft + trackWidth * 0.5f;
            const float fillLeft    = juce::jmin (sliderPos, pivotCentre);
            const float fillRight   = juce::jmax (sliderPos, pivotCentre);
            const float fillW       = juce::jmax (0.0f, fillRight - fillLeft);
            if (fillW > 0.0f)
            {
                g.setColour (findColour (juce::Slider::trackColourId));
                g.fillRoundedRectangle ({ fillLeft, trackTop, fillW, trackHeight },
                                        trackHeight * 0.5f);
            }

            // Thumb.
            const float thumbCentreY = trackTop + trackHeight * 0.5f;
            g.setColour (findColour (juce::Slider::thumbColourId));
            g.fillEllipse (sliderPos - thumbWidth * 0.5f,
                           thumbCentreY - thumbWidth * 0.5f,
                           thumbWidth, thumbWidth);
        }
    };
} // namespace ebs
