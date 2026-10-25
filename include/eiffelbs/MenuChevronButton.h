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
#include "eiffelbs/IconButton.h"   // the shared glyph paths (an optional menu icon)

namespace ebs
{

    class MenuChevronButton : public juce::Button
    {
    public:
        MenuChevronButton() : juce::Button ("MenuChevron")
        {
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }

        /** The optional GLYPH the menu carries, drawn at the left of the label.
         *  Built by the SAME `IconButton::glyphPath`, so a shape means exactly
         *  what it means on an IconButton, and measured into
         *  `widthNeededFor (text, withIcon)`. Absent by default: a menu with no
         *  glyph is the button this class shipped, pixel for pixel. */
        void setShape (IconButton::Shape s)
        {
            shape = s;
            hasShape = true;
            repaint();
        }

        /** A SECOND, dim text drawn right-aligned before the chevron. The
         *  model menus show "model ... effort" as two facts of different weight:
         *  the model name may ellipsize, the effort word must stay readable.
         *  Empty by default - a button without trailing text is the one this
         *  class shipped, pixel for pixel. */
        void setTrailingText (juce::String text)
        {
            trailingText = std::move (text);
            repaint();
        }
        const juce::String& getTrailingText() const noexcept { return trailingText; }

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
            const auto ink = ! isEnabled() ? ebs::textDim().withAlpha (0.35f)
                                           : ebs::text();
            auto textArea = b.withTrimmedLeft (pad()).withTrimmedRight (chevronSlot());
            if (trailingText.isNotEmpty())
            {
                auto trail = textArea.removeFromRight (
                    (int) std::ceil (ebs::fontHint().getStringWidth (trailingText))
                    + 10);
                g.setColour (isEnabled() ? ebs::textDim()
                                         : ebs::textDim().withAlpha (0.35f));
                g.setFont (ebs::fontHint());
                g.drawText (trailingText, trail,
                            juce::Justification::centredRight, false);
            }
            if (hasShape)
            {
                // The glyph owns the left of the text area, at the height of
                // the hint font: one box, one centre, so the label and the
                // shape share a baseline whatever the button's height.
                const auto box = textArea.removeFromLeft (iconSlot()).toFloat()
                                    .withSizeKeepingCentre (iconSide(), iconSide());
                g.setColour (ink);
                g.fillPath (IconButton::glyphPath (shape, box, false, false,
                                                   iconSide()));
            }
            g.setColour (ink);
            g.setFont (ebs::fontHint());
            g.drawText (getButtonText(), textArea,
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
        /** The glyph's slot and its side, when the button carries one. */
        static constexpr int iconSlot()    { return 22; }
        static constexpr int iconSide()    { return 14; }

        /** The width this label needs AT THIS FONT. A layout that sizes the
         *  button to a guess either cuts the text mid-word or leaves a dead
         *  band the pointer crosses before the click counts, so the width is
         *  measured, never assumed - the same rule IconTextButton follows for
         *  its icon-plus-label row. `withIcon` adds the glyph's slot: a caller
         *  that measures only the text of a button that HAS one is the layout
         *  that cuts it. */
        static int widthNeededFor (const juce::String& text,
                                   bool withIcon = false,
                                   const juce::String& trailing = {})
        {
            return pad()
                 + (withIcon ? iconSlot() : 0)
                 + (int) std::ceil (ebs::fontHint().getStringWidth (text))
                 + (trailing.isNotEmpty()
                        ? (int) std::ceil (ebs::fontHint().getStringWidth (trailing))
                          + 10
                        : 0)
                 + chevronSlot() + pad();
        }

        int widthForLabel() const
        {
            return widthNeededFor (getButtonText(), hasShape, trailingText);
        }

    private:
        IconButton::Shape shape = IconButton::Shape::play;
        bool hasShape = false;
        juce::String trailingText;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MenuChevronButton)
    };

} // namespace ebs