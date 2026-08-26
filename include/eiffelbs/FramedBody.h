// FramedBody.h
// eiffelbs-ui - rounded framed panel body component.
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
// Adapted from OpenTimbre source/ui/GenerationPanel.h (struct FramedBody,
// original copyright (C) 2026 EiffelBS).
//
// A plain child component that paints the shared EiffelBS frame language
// (continuous rounded bgPanel body + soft accent border via drawFramePlain).
// Titles remain the caller's job: add plain labels fully inside the frame.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "eiffelbs/LookAndFeel.h"

namespace ebs
{
    struct FramedBody : juce::Component
    {
        void paint (juce::Graphics& g) override
        {
            drawFramePlain (g, getLocalBounds().toFloat());
        }
    };
} // namespace ebs
