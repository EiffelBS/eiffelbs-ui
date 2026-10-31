// ClearableEditor.h
// eiffelbs-ui - a TextEditor with the two affordances every filter and every
// secret field needs:
//
//   * a CLEAR "x" once it holds text (one click, no select-all + Delete), and
//   * optionally a REVEAL eye for a masked (password) field.
//
// WHY: the author asked for a way to empty a text filter "everywhere" - the
// Fetch models picker, the Models searches, the picker's own search - and for
// API keys to behave like password fields with an eye instead of sitting in
// clear text (2026-10-26). One component, one hit test, so the two gestures
// cannot drift apart between panels.
//
// The buttons live INSIDE the field: the right edge is reserved through
// setIndents(), so text never runs under them, and mouseDown consumes the tap
// before TextEditor positions the caret.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "eiffelbs/Theme.h"
#include "eiffelbs/IconButton.h"

namespace ebs
{

class ClearableEditor : public juce::TextEditor,
                        private juce::Timer
{
public:
    ClearableEditor()
    {
        // Reserve the right edge through the BORDER, not setIndents(): that
        // function's second argument is the TOP indent (not a right inset), and
        // passing the icon width there pushed the text/caret to the bottom and
        // collapsed the placeholder band to 4 px (author report 2026-10-26).
        applyInsets();
        setMouseCursor (juce::MouseCursor::IBeamCursor);
        // TextEditor repaints the TEXT area when it changes, not the icon
        // strip: the clear x stayed invisible until something else forced a
        // full repaint (hover, focus change - author report 2026-10-26). A
        // slow watcher flips the strip only when emptiness CHANGES, so typing
        // costs one repaint at most.
        startTimerHz (10);
    }

    ~ClearableEditor() override { stopTimer(); }

    /** Turn the field into a masked one with a reveal eye. The mask is ON by
        default: a key is secret until the reader asks otherwise. */
    void setSecretMode (bool shouldBeSecret)
    {
        secretMode = shouldBeSecret;
        if (secretMode)
            setPasswordCharacter ((juce::juce_wchar) '*');
        applyInsets();
    }

    std::function<void()> onCleared;
    std::function<void()> onRevealToggled;

    // === TEST SEAMS ========================================================
    void clickClearForTest() { clearText(); }
    /** One watcher tick, exposed so a headless gate can prove the x appears
        without waiting on wall-clock time. */
    void pumpStripStateForTest() { timerCallback(); }
    bool clearGlyphShownForTest() const { return stripShowsClear; }
    void clickRevealForTest() { toggleReveal(); }
    bool maskedForTest() const { return getPasswordCharacter() != 0; }
    /** 1 = open eye (masked), 2 = eye-off (visible): what the glyph MEANS. */
    int eyeGlyphForTest() const { return getPasswordCharacter() != 0 ? 1 : 2; }

    void paintOverChildren (juce::Graphics& g) override
    {
        // TextEditor draws its BORDER and its PLACEHOLDER in ITS
        // paintOverChildren (juce_TextEditor.cpp): overriding without calling
        // the base removed both, which is exactly what the author saw in the
        // Fetch models dialog (2026-10-26).
        juce::TextEditor::paintOverChildren (g);

        const auto area = getLocalBounds();
        const int h = area.getHeight();
        int x = area.getRight();
        if (secretMode)
        {
            const auto eye = juce::Rectangle<int> (x - kSlot, 0, kSlot, h)
                                 .withSizeKeepingCentre (16, 16);
            g.setColour (hovered == Part::reveal ? accent()
                                                 : textDim().withAlpha (0.85f));
            // An OPEN eye while the text is masked (the click reveals), a
            // CROSSED eye once it is visible (the click hides again) - the
            // convention the author expects (2026-10-26).
            drawEye (g, eye, getPasswordCharacter() != 0);
            x -= kSlot;
        }
        if (stripShowsClear && clearIsVisible())
        {
            const auto clear = juce::Rectangle<int> (x - kSlot, 0, kSlot, h)
                                   .withSizeKeepingCentre (12, 12);
            g.setColour (hovered == Part::clear ? danger()
                                                : textDim().withAlpha (0.65f));
            drawCross (g, clear);
        }
    }

    void timerCallback() override { stripWatcherTick(); }

protected:
    /** The 10 Hz watcher's body. Subclasses with their own per-tick work
        (VariableTextEditor's suggestion list) override this and call the
        base first - one Timer for the whole field, never two competing ones. */
    virtual void stripWatcherTick()
    {
        const bool hasText = getText().isNotEmpty();
        if (hasText != stripShowsClear)
        {
            stripShowsClear = hasText;
            repaint();      // the x appears/disappears NOW, not on hover
        }
    }

private:

    void mouseMove (const juce::MouseEvent& e) override
    {
        setHovered (partAt (e.getPosition()));
        juce::TextEditor::mouseMove (e);
    }

    void mouseEnter (const juce::MouseEvent& e) override
    {
        juce::TextEditor::mouseEnter (e);
        repaint();   // the clear X appears with the pointer, not on the next tick
    }

    void mouseExit (const juce::MouseEvent& e) override
    {
        setHovered (Part::none);
        juce::TextEditor::mouseExit (e);
        repaint();   // and disappears with it
    }

protected:
    void focusGained (FocusChangeType cause) override
    {
        juce::TextEditor::focusGained (cause);
        repaint();
    }
    void focusLost (FocusChangeType cause) override
    {
        juce::TextEditor::focusLost (cause);
        repaint();
    }

private:
    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto part = partAt (e.getPosition());
        if (part == Part::clear)
        {
            clearText();
            return;                 // the tap never reaches the caret logic
        }
        if (part == Part::reveal)
        {
            toggleReveal();
            return;
        }
        juce::TextEditor::mouseDown (e);
    }

private:
    enum class Part { none, clear, reveal };
    static constexpr int kSlot = 16;

