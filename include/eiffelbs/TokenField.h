// TokenField.h
// eiffelbs-ui - a field of TOKENS: chips for the values already chosen, and a
// small editor whose typing filters a suggestion list of the values the host
// ALLOWS.
//
// Why a component and not a TextEditor of space-separated names: a name the
// host does not recognise must be UNADDABLE, not merely discouraged. A free
// text field of ids accepts every typo and every name this build does not
// provide, and the failure lands much later, somewhere else ("template ...
// declares 'assistant.time', which this build does not provide"). Here the
// allowed set is the only set: typing filters it, Enter/Tab/click picks from
// it, and an exact match commits on space or comma for hands that type the
// whole name. A value already present is never offered twice, Backspace on an
// empty editor removes the last chip, and each chip's cross removes it.
//
// The popup is a child of the TOP-LEVEL component (a list that must draw
// outside the field's own bounds cannot be a child of the field), placed in
// the top-level's coordinates and closed by selection, Escape, focus loss or
// a wheel over the field - a stale popup floating over a scrolled panel is
// worse than no popup. It is REMOVED from its parent whenever it is hidden,
// so a member never faces two owners when a window dies.
//
// Unknown values ALREADY in a host's data are KEPT and shown with a danger
// outline: a hand-edited file must not silently lose a name this build cannot
// resolve - the reader removes it, the field never does.
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

#include "eiffelbs/Theme.h"
#include "eiffelbs/LookAndFeel.h"

namespace ebs
{

class TokenField : public juce::Component
{
public:
    /** One value the host allows. `description` is optional prose shown to
        the right of the id in the suggestion list. */
    struct Token
    {
        juce::String id;
        juce::String description;
    };

    TokenField()
    {
        addAndMakeVisible (editor);
        editor.setMultiLine (false);
        editor.setReturnKeyStartsNewLine (false);
        // The FIELD paints one rounded surface around the chips AND the
        // editor (a box in a box reads as two controls - the chat frame's
        // lesson).
        editor.setColour (juce::TextEditor::backgroundColourId,
                          juce::Colours::transparentBlack);
        editor.setColour (juce::TextEditor::outlineColourId,
                          juce::Colours::transparentBlack);
        editor.setColour (juce::TextEditor::focusedOutlineColourId,
                          juce::Colours::transparentBlack);
        editor.setColour (juce::TextEditor::textColourId, ebs::text());
        editor.setFont (ebs::fontBody());
        editor.setWantsKeyboardFocus (true);

        editor.onTextChange = [this] { editorTextChanged(); };
        // TextEditor carries onFocusLost but no onFocusGained std::function:
        // the gained half rides the virtual.
        editor.onFocusGainedCallback = [this] { editorFocusGained(); };
        editor.onFocusLost = [this] { editorFocusLost(); };
        editor.onKey = [this] (const juce::KeyPress& key) { return handleKey (key); };
        editor.onWheel = [this] { hideSuggestions(); };

        popup.onPick = [this] (const juce::String& id) { commit (id); };
    }

    ~TokenField() override
    {
        // The popup lives on the top-level component while shown: it must
        // leave that parent BEFORE this member dies, or the window would
        // delete what this class also owns.
        hideSuggestions();
    }

    /** The values this field may ADD. Values already set are kept whatever
        this list says (see the header note). */
    void setAllowedTokens (std::vector<Token> allowedTokens)
    {
        allowed = std::move (allowedTokens);
        refreshSuggestions();
        repaint();
    }

    /** The values currently held, in the order they were added. */
    juce::StringArray tokens() const { return chosen; }

    /** Replace the held values (a file load, a selection change). Unknown
        values are kept, not dropped. Never notifies. */
    void setTokens (const juce::StringArray& ids)
    {
        chosen.clear();
        for (const auto& id : ids)
            if (id.trim().isNotEmpty() && ! chosen.contains (id.trim()))
                chosen.add (id.trim());
        hideSuggestions();
        resized();
        repaint();
    }

    /** Fired when the value list changes by the user or by addToken/
        removeToken - never by setTokens. */
    std::function<void()> onTokensChanged;

    void setPlaceholder (const juce::String& text)
    {
        editor.setTextToShowWhenEmpty (text, ebs::textDim());
    }

