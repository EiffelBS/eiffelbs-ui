// MenuChevronButton.h
// eiffelbs-ui - shared menu button (EiffelBS design system).
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// A button that OPENS something and says so: text left, a light V chevron
// right. A plain TextButton centres its label and carries no chevron, so a
// menu rendered as one reads as a label - the reader presses it, nothing
// appears to be a menu, and the control looks broken rather than closed.
//
// Transparent at rest, with a quiet accent hover: grey hover reads as
// disabled, and an affordance that looks dead under the pointer teaches the
// user that it is dead. The V is two thin strokes, never a filled triangle.
//
// It lives here rather than in the chat panel that first needed it because a
// second panel needs it for the same job (the TTS text area offers the vocal
// tags the selected model reads), and a control two panels each carry a copy
// of is two controls that will drift.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

#include "eiffelbs/Theme.h"
#include "eiffelbs/Fonts.h"

namespace ebs
{

    class MenuChevronButton : public juce::Button
    {
    public:
        MenuChevronButton() : juce::Button ("MenuChevron")
        {
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }

        void paintButton (juce::Graphics& g, bool over, bool down) override
        {
            const auto b = getLocalBounds().toFloat();
            if ((over || down) && isEnabled())
            {
                // Quiet accent hover, the shared language: grey reads as
                // disabled, and an affordance that looks dead under the
                // pointer teaches the user that it is dead.
                g.setColour (ebs::accent().withAlpha (down ? 0.28f : 0.16f));
                g.fillRoundedRectangle (b, 5.0f);
            }
            g.setColour (! isEnabled() ? ebs::textDim().withAlpha (0.35f)
                                       : ebs::text());
            g.setFont (ebs::fontHint());
            g.drawText (getButtonText(),
                        b.withTrimmedLeft (pad()).withTrimmedRight (chevronSlot()),
                        juce::Justification::centredLeft, true);
            const float cx = b.getRight() - pad() - 5.0f;
            const float cy = b.getCentreY();
            juce::Path v;
            v.startNewSubPath (cx - 4.0f, cy - 2.5f);
            v.lineTo (cx, cy + 2.5f);
            v.lineTo (cx + 4.0f, cy - 2.5f);
            g.setColour (! isEnabled() ? ebs::textDim().withAlpha (0.35f)
                                       : ebs::textDim());
            g.strokePath (v, juce::PathStrokeType (1.4f,
                                                   juce::PathStrokeType::mitered,
                                                   juce::PathStrokeType::rounded));
        }

        /** Left padding plus the chevron slot plus the right padding: the two
         *  numbers every layout needs, named so a panel measures against the
         *  button's own chrome instead of copying the constants. */
        static constexpr int pad()        { return 8; }
        static constexpr int chevronSlot() { return 18; }

        /** The width this label needs AT THIS FONT. A layout that sizes the
         *  button to a guess either cuts the text mid-word or leaves a dead
         *  band the pointer crosses before the click counts, so the width is
         *  measured, never assumed - the same rule IconTextButton follows for
         *  its icon-plus-label row. */
        static int widthNeededFor (const juce::String& text)
        {
            return pad()
                 + (int) std::ceil (ebs::fontHint().getStringWidth (text))
                 + chevronSlot() + pad();
        }

        int widthForLabel() const { return widthNeededFor (getButtonText()); }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MenuChevronButton)
    };

} // namespace ebs