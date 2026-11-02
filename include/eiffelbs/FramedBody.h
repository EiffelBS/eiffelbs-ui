// FramedBody.h
// eiffelbs-ui - rounded framed panel body component.
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
// Adapted from an in-house generation panel (struct FramedBody,
// original copyright (C) 2026 EiffelBS).
//
// A plain child component that paints the shared EiffelBS frame language
// (continuous rounded bgCard body + neutral border via drawCardFrame).
// It is a GROUP nested in a page, so it takes the CARD surface: painting the
// page surface here made a card and the page it sits on the same pixels
// (measured on the OpenTimbre Voice tab, 2026-11-02: both #373A3E, so a page
// built from frames read as one flat slab).
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
            drawCardFrame (g, getLocalBounds().toFloat());
        }
    };
} // namespace ebs