    /** The allowed values a typed fragment matches, minus the ones already
        held. Empty fragment = everything still available. Case-insensitive
        substring match. Pure - the smoke measures it. */
    juce::StringArray suggestionsFor (const juce::String& typed) const
    {
        juce::StringArray out;
        const auto needle = typed.trim();
        for (const auto& t : allowed)
        {
            if (chosen.contains (t.id))
                continue;
            if (needle.isEmpty() || t.id.containsIgnoreCase (needle))
                out.add (t.id);
        }
        return out;
    }

    /** Add one value. False when the value is not allowed or already held -
        the same rule the typing obeys, exposed so a test can ask without a
        keyboard. */
    bool addToken (const juce::String& id)
    {
        const auto canonical = canonicalFor (id);
        if (canonical.isEmpty() || chosen.contains (canonical))
            return false;
        chosen.add (canonical);
        if (onTokensChanged != nullptr)
            onTokensChanged();
        hideSuggestions();
        resized();
        repaint();
        return true;
    }

    bool removeToken (const juce::String& id)
    {
        if (! chosen.contains (id))
            return false;
        chosen.removeString (id);
        if (onTokensChanged != nullptr)
            onTokensChanged();
        refreshSuggestions();
        resized();
        repaint();
        return true;
    }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (ebs::bgDark());
        g.fillRoundedRectangle (b, 4.0f);
        g.setColour (editor.hasKeyboardFocus (false)
                         ? ebs::accent().withAlpha (0.65f)
                         : ebs::panelBorder());
        g.drawRoundedRectangle (b, 4.0f, 1.0f);

        for (int i = 0; i < (int) chipRects.size(); ++i)
        {
            const auto r = chipRects[(size_t) i].toFloat();
            const bool known = isAllowed (chosen[i]);
            g.setColour (ebs::bgPanel());
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (known ? ebs::panelBorder() : ebs::danger());
            g.drawRoundedRectangle (r, 4.0f, 1.0f);

            g.setColour (known ? ebs::text() : ebs::danger());
            g.setFont (ebs::fontHint());
            g.drawText (chosen[i], r.toNearestInt().reduced (6, 0),
                        juce::Justification::centredLeft, true);

            // The cross: the chip's own remove affordance, drawn at its
            // right edge so the pointer has one obvious target.
            const auto cross = crossRect (i).toFloat();
            if (! cross.isEmpty())
            {
                g.setColour (known ? ebs::textDim() : ebs::danger());
                g.drawLine (cross.getX(), cross.getY(),
                            cross.getRight(), cross.getBottom(), 1.2f);
                g.drawLine (cross.getRight(), cross.getY(),
                            cross.getX(), cross.getBottom(), 1.2f);
            }
        }
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (4, 3);
        chipRects.clear();

        int x = b.getX();
        for (int i = 0; i < chosen.size(); ++i)
        {
            const int w = chipWidth (chosen[i]);
            // Leave room for the editor: a chip that eats the whole row
            // makes the field un-typeable.
            if (x + w > b.getRight() - 70)
                break;
            chipRects.push_back ({ x, b.getY(), w, b.getHeight() });
            x += w + 4;
        }

        editor.setBounds (x, b.getY(), juce::jmax (60, b.getRight() - x),
                          b.getHeight());
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int i = 0; i < (int) chipRects.size(); ++i)
            if (crossRect (i).contains (e.getPosition()))
            {
                removeToken (chosen[i]);
                return;
            }
    }

