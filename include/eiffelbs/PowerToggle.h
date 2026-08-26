// PowerToggle.h
// eiffelbs-ui - power-style glowing toggle (EiffelBS design system).
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
// Adapted from OpenVoxTuner Source/ui/LookAndFeel.cpp drawToggleButton
// PowerButton branch (original copyright (C) 2026 EiffelBS).
//
// A circular power glyph (broken ring + stem) that glows warm yellow when
// engaged, next to an optional text label taken from the Button text.
// Typing replaces dispatch: the origin app branch matched Component name
// ("PowerButton") or magic labels ("ON"/"Power"); consumers now simply
// declare ebs::PowerToggle. Plain juce::ToggleButtons in the same app keep
// their checkbox LookAndFeel untouched.
//
// Disabled state dims the stem/ring and drops the glow (visible
// non-interactive cue, exactly like the origin rendering).
//
// ColourIds follow the shared three-level contract:
//   glowColourId - engaged halo/arc tint (built-in 0xFFE8D050 gold).
// Everything else derives live from the palette (text(), alpha ramps),
// so a Dark/Light switch repaints correctly without bookkeeping.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "eiffelbs/LookAndFeel.h"

namespace ebs
{
    // NB: juce::ToggleButton already carries SettableTooltipClient - adding
    // it here would trip MSVC C4584 (duplicate indirect base).
    class PowerToggle : public juce::ToggleButton
    {
    public:
        enum ColourIds
        {
            glowColourId = 0x2b7e1301   ///< engaged glow + glyph tint
        };

        PowerToggle() : juce::ToggleButton() {}

        /** Legacy compatibility: the origin code flagged instances by
            name ("PowerButton")/labels ("ON"/"Power"). Those markers are
            ignored visually by this widget; they stay legal no-ops so
            call sites may carry over unchanged during migration. */

        void paint (juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat();
            const bool isOn = getToggleState();

            const float radius    = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.3f;
            const float iconWidth = radius * 2.0f;

            // NB: qualified ebs::text() below - NEVER name a local
            // "text" here, it would shadow the free colour function
            // (paid-for MSVC lesson from the v0.1.0 extraction).
            const auto label = getButtonText();
            const bool hasText = label.isNotEmpty();

            float textWidth = 0.0f;
            if (hasText)
            {
                g.setFont (fontComboBox());
                textWidth = juce::GlyphArrangement::getStringWidth (
                                g.getCurrentFont(), label) + 8.0f;
            }

            const float totalWidth = iconWidth + textWidth;
            const float startX     = (bounds.getWidth() - totalWidth) * 0.5f;
            juce::Point<float> center (startX + radius, bounds.getHeight() * 0.5f);

            // Colours based on state.
            juce::Colour glowColor   = resolvedGlow();
            juce::Colour activeColor = isOn ? glowColor
                                            : ebs::text().withAlpha (0.3f);

            if (! isEnabled())
            {
                activeColor = activeColor.withAlpha (0.25f);
                glowColor   = glowColor.withAlpha (0.0f);
            }

            // Engaged glow halo.
            if (isOn && isEnabled())
            {
                juce::ColourGradient glowGrad (glowColor.withAlpha (0.4f),
                                               center.x, center.y,
                                               glowColor.withAlpha (0.0f),
                                               center.x, center.y + radius * 1.5f, true);
                g.setGradientFill (glowGrad);
                g.fillEllipse (center.x - radius * 1.5f, center.y - radius * 1.5f,
                               radius * 3.0f, radius * 3.0f);
            }

            // Power glyph: broken circle + vertical stem.
            g.setColour (activeColor);

            juce::Path powerArc;
            const float gapAngle = juce::MathConstants<float>::pi * 0.25f;
            powerArc.addCentredArc (center.x, center.y, radius, radius, 0.0f,
                                    gapAngle, juce::MathConstants<float>::pi * 2.0f - gapAngle,
                                    true);
            g.strokePath (powerArc, juce::PathStrokeType (2.0f,
                              juce::PathStrokeType::curved,
                              juce::PathStrokeType::rounded));

            juce::Path powerLine;
            powerLine.startNewSubPath (center.x, center.y - radius * 0.2f);
            powerLine.lineTo (center.x, center.y - radius * 1.2f);
            g.strokePath (powerLine, juce::PathStrokeType (2.0f,
                              juce::PathStrokeType::curved,
                              juce::PathStrokeType::rounded));

            if (hasText)
            {
                g.setColour (isOn ? ebs::text().withAlpha (isEnabled() ? 1.0f : 0.25f)
                                  : ebs::text().withAlpha (0.5f * (isEnabled() ? 1.0f : 0.5f)));
                g.drawText (label, startX + iconWidth + 8.0f, 0.0f, textWidth,
                            bounds.getHeight(),
                            juce::Justification::centredLeft, true);
            }
        }

    private:
        juce::Colour resolvedGlow() const
        {
            return widgetColour (*this, glowColourId, juce::Colour (0xFFE8D050));
        }
    };
} // namespace ebs
