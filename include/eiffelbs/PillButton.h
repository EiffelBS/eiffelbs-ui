// PillButton.h
// eiffelbs-ui - the compact "icon + label in a rounded pill" action button.
//
// WHY THIS EXISTS: the Models lists drew their row actions as small pills (an
// icon and a word inside a tinted rounded shape). The author preferred that
// look over the flat IconTextButton used across the Settings window and asked
// for a reusable component with a REAL hover state (2026-10-26). This is that
// component: same ebs palette, same shape vocabulary as IconButton, sized to
// its label, with hover and pressed feedback.
//
// It is NOT a replacement for IconTextButton everywhere: IconTextButton is the
// established Settings control. PillButton is the alternative for dense,
// row-level or toolbar actions where the pill's tighter footprint reads better.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "eiffelbs/Theme.h"
#include "eiffelbs/Fonts.h"
#include "eiffelbs/IconButton.h"
#include "eiffelbs/PillPainter.h"

#include <cmath>

namespace ebs
{

class PillButton : public juce::Button
{
public:
    enum class Tone
    {
        neutral,   // Remove / Verify / secondary actions
        primary,   // Install / Update / the main action
        disabledish  // a gated action: readable, visibly not actionable
    };

    explicit PillButton (IconButton::Shape s = IconButton::Shape::play,
                         juce::String label = {})
        : juce::Button (label), shape (s), label_ (std::move (label))
    {
        // juce::Button's String argument is the component NAME: the label the
        // painter reads is the button TEXT.
        setButtonText (label_);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void setShape (IconButton::Shape s) { shape = s; repaint(); }
    void setFont (juce::Font f) { font = f; resized(); repaint(); }
    const juce::Font& getFont() const noexcept { return font; }
    void setTone (Tone t) { tone = t; repaint(); }

    /** The width this label needs (icon slot + text + padding). The label is
        juce::Button::getButtonText(), so setButtonText() keeps working as on
        every other button. */
    int widthForLabel() const
    {
        return 8 + 14 + 4
             + (int) std::ceil (juce::GlyphArrangement::getStringWidth (font,
                                                                        getButtonText()))
             + 8;
    }

    // Test seam: draw the SAME paint path JUCE calls, with an explicit hover
    // state (an offscreen smoke test cannot move a real pointer).
    void paintForTest (juce::Graphics& g, bool over, bool down)
    {
        paintButtonImpl (g, over, down);
    }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        paintButtonImpl (g, over, down);
    }

    void paintButtonImpl (juce::Graphics& g, bool over, bool down)
    {
        paintPillButton (g, getLocalBounds(), shape, getButtonText(), font,
                         iconPx, painterTone(), over, down, isEnabled());
    }

    PillTone painterTone() const noexcept
    {
        switch (tone)
        {
            case Tone::primary:     return PillTone::primary;
            case Tone::disabledish: return PillTone::gated;
            case Tone::neutral:     break;
        }
        return PillTone::neutral;
    }

    float iconPx = 12.0f;


    IconButton::Shape shape = IconButton::Shape::play;
    juce::String label_;
    juce::Font font = fontHint();
    Tone tone = Tone::neutral;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PillButton)
};

} // namespace ebs