private:
    class Editor : public juce::TextEditor
    {
    public:
        std::function<bool (const juce::KeyPress&)> onKey;
        std::function<void()> onWheel;
        std::function<void()> onFocusGainedCallback;

        void focusGained (FocusChangeType) override
        {
            if (onFocusGainedCallback != nullptr)
                onFocusGainedCallback();
        }

        bool keyPressed (const juce::KeyPress& key) override
        {
            if (onKey != nullptr && onKey (key))
                return true;
            return juce::TextEditor::keyPressed (key);
        }

        void mouseWheelMove (const juce::MouseEvent& e,
                             const juce::MouseWheelDetails& wheel) override
        {
            // A popup floating over a panel that just scrolled is worse than
            // no popup: the wheel closes it and the wheel's own job goes on.
            if (onWheel != nullptr)
                onWheel();
            juce::TextEditor::mouseWheelMove (e, wheel);
        }
    };

    class SuggestionPopup : public juce::Component,
                            private juce::ListBoxModel
    {
    public:
        SuggestionPopup()
        {
            addAndMakeVisible (list);
            list.setModel (this);
            list.setRowHeight (24);
            list.setColour (juce::ListBox::backgroundColourId,
                            juce::Colours::transparentBlack);
            list.setColour (juce::ListBox::outlineColourId,
                            juce::Colours::transparentBlack);
            list.setOutlineThickness (0);
            // Mouse-only: clicking a row must not move the keyboard focus
            // away from the editor, or the focus-lost close would fire
            // before the click lands.
            list.setWantsKeyboardFocus (false);
        }

        /** Rows carry the id, the description dimmed to its right. */
        void setRows (std::vector<Token> rowsToShow)
        {
            rows = std::move (rowsToShow);
            selected = rows.empty() ? -1 : 0;
            list.updateContent();
            if (selected >= 0)
                list.selectRow (selected, false, true);
            resized();
        }

        void moveSelection (int delta)
        {
            if (rows.empty())
                return;
            selected = juce::jlimit (0, (int) rows.size() - 1, selected + delta);
            list.selectRow (selected, false, true);
        }

        juce::String selectedId() const
        {
            return selected >= 0 && selected < (int) rows.size()
                       ? rows[(size_t) selected].id
                       : juce::String();
        }

        int preferredWidth() const
        {
            int w = 0;
            for (const auto& t : rows)
            {
                w = juce::jmax (w, (int) ebs::fontHint().getStringWidth (t.id));
                if (t.description.isNotEmpty())
                    w = juce::jmax (w, (int) ebs::fontHint().getStringWidth (
                                           t.id + "   " + t.description));
            }
            return w + 28;
        }

        std::function<void (const juce::String&)> onPick;

        void paint (juce::Graphics& g) override
        {
            g.setColour (ebs::bgDark());
            g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f),
                                    5.0f);
            g.setColour (ebs::accent().withAlpha (0.35f));
            g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f),
                                    5.0f, 1.0f);
        }

        void resized() override
        {
            list.setBounds (getLocalBounds().reduced (3));
        }

    private:
        int getNumRows() override { return (int) rows.size(); }

        void paintListBoxItem (int row, juce::Graphics& g, int w, int h,
                               bool isSelected) override
        {
            if (row < 0 || row >= (int) rows.size())
                return;
            const auto& t = rows[(size_t) row];
            if (isSelected)
                g.fillAll (ebs::bgPanel());
            g.setColour (isSelected ? ebs::accent() : ebs::text());
            g.setFont (ebs::fontBody());
            const int idW = (int) ebs::fontBody().getStringWidth (t.id) + 8;
            g.drawText (t.id, 6, 0, idW, h, juce::Justification::centredLeft,
                        true);
            if (t.description.isNotEmpty())
            {
                g.setColour (ebs::textDim());
                g.setFont (ebs::fontHint());
                g.drawText (t.description, 6 + idW, 0, w - idW - 12, h,
                            juce::Justification::centredLeft, true);
            }
        }

        void listBoxItemClicked (int row, const juce::MouseEvent&) override
        {
            if (row >= 0 && row < (int) rows.size() && onPick != nullptr)
                onPick (rows[(size_t) row].id);
        }

        juce::ListBox list;
        std::vector<Token> rows;
        int selected = -1;
    };

    /** The canonical spelling of an allowed id, case-insensitively - "" when
        nothing corresponds. Committing the CANONICAL id is what makes the
        file carry what the build provides, whatever was typed. */
    juce::String canonicalFor (const juce::String& typed) const
    {
        const auto needle = typed.trim();
        if (needle.isEmpty())
            return {};
        for (const auto& t : allowed)
            if (t.id.equalsIgnoreCase (needle))
                return t.id;
        return {};
    }

    bool isAllowed (const juce::String& id) const
    {
        return canonicalFor (id).isNotEmpty();
    }

    bool handleKey (const juce::KeyPress& key)
    {
        if (key.isKeyCode (juce::KeyPress::downKey))
        {
            if (! popupShown)
                showSuggestions();
            else
                popup.moveSelection (1);
            return true;
        }
        if (key.isKeyCode (juce::KeyPress::upKey))
        {
            if (popupShown)
                popup.moveSelection (-1);
            return true;
        }
        if (key.isKeyCode (juce::KeyPress::returnKey)
            || key.isKeyCode (juce::KeyPress::tabKey))
        {
            if (popupShown && popup.selectedId().isNotEmpty())
            {
                commit (popup.selectedId());
                return true;
            }
            const auto canonical = canonicalFor (editor.getText());
            if (canonical.isNotEmpty())
            {
                commit (canonical);
                return true;
            }
            return false;   // nothing to pick: let the editor have the key
        }
        if (key.isKeyCode (juce::KeyPress::escapeKey))
        {
            if (popupShown)
            {
                hideSuggestions();
                return true;
            }
            return false;
        }
        if (key.isKeyCode (juce::KeyPress::backspaceKey)
            && editor.getText().isEmpty() && ! chosen.isEmpty())
        {
            removeToken (chosen[chosen.size() - 1]);
            return true;
        }
        if (key.getTextCharacter() == ' ' || key.getTextCharacter() == ',')
        {
            const auto canonical = canonicalFor (editor.getText());
            if (canonical.isNotEmpty())
            {
                commit (canonical);
                return true;
            }
        }
        return false;
    }

    void editorTextChanged()
    {
        refreshSuggestions();
    }

    void editorFocusGained()
    {
        repaint();   // the focus ring
        showSuggestions();
    }

    void editorFocusLost()
    {
        repaint();
        // JUCE delivers this on a LATER message-loop turn, so the report can
        // arrive after the focus is already back somewhere real (the copy
        // pattern, defect 23): only close when the keyboard went somewhere
        // that EXISTS and is not our own editor.
        auto* now = juce::Component::getCurrentlyFocusedComponent();
        if (now != nullptr && now != &editor && popup.isParentOf (now))
            return;   // the popup itself (mouse-only, kept for safety)
        hideSuggestions();
    }

    void showSuggestions()
    {
        refreshSuggestions();
    }

    void hideSuggestions()
    {
        if (! popupShown)
            return;
        popupShown = false;
        popup.setVisible (false);
        if (auto* parent = popup.getParentComponent())
            parent->removeChildComponent (&popup);
    }

    void refreshSuggestions()
    {
        if (! editor.hasKeyboardFocus (false) && ! popupShown)
            return;

        const auto ids = suggestionsFor (editor.getText());
        if (ids.isEmpty())
        {
            hideSuggestions();
            return;
        }

        std::vector<Token> rowsToShow;
        for (const auto& id : ids)
            for (const auto& t : allowed)
                if (t.id == id)
                {
                    rowsToShow.push_back (t);
                    break;
                }

        auto* top = getTopLevelComponent();
        if (top == nullptr || top == this)
            return;

        if (popup.getParentComponent() != top)
            top->addAndMakeVisible (popup);
        popup.setRows (std::move (rowsToShow));

        const int width = juce::jlimit (180, 460,
                                        juce::jmax (getWidth(), popup.preferredWidth()));
        const int rowsShown = juce::jmin (7, (int) ids.size());
        const int height = rowsShown * 24 + 8;

        // Place under the field in the top-level's coordinates; flip above
        // when the window has no room below (a field near the bottom edge).
        const auto inTop = top->getLocalArea (this, getLocalBounds());
        auto y = inTop.getBottom() + 2;
        if (y + height > top->getHeight() - 4)
            y = juce::jmax (4, inTop.getY() - height - 2);

        popup.setBounds (inTop.getX(), y, width, height);
        popup.setVisible (true);
        popup.toFront (false);
        popupShown = true;
    }

    void commit (const juce::String& id)
    {
        if (! addToken (id))
            return;
        editor.clear();
        // addToken hid the popup; a fresh fragment is empty, so show the
        // remaining values again for the next pick.
        if (editor.hasKeyboardFocus (false))
            refreshSuggestions();
    }

    juce::Rectangle<int> chipRect (int index) const
    {
        return index >= 0 && index < (int) chipRects.size()
                   ? chipRects[(size_t) index] : juce::Rectangle<int>();
    }

    juce::Rectangle<int> crossRect (int index) const
    {
        const auto r = chipRect (index);
        if (r.isEmpty())
            return {};
        return juce::Rectangle<int> (r.getRight() - 14,
                                     r.getCentreY() - 4, 9, 9);
    }

    int chipWidth (const juce::String& id) const
    {
        return (int) ebs::fontHint().getStringWidth (id) + 26;
    }

    Editor editor;
    SuggestionPopup popup;
    std::vector<Token> allowed;
    juce::StringArray chosen;
    std::vector<juce::Rectangle<int>> chipRects;
    bool popupShown = false;

    friend struct TokenFieldTestAccess;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TokenField)
};

} // namespace ebs
