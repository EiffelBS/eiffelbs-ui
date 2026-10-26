// VariableTextEditor.h
// eiffelbs-ui - a TextEditor that helps write {{ns.name}} variables.
//
// WHY: the Templates editor declares its variables as chips, but the Prompt and
// System boxes offered no way to INSERT one: the reader had to remember the
// exact syntax and type it by hand (author report 2026-10-26). This editor
// shows the provided variables as a filterable list while the reader types
// "{{", and on Ctrl+Space on demand; Enter/Tab/click inserts "{{name}}" at the
// caret, Escape closes.
//
// The popup is an OVERLAY child of the editor, not a CallOutBox: the keyboard
// focus must never leave the text (a modal-ish popup steals the caret, and a
// second click to come back loses the typed prefix). It never takes focus.
//
// The typed prefix is watched by a small TIMER rather than a textChanged()
// override: JUCE's TextEditor::textChanged() is protected and not virtual, and
// its onTextChange std::function belongs to the HOST (the settings editor uses
// it for markDirty). Polling the caret while focused keeps the suggestion list
// in sync without stealing the host's hook.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>

#include "eiffelbs/ClearableEditor.h"
#include "eiffelbs/Theme.h"
#include "eiffelbs/Fonts.h"

namespace ebs
{

class VariableTextEditor : public ClearableEditor,
                          private juce::Timer,
                          public juce::KeyListener,
                          public juce::DragAndDropTarget
{
public:
    struct Variable
    {
        juce::String id;
        juce::String description;
    };

    VariableTextEditor()
    {
        popup.owner = this;
        popup.onPick = [this] (int index) { pick (index); };
        addChildComponent (popup);

        // Ctrl+Space opens the full list even before anything is typed.
        addKeyListener (this);
    }

    ~VariableTextEditor() override { stopTimer(); }

    /** The variables this build provides - the same list the chips offer. */
    void setVariables (std::vector<Variable> provided)
    {
        variables = std::move (provided);
        hideSuggestions();
    }

    std::function<void (const juce::String&)> onVariableInserted;

    // === TEST SEAMS ========================================================
    void openSuggestionsForTest() { showSuggestions (currentPrefix()); }
    bool suggestionsVisibleForTest() const { return popup.isVisible(); }
    int suggestionsCountForTest() const { return (int) matches.size(); }
    juce::String suggestionAtForTest (int index) const
    {
        return index >= 0 && index < (int) matches.size()
                   ? matches[(size_t) index] : juce::String();
    }
    void chooseSuggestionForTest (int index) { pick (index); }
    void hideSuggestionsForTest() { hideSuggestions(); }

    // === DragAndDropTarget: a chip dropped from a TokenField ==============
    bool isInterestedInDragSource (const SourceDetails& d) override
    {
        return d.description.toString().startsWith ("ebs.variable.");
    }

    void itemDragEnter (const SourceDetails& d) override
    {
        dragOver = true;
        moveCaretToDropPoint (d.localPosition);
        repaint();
    }

    void itemDragMove (const SourceDetails& d) override
    {
        // The DROP POINT is where the reader is pointing, exactly like a text
        // drop: the caret follows the drag, so nobody has to place it first
        // (author request 2026-10-26).
        moveCaretToDropPoint (d.localPosition);
    }

    void itemDragExit (const SourceDetails&) override
    {
        dragOver = false;
        repaint();
    }

    void itemDropped (const SourceDetails& d) override
    {
        dragOver = false;
        repaint();
        insertVariable (d.description.toString().fromFirstOccurrenceOf (
            "ebs.variable.", false, false));
    }

    /** Insert `{{id}}` at the caret (a drop and the suggestion list share this
        path, so both behave the same way). */
    /** Move the caret to the character under a drop point. */
    void moveCaretToDropPoint (juce::Point<int> localPosition)
    {
        dropPoint = localPosition;
        const int index = getTextIndexAt (localPosition);
        if (index >= 0)
            setCaretPosition (juce::jlimit (0, getText().length(), index));
        repaint();   // the drop caret/liseré are OUR painting, not JUCE's
    }

    /** Drop seam used by the gate: aims the caret then inserts. */
    void setDragPreviewForTest (bool on, juce::Point<int> localPosition)
    {
        dragOver = on;
        moveCaretToDropPoint (localPosition);
    }

    void dropVariableAtForTest (const juce::String& id,
                                juce::Point<int> localPosition)
    {
        moveCaretToDropPoint (localPosition);
        insertVariable (id);
    }

    void insertVariable (const juce::String& id)
    {
        if (id.isEmpty())
            return;
        const auto text = getText();
        const int caret = getCaretPosition();
        const auto before = text.substring (0, caret);
        const int open = before.lastIndexOf ("{{");
        if (open >= 0 && ! before.substring (open).containsChar ('}'))
        {
            auto replaced = text.substring (0, open) + "{{" + id + "}}"
                          + text.substring (caret);
            setText (replaced, juce::dontSendNotification);
            setCaretPosition (juce::jlimit (0, replaced.length(),
                                            open + 4 + id.length()));
        }
        else
        {
            insertTextAtCaret ("{{" + id + "}}");
        }
        grabKeyboardFocus();
        if (onVariableInserted != nullptr)
            onVariableInserted (id);
    }

    void dropVariableForTest (const juce::String& id) { insertVariable (id); }

    void paintOverChildren (juce::Graphics& g) override
    {
        ClearableEditor::paintOverChildren (g);
        if (! dragOver)
            return;

        // A 2 px accent border plus OUR OWN drop caret: the real caret is
        // only painted while the editor has keyboard focus, and during a drag
        // the focus sits on the source chip - which is exactly why the author
        // saw the drop land correctly with no visual cue (2026-10-26).
        g.setColour (accent());
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.5f),
                                6.0f, 2.0f);
        auto caret = getCaretRectangleForCharIndex (getCaretPosition());
        if (caret.isEmpty())
            caret = juce::Rectangle<int> (dropPoint.x, dropPoint.y - 9, 2, 18);
        g.fillRect (caret.withWidth (2).expanded (1, 0));
    }

    // === KeyListener ======================================================
    bool keyPressed (const juce::KeyPress& key, juce::Component*) override
    {
        if (popup.isVisible())
        {
            if (key == juce::KeyPress::upKey)
            {
                moveHighlight (-1);
                return true;
            }
            if (key == juce::KeyPress::downKey)
            {
                moveHighlight (1);
                return true;
            }
            if (key == juce::KeyPress::returnKey
                || key == juce::KeyPress::tabKey)
            {
                if (! matches.isEmpty())
                    pick (juce::jlimit (0, (int) matches.size() - 1, highlight));
                return true;
            }
            if (key == juce::KeyPress::escapeKey)
            {
                hideSuggestions();
                return true;
            }
        }
        if (key.getKeyCode() == juce::KeyPress::spaceKey
            && key.getModifiers().isCtrlDown())
        {
            showSuggestions (currentPrefix());
            return true;
        }
        return false;
    }

    void focusGained (FocusChangeType cause) override
    {
        ClearableEditor::focusGained (cause);
        lastText = getText();
        lastCaret = getCaretPosition();
        startTimerHz (8);
    }

    void focusLost (FocusChangeType cause) override
    {
        ClearableEditor::focusLost (cause);
        stopTimer();
        hideSuggestions();
    }

    void resized() override
    {
        ClearableEditor::resized();
        layoutPopup();
    }

