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

namespace ebs
{

class ClearableEditor : public juce::TextEditor
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
    }

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
    void clickRevealForTest() { toggleReveal(); }
    bool maskedForTest() const { return getPasswordCharacter() != 0; }

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
        if (getText().isNotEmpty())
        {
            const auto clear = juce::Rectangle<int> (x - kSlot, 0, kSlot, h)
                                   .withSizeKeepingCentre (16, 16);
            g.setColour (hovered == Part::clear ? danger()
                                                : textDim().withAlpha (0.85f));
            drawCross (g, clear);
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        setHovered (partAt (e.getPosition()));
        juce::TextEditor::mouseMove (e);
    }

    void mouseExit (const juce::MouseEvent& e) override
    {
        setHovered (Part::none);
        juce::TextEditor::mouseExit (e);
    }

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
    static constexpr int kSlot = 20;

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
        if (getText().isNotEmpty()
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
        auto a = r.toFloat().reduced (2.0f);
        g.drawLine (a.getX(), a.getY(), a.getRight(), a.getBottom(), 1.6f);
        g.drawLine (a.getRight(), a.getY(), a.getX(), a.getBottom(), 1.6f);
    }

    static void drawEye (juce::Graphics& g, juce::Rectangle<int> r, bool open)
    {
        const auto a = r.toFloat().reduced (1.0f);
        if (! open)
        {
            // Closed eye: lid line + a small slash.
            g.drawLine (a.getX(), a.getCentreY(), a.getRight(), a.getCentreY(),
                        1.4f);
            g.drawLine (a.getCentreX(), a.getY() + 2.0f,
                        a.getCentreX(), a.getBottom() - 2.0f, 1.2f);
            return;
        }
        juce::Path lid;
        lid.startNewSubPath (a.getX(), a.getCentreY());
        lid.quadraticTo (a.getCentreX(), a.getY(),
                         a.getRight(), a.getCentreY());
        lid.quadraticTo (a.getCentreX(), a.getBottom(),
                         a.getX(), a.getCentreY());
        g.strokePath (lid, juce::PathStrokeType (1.2f));
        g.fillEllipse (a.getCentreX() - 1.6f, a.getCentreY() - 1.6f, 3.2f,
                       3.2f);
    }

    bool secretMode = false;
    bool reveal = false;
    Part hovered = Part::none;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClearableEditor)
};

} // namespace ebs
