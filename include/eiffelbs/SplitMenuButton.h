// SplitMenuButton.h
// eiffelbs-ui - shared split button with a primary action and an attached menu.
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// A SPLIT control: the left part executes the current action, the right part
// (a light V) opens a menu that CHOOSES that action. It exists because a menu
// button that only selects is slow for the common action, while a plain button
// has no way to offer the alternatives. The menu never executes: it changes the
// action, and the primary part runs it - one affordance, one destructive click.
//
// When `setMenuOnly(true)` is in force (or no primary text/callback is set), the
// whole face opens the menu, exactly like MenuChevronButton. That is the
// fallback a host uses when no default action exists: the control never shows a
// dead primary half.
//
// Visual language follows MenuChevronButton: transparent at rest, a quiet
// accent hover on the part under the pointer, a text hint font, and a thin
// stroked V - never a filled triangle. The divider only appears in split mode.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <functional>

#include "eiffelbs/Theme.h"
#include "eiffelbs/Fonts.h"
#include "eiffelbs/MenuChevronButton.h"

namespace ebs
{

class SplitMenuButton : public juce::Component,
                        public juce::SettableTooltipClient
{
public:
    SplitMenuButton()
    {
        setWantsKeyboardFocus (true);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    /** The primary face's label. Empty makes the control menu-only, so a host
        cannot accidentally paint a split with nothing to run. */
    void setPrimaryText (const juce::String& text)
    {
        primaryText = text;
        repaint();
    }
    const juce::String& getPrimaryText() const noexcept { return primaryText; }

    /** TRUE = the whole face opens the menu and no primary action exists.
        FALSE = split: left runs, right selects. */
    void setMenuOnly (bool on)
    {
        if (menuOnly == on)
            return;
        menuOnly = on;
        hovered = pressed = Part::none;
        repaint();
    }
    bool isMenuOnly() const noexcept
    {
        return menuOnly || primaryText.isEmpty() || onPrimary == nullptr;
    }

    /** The primary action. Null keeps the control menu-only. */
    std::function<void()> onPrimary;
    /** The menu opener for the chevron (or the whole face in menu-only mode). */
    std::function<void()> onMenu;

    /** Measured width, including this control's own chrome. A host that guesses
        either cuts the label or leaves a dead band before the click counts. */
    int widthForContent() const
    {
        if (isMenuOnly())
            return MenuChevronButton::widthNeededFor (primaryText.isEmpty()
                                                          ? juce::String ("Apply")
                                                          : primaryText);
        return pad() + textWidth() + gap() + menuWidth() + pad();
    }

    /** Gate seam: the two hit zones, in this component's coordinates. */
    juce::Rectangle<int> primaryAreaForTest() const { return primaryArea(); }
    juce::Rectangle<int> menuAreaForTest() const { return menuArea(); }
    /** Drive the SAME dispatch a real click runs, without a mouse event. */
    void clickPrimaryForTest()
    {
        if (! isEnabled())
            return;
        trigger (Part::primary);
    }
    void clickMenuForTest()
    {
        if (! isEnabled())
            return;
        trigger (Part::menu);
    }

    // === Component =========================================================
    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        const bool on = isEnabled();
        const auto ink = on ? ebs::text() : ebs::textDim().withAlpha (0.35f);
        const auto quiet = on ? ebs::textDim() : ebs::textDim().withAlpha (0.35f);

        // Quiet accent hover on the part under the pointer. Grey reads as
        // disabled; a dead-looking affordance teaches the user it is dead.
        const auto paintPart = [&] (Part part)
        {
            if (! on || part == Part::none || hovered != part)
                return;
            g.setColour (ebs::accent().withAlpha (pressed == part ? 0.28f : 0.16f));
            g.fillRoundedRectangle (part == Part::primary
                                        ? primaryArea().toFloat()
                                        : menuArea().toFloat(),
                                    5.0f);
        };
        paintPart (Part::primary);
        paintPart (Part::menu);

        if (! isMenuOnly())
        {
            // The divider is what makes one control read as two: the primary
            // face and the selector, without two separate pill outlines.
            const float x = (float) menuArea().getX();
            g.setColour (quiet.withAlpha (on ? 0.55f : 0.20f));
            g.drawVerticalLine (juce::roundToInt (x),
                                b.getY() + 4.0f, b.getBottom() - 4.0f);
        }

        auto textArea = primaryArea().reduced (pad(), 0);
        if (isMenuOnly())
            textArea.removeFromRight (MenuChevronButton::chevronSlot());
        else
            textArea.removeFromRight (gap());
        g.setColour (ink);
        g.setFont (ebs::fontHint());
        g.drawText (isMenuOnly() && primaryText.isEmpty()
                        ? juce::String ("Apply")
                        : primaryText,
                    textArea, juce::Justification::centredLeft, true);

        // The V occupies its own slot, exactly like MenuChevronButton's.
        const auto menu = menuArea();
        const float cx = (float) menu.getRight() - pad() - 5.0f;
        const float cy = (float) menu.getCentreY();
        juce::Path v;
        v.startNewSubPath (cx - 4.0f, cy - 2.5f);
        v.lineTo (cx, cy + 2.5f);
        v.lineTo (cx + 4.0f, cy - 2.5f);
        g.setColour (quiet);
        g.strokePath (v, juce::PathStrokeType (1.4f,
                                               juce::PathStrokeType::mitered,
                                               juce::PathStrokeType::rounded));

        if (hasKeyboardFocus (false) && on)
        {
            g.setColour (ebs::accent().withAlpha (0.8f));
            g.drawRoundedRectangle (b.reduced (0.5f), 5.0f, 1.0f);
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const auto part = partAt (e.getPosition());
        if (part != hovered)
        {
            hovered = part;
            repaint();
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        if (hovered != Part::none)
        {
            hovered = Part::none;
            repaint();
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! isEnabled())
            return;
        grabKeyboardFocus();
        pressed = partAt (e.getPosition());
        repaint();
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        const auto wasPressed = pressed;
        pressed = Part::none;
        repaint();
        if (! isEnabled())
            return;
        const auto part = partAt (e.getPosition());
        if (part != Part::none && part == wasPressed)
            trigger (part);
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (! isEnabled())
            return false;
        if (key == juce::KeyPress::returnKey
            || key == juce::KeyPress::spaceKey)
        {
            trigger (isMenuOnly() ? Part::menu : Part::primary);
            return true;
        }
        if (key == juce::KeyPress::downKey
            || (key.getKeyCode() == juce::KeyPress::downKey
                && key.getModifiers().isAltDown()))
        {
            trigger (Part::menu);
            return true;
        }
        return false;
    }

    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override    { repaint(); }
    void enablementChanged() override
    {
        hovered = pressed = Part::none;
        repaint();
    }

private:
    enum class Part { none, primary, menu };

    static constexpr int pad()       { return 8; }
    static constexpr int gap()       { return 4; }
    static constexpr int menuWidth() { return 28; }

    int textWidth() const
    {
        return (int) std::ceil (ebs::fontHint().getStringWidth (primaryText));
    }

    juce::Rectangle<int> menuArea() const
    {
        auto b = getLocalBounds();
        if (isMenuOnly())
            return b;
        return b.removeFromRight (menuWidth());
    }

    juce::Rectangle<int> primaryArea() const
    {
        auto b = getLocalBounds();
        if (isMenuOnly())
            return b;
        b.removeFromRight (menuWidth());
        return b;
    }

    Part partAt (juce::Point<int> p) const
    {
        if (! getLocalBounds().contains (p))
            return Part::none;
        if (isMenuOnly())
            return Part::menu;
        if (menuArea().contains (p))
            return Part::menu;
        return primaryArea().contains (p) ? Part::primary : Part::none;
    }

    void trigger (Part part)
    {
        if (part == Part::menu)
        {
            if (onMenu != nullptr)
                onMenu();
            return;
        }
        if (part == Part::primary && ! isMenuOnly() && onPrimary != nullptr)
            onPrimary();
    }

    juce::String primaryText;
    bool menuOnly = true;
    Part hovered = Part::none;
    Part pressed = Part::none;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SplitMenuButton)
};

} // namespace ebs