private:
    struct Popup : juce::Component
    {
        std::function<void (int)> onPick;
        int hoverIndex = -1;

        void paint (juce::Graphics& g) override
        {
            if (owner == nullptr)
                return;
            const auto area = getLocalBounds().toFloat();
            g.setColour (bgPanel());
            g.fillRoundedRectangle (area, 6.0f);
            g.setColour (panelBorder());
            g.drawRoundedRectangle (area, 6.0f, 1.0f);

            for (int i = 0; i < (int) owner->matches.size(); ++i)
            {
                const auto row = rowAt (i);
                if (i == owner->highlight || i == hoverIndex)
                {
                    g.setColour (accent().withAlpha (i == owner->highlight
                                                         ? 0.20f : 0.12f));
                    g.fillRoundedRectangle (row.toFloat(), 4.0f);
                }
                auto rowText = row.reduced (8, 0);
                const int descW = juce::jmin (130, rowText.getWidth() / 2);
                g.setColour (i == owner->highlight ? text() : textDim());
                g.setFont (fontHint());
                g.drawText (owner->matches[(size_t) i],
                            rowText.removeFromLeft (juce::jmax (0,
                                rowText.getWidth() - descW)),
                            juce::Justification::centredLeft, true);
                if (i < (int) owner->descriptions.size()
                    && owner->descriptions[(size_t) i].isNotEmpty())
                {
                    g.setColour (textDim().withAlpha (0.75f));
                    g.setFont (fontHint());
                    g.drawText (owner->descriptions[(size_t) i], rowText,
                                juce::Justification::centredRight, true);
                }
            }
        }

        static constexpr int rowH = 22;

        juce::Rectangle<int> rowAt (int index) const
        {
            return { 1, 2 + index * rowH, getWidth() - 2, rowH };
        }

        void mouseMove (const juce::MouseEvent& e) override
        {
            const int i = (e.y - 2) / rowH;
            if (i != hoverIndex)
            {
                hoverIndex = i;
                repaint();
            }
        }

        void mouseExit (const juce::MouseEvent&) override
        {
            hoverIndex = -1;
            repaint();
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            const int i = (e.y - 2) / rowH;
            if (i >= 0 && i < (int) owner->matches.size())
            {
                const auto pickIndex = i;
                juce::Component::SafePointer<Popup> safe (this);
                juce::MessageManager::callAsync ([safe, pickIndex]() mutable
                {
                    if (safe != nullptr && safe->onPick != nullptr)
                        safe->onPick (pickIndex);
                });
            }
        }

        VariableTextEditor* owner = nullptr;
    };

    static constexpr int kMaxRows = 6;
    static constexpr int kPopupW = 300;

    void timerCallback() override
    {
        if (! hasKeyboardFocus (true))
        {
            stopTimer();
            return;
        }
        const auto text = getText();
        const int caret = getCaretPosition();
        if (text == lastText && caret == lastCaret)
            return;
        lastText = text;
        lastCaret = caret;
        const auto prefix = currentPrefix();
        if (prefix.isEmpty() && ! openBracesAtCaret())
        {
            hideSuggestions();
            return;
        }
        showSuggestions (prefix);
    }

    bool openBracesAtCaret() const
    {
        return getText().substring (0, getCaretPosition()).endsWith ("{{");
    }

    /** The name prefix typed after the last unclosed "{{", or empty. */
    juce::String currentPrefix() const
    {
        const auto before = getText().substring (0, getCaretPosition());
        const int open = before.lastIndexOf ("{{");
        if (open < 0)
            return {};
        const auto between = before.substring (open + 2);
        if (between.containsChar ('}') || between.containsChar ('{'))
            return {};
        for (const auto ch : between)
            if (! (juce::CharacterFunctions::isLetterOrDigit (ch)
                   || ch == '.' || ch == '_' || ch == '-'))
                return {};
        return between;
    }

    void showSuggestions (const juce::String& prefix)
    {
        if (variables.empty())
            return;
        matches.clear();
        descriptions.clear();
        for (const auto& v : variables)
            if (prefix.isEmpty() || v.id.containsIgnoreCase (prefix))
            {
                matches.add (v.id);
                descriptions.add (v.description);
            }
        if (matches.isEmpty())
        {
            hideSuggestions();
            return;
        }
        highlight = 0;
        popup.setVisible (true);
        layoutPopup();
        popup.toFront (false);
        popup.repaint();
    }

    void hideSuggestions()
    {
        popup.setVisible (false);
        matches.clear();
        descriptions.clear();
        highlight = 0;
    }

    void layoutPopup()
    {
        if (! popup.isVisible())
            return;
        const int rows = juce::jmin (kMaxRows, (int) matches.size());
        const int w = juce::jmin (kPopupW, juce::jmax (160, getWidth() - 8));
        const int h = rows * Popup::rowH + 4;
        auto caret = getCaretRectangleForCharIndex (getCaretPosition());
        int x = caret.getX();
        int y = caret.getBottom() + 2;
        if (y + h > getHeight())
            y = juce::jmax (0, caret.getY() - h - 2);
        x = juce::jlimit (0, juce::jmax (0, getWidth() - w), x);
        popup.setBounds (x, y, w, h);
    }

    void moveHighlight (int delta)
    {
        if (matches.isEmpty())
            return;
        highlight = juce::jlimit (0, (int) matches.size() - 1,
                                  highlight + delta);
        popup.repaint();
    }

    void pick (int index)
    {
        if (index < 0 || index >= (int) matches.size())
            return;
        const auto name = matches[(size_t) index];
        auto text = getText();
        const int caret = getCaretPosition();
        const auto before = text.substring (0, caret);
        const int open = before.lastIndexOf ("{{");
        if (open < 0)
        {
            insertTextAtCaret ("{{" + name + "}}");
        }
        else
        {
            auto replaced = text.substring (0, open) + "{{" + name + "}}"
                          + text.substring (caret);
            const int newCaret = open + 4 + name.length();
            setText (replaced, juce::dontSendNotification);
            setCaretPosition (juce::jlimit (0, replaced.length(), newCaret));
        }
        hideSuggestions();
        grabKeyboardFocus();
        lastText = getText();
        lastCaret = getCaretPosition();
        if (onVariableInserted != nullptr)
            onVariableInserted (name);
    }

    Popup popup;
    std::vector<Variable> variables;
    juce::StringArray matches, descriptions;
    juce::String lastText;
    int lastCaret = 0;
    int highlight = 0;
    bool dragOver = false;
    juce::Point<int> dropPoint;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VariableTextEditor)
};

} // namespace ebs
