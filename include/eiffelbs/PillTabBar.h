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
#include <cmath>
#include <functional>
#include <optional>
#include <vector>

#include "eiffelbs/Theme.h"
#include "eiffelbs/Fonts.h"
#include "eiffelbs/IconButton.h"

namespace ebs
{

class PillTabBar : public juce::Component,
                   public juce::SettableTooltipClient,
                   private juce::Timer
{
public:
    PillTabBar()
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setWantsKeyboardFocus (false);
    }

    ~PillTabBar() override { stopTimer(); }

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
        startSlide();
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

        // The ACTIVE pill slides between slots (eased): the eye follows ONE
        // object moving, instead of a pill blinking out and another appearing
        // (author request 2026-10-26).
        if (tabCount() > 0)
        {
            const auto pill = juce::Rectangle<float> (
                (float) kPad + slot * pillPos, 2.0f, slot,
                (float) getHeight() - 4.0f);
            g.setColour (accent().withAlpha (0.18f));
            g.fillRoundedRectangle (pill, radius - 2.0f);
            g.setColour (accent());
            g.drawRoundedRectangle (pill, radius - 2.0f, 1.0f);
        }

        for (int i = 0; i < tabCount(); ++i)
        {
            const auto r = juce::Rectangle<float> (
                (float) kPad + slot * (float) i, 2.0f, slot,
                (float) getHeight() - 4.0f);
            const bool on = i == active;

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
            startSlide();
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
            startSlide();
            repaint();
            if (onTabSelected != nullptr)
                onTabSelected (active);
        }
    }

    /** Advances the slide by `dt` seconds; the timer calls it, the gate drives
        it so a headless test never depends on wall-clock timers. */
    void stepAnimationForTest (double dt) { advanceAnimation (dt); }
    float pillPositionForTest() const { return pillPos; }

private:
    static constexpr int kPad = 4;

    void startSlide()
    {
        targetPillPos = (float) juce::jmax (0, active);
        if (std::abs (targetPillPos - pillPos) < 0.01f)
        {
            pillPos = targetPillPos;
            stopTimer();
            return;
        }
        if (! isTimerRunning())
            startTimerHz (60);
    }

    void advanceAnimation (double dtSeconds)
    {
        const float target = targetPillPos;
        const float delta = target - pillPos;
        if (std::abs (delta) < 0.005f)
        {
            pillPos = target;
            stopTimer();
            repaint();
            return;
        }
        // Exponential ease: fast at the start, settles without overshoot.
        const float k = juce::jlimit (0.0f, 1.0f,
                                      (float) (1.0 - std::exp (-dtSeconds * 18.0)));
        pillPos += delta * k;
        repaint();
    }

    void timerCallback() override
    {
        advanceAnimation (1.0 / 60.0);
    }

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
    float pillPos = 0.0f;
    float targetPillPos = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PillTabBar)
};

} // namespace ebs