    /** The clear X is an affordance, not decoration: it shows while the field
        has text AND the reader is in it (focus or pointer), so a small numeric
        field is not permanently crowded by a badge. */
    bool clearIsVisible() const
    {
        return stripShowsClear
            && (hasKeyboardFocus (false) || isMouseOverOrDragging());
    }

    int reservedRight() const { return kSlot + (secretMode ? kSlot : 0) + 4; }

    /** Text area = inside a 4 px left pad and the icon strip on the right. */
    void applyInsets()
    {
        setBorder (juce::BorderSize<int> (0, 4, 0, reservedRight()));
    }

    Part partAt (juce::Point<int> p) const
    {
        const auto area = getLocalBounds();
        int x = area.getRight();
        if (secretMode)
        {
            if (juce::Rectangle<int> (x - kSlot, 0, kSlot, area.getHeight())
                    .contains (p))
                return Part::reveal;
            x -= kSlot;
        }
        if (clearIsVisible()
            && juce::Rectangle<int> (x - kSlot, 0, kSlot, area.getHeight())
                   .contains (p))
            return Part::clear;
        return Part::none;
    }

    void setHovered (Part p)
    {
        if (p == hovered)
            return;
        hovered = p;
        setMouseCursor (p == Part::none ? juce::MouseCursor::IBeamCursor
                                        : juce::MouseCursor::PointingHandCursor);
        repaint();
    }

    void clearText()
    {
        if (getText().isEmpty())
            return;
        // setText's second argument is a bool named sendNotificationSync, not a
        // NotificationType enum: passing the enum truncates (C4305) yet works.
        // Pass what the signature actually takes.
        setText ({}, true);
        if (onCleared != nullptr)
            onCleared();
        repaint();
    }

    void toggleReveal()
    {
        if (! secretMode)
            return;
        reveal = ! reveal;
        setPasswordCharacter (reveal ? 0 : (juce::juce_wchar) '*');
        repaint();
        if (onRevealToggled != nullptr)
            onRevealToggled();
    }

    static void drawCross (juce::Graphics& g, juce::Rectangle<int> r)
    {
        auto a = r.toFloat().reduced (1.5f);
        g.drawLine (a.getX(), a.getY(), a.getRight(), a.getBottom(), 1.2f);
        g.drawLine (a.getRight(), a.getY(), a.getX(), a.getBottom(), 1.2f);
    }

    /** The SAME glyph pair the rest of the app uses (Library's hidden
        packages): an OPEN eye while the text is masked (click reveals), the
        design system's EYE-OFF once it is visible (click hides). Hand-drawing
        these produced a "+" instead of the crossed eye the reader expects
        (author report 2026-10-26). */
    static void drawEye (juce::Graphics& g, juce::Rectangle<int> r, bool open)
    {
        g.fillPath (IconButton::glyphPath (open ? IconButton::Shape::eye
                                                : IconButton::Shape::eyeOff,
                                           r.toFloat(), false, false, 14.0f));
    }

    bool secretMode = false;
    bool reveal = false;
    bool stripShowsClear = false;
    Part hovered = Part::none;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClearableEditor)
};

} // namespace ebs
