// PillTabBar.h
// eiffelbs-ui - the callback-driven pill tab strip (icon + label per tab).
//
// WHY: ebs::PillTabSwitch paints the SAME pills but is bound to a
// juce::TabbedComponent, and a panel that owns its own page switching (the
// Assistant editor: three files, one sticky Save bar) cannot host a
// TabbedComponent without restructuring. This bar is the same look with no
// host component: the caller owns the active index and the pages, the bar only
// paints, measures and reports taps (author request 2026-10-26).
//
// The geometry is the one the switch already established: a rounded bgPanel
// track, one uniform slot per tab, the active slot as an accent-tinted pill.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <optional>
#include <vector>

#include "eiffelbs/Theme.h"
#include "eiffelbs/Fonts.h"
#include "eiffelbs/IconButton.h"

namespace ebs
{

class PillTabBar : public juce::Component,
                   public juce::SettableTooltipClient
{
public:
    PillTabBar()
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setWantsKeyboardFocus (false);
    }

    /** Display labels, in tab order. */
    void setTabs (const juce::StringArray& labels)
    {
        tabLabels = labels;
        active = juce::jlimit (0, juce::jmax (0, labels.size() - 1), active);
        repaint();
    }

    /** Optional glyph per tab (nullopt = text only). */
    std::function<std::optional<IconButton::Shape> (int)> onTabIcon;

    /** Fired when the reader taps a DIFFERENT tab. */
    std::function<void (int)> onTabSelected;

    int activeIndex() const noexcept { return active; }
    int tabCount() const noexcept { return tabLabels.size(); }

    /** Programmatic selection (the host's guard may refuse: it answers by
        calling this with the index it really shows). */
    void setActiveIndex (int index, bool notify = false)
    {
        const int next = juce::jlimit (0, juce::jmax (0, tabCount() - 1), index);
        if (next == active)
            return;
        active = next;
        repaint();
        if (notify && onTabSelected != nullptr)
            onTabSelected (active);
    }

    /** Width the strip asks for: every slot measured at the same font. */
    int preferredWidth() const
    {
        return juce::roundToInt (slotWidth()) * juce::jmax (1, tabCount())
             + 2 * kPad;
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat().reduced (1.0f);
        const float radius = area.getHeight() * 0.5f;
        g.setColour (bgPanel());
        g.fillRoundedRectangle (area, radius);
        g.setColour (text().withAlpha (0.18f));
        g.drawRoundedRectangle (area, radius, 1.0f);

        const float slot = slotWidth();
        for (int i = 0; i < tabCount(); ++i)
        {
            const auto r = juce::Rectangle<float> (
                (float) kPad + slot * (float) i, 2.0f, slot,
                (float) getHeight() - 4.0f);
            const bool on = i == active;
            if (on)
            {
                g.setColour (accent().withAlpha (0.18f));
                g.fillRoundedRectangle (r, radius - 2.0f);
                g.setColour (accent());
                g.drawRoundedRectangle (r, radius - 2.0f, 1.0f);
            }

            auto textArea = r.reduced (8.0f, 0.0f);
            const auto glyph = onTabIcon != nullptr ? onTabIcon (i) : std::nullopt;
            if (glyph.has_value())
            {
                const auto box = textArea.removeFromLeft (16.0f)
                                         .withSizeKeepingCentre (12.0f, 12.0f);
                g.setColour (on ? accent() : textDim());
                g.fillPath (IconButton::glyphPath (*glyph, box, false, false,
                                                   12.0f));
                textArea.removeFromLeft (4.0f);
            }
            g.setColour (on ? text() : textDim());
            g.setFont (fontHint());
            g.drawFittedText (tabLabels[i], textArea.toNearestInt(),
                              juce::Justification::centredLeft, 1);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int i = tabAt (e.x);
        if (i >= 0 && i != active)
        {
            active = i;
            repaint();
            if (onTabSelected != nullptr)
                onTabSelected (active);
        }
    }

    // === TEST SEAMS ========================================================
    int tabAtForTest (int x) const { return tabAt (x); }
    void clickTabForTest (int index)
    {
        if (index >= 0 && index < tabCount() && index != active)
        {
            active = index;
            repaint();
            if (onTabSelected != nullptr)
                onTabSelected (active);
        }
    }

private:
    static constexpr int kPad = 4;

    float slotWidth() const
    {
        int widest = 0;
        for (const auto& label : tabLabels)
        {
            const int icons = 16;
            widest = juce::jmax (widest,
                                 (int) juce::GlyphArrangement::getStringWidth (
                                     fontHint(), label) + icons + 16);
        }
        return (float) widest;
    }

    int tabAt (int x) const
    {
        const float slot = slotWidth();
        if (slot <= 0.0f)
            return -1;
        const int i = (int) std::floor ((float) (x - kPad) / slot);
        return juce::jlimit (0, juce::jmax (0, tabCount() - 1), i);
    }

    juce::StringArray tabLabels;
    int active = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PillTabBar)
};

} // namespace ebs
