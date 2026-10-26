// SearchableModelPicker.h
// eiffelbs-ui - a labelled picker that opens a FILTERABLE list in a CallOut.
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
#pragma once
/*
 * SearchableModelPicker.h - ONE searchable model picker for the Engine frames
 * (idea #162 phase 2, author decision 6=A: "searchable model pickers in the
 * Engine frames - audiocpp first").
 *
 * WHY: a flat juce::ComboBox does not scale to hundreds of audiocpp packages /
 * OT-serve models. The picker keeps a combo's FOOTPRINT (same row height, same
 * bgDark + panelBorder + chevron look) but opens a CallOutBox carrying a search
 * field, a filtered list and an "N of M" footer. Enter picks the first (or the
 * highlighted) match, Escape closes with NO selection, a click outside simply
 * dismisses it. A disabled picker paints dim and never opens.
 *
 * The public contract is deliberately ComboBox-shaped, so an Engine frame can
 * swap one in without touching its editor wiring:
 *
 *     ebs::SearchableModelPicker picker;
 *     picker.setItems (ids, labels, details);      // details optional
 *     picker.setSelectedId (id, /*notify* / false);
 *     picker.setTextWhenNothingSelected ("(built-in)");
 *     picker.onChange = [] (const juce::String& id) { ... };
 *
 * Matching is case-insensitive over the label, the id AND the detail line, so
 * "audio8" and "q8" both narrow a long catalog. A setItems() that no longer
 * contains the selected id CLEARS the selection without firing onChange (a
 * host repopulating from a refresh must not be told the user picked something).
 *
 * ASCII source (repo convention).
 */
#include <juce_gui_basics/juce_gui_basics.h>
#include <eiffelbs/eiffelbs.h>
#include <functional>
#include <cmath>
#include <vector>

namespace ebs
{

class SearchableModelPicker : public juce::Component,
                              public juce::SettableTooltipClient
{
public:
    SearchableModelPicker() = default;

    ~SearchableModelPicker() override
    {
        // The CallOutBox owns the popup content; drop our pointer first so a
        // late dismissal cannot call back into a destroyed picker.
        if (popup != nullptr)
            popup->owner = nullptr;
        closePopup (false);
    }

    // === Data =============================================================
    void setItems (const juce::StringArray& newIds,
                   const juce::StringArray& newLabels,
                   const juce::StringArray& newDetails = {})
    {
        ids = newIds;
        labels = newLabels;
        details = newDetails;
        // The selected id must still exist: a refresh that dropped it clears
        // the selection SILENTLY (the host is repopulating, not the user).
        if (selectedIdValue.isNotEmpty() && ! ids.contains (selectedIdValue))
            selectedIdValue.clear();
        searchText.clear();
        highlighted = 0;
        applyFilter();
        repaint();
    }

    void setSelectedId (const juce::String& id, bool notify = false)
    {
        if (selectedIdValue == id)
            return;
        selectedIdValue = ids.contains (id) ? id : juce::String();
        highlighted = 0;
        repaint();
        if (notify && onChange != nullptr)
            onChange (selectedIdValue);
    }

    juce::String selectedId() const { return selectedIdValue; }

    void setTextWhenNothingSelected (const juce::String& text)
    {
        placeholder = text;
        repaint();
    }
    juce::String getTextWhenNothingSelected() const { return placeholder; }

    /** INSTALLED-ONLY mode (SFX uses it): only ids listed via setInstalledIds
        are eligible - unusable rows are not shown at all, instead of being
        greyed out (the picker has no disabled-item concept). */
    void setInstalledOnly (bool on)
    {
        installedOnly = on;
        searchText.clear();
        highlighted = 0;
        hoveredIndex = -1;
        applyFilter();
        repaint();
    }
    bool isInstalledOnly() const { return installedOnly; }

    /** Mark which ids the caller considers installed/eligible. Only consulted
        when setInstalledOnly(true) is in force. */
    void setInstalledIds (const juce::StringArray& idsThatAreInstalled)
    {
        installedIds = idsThatAreInstalled;
        applyFilter();
        repaint();
    }

    /** Fine link pinned at the BOTTOM of the popup ("Manage models..."): fires
        the same callback the frame's own link does, and closes the popup. Shown
        only when set. */
    std::function<void()> onManageModels;

    /** The label the component paints right now (selected label, or the
        placeholder when nothing is selected). Gate seam: proves what the
        reader sees without a screenshot. */
    juce::String displayText() const
    {
        if (selectedIdValue.isEmpty())
            return placeholder;
        const int index = ids.indexOf (selectedIdValue);
        return index >= 0 && index < labels.size() ? labels[index]
                                                   : selectedIdValue;
    }

    // === Combo-compatible accessors ======================================
    // The Engine frames expose control-API helpers written against
    // juce::ComboBox (item count, item text, selected index). These keep those
    // helpers working over the picker's ONE array (no second list).
    int itemCount() const { return ids.size(); }
    juce::String itemIdAt (int index) const
    {
        return index >= 0 && index < ids.size() ? ids[index] : juce::String();
    }
    juce::String itemLabelAt (int index) const
    {
        if (index < 0 || index >= ids.size())
            return {};
        return index < labels.size() && labels[index].isNotEmpty()
                   ? labels[index] : ids[index];
    }
    int selectedIndex() const
    {
        return selectedIdValue.isEmpty() ? -1 : ids.indexOf (selectedIdValue);
    }
    /** Select by row index (control-API path): silent by default, because the
        caller runs its own switch handler afterwards. */
    void selectItemIndex (int index, bool notify = false)
    {
        if (index >= 0 && index < ids.size())
            setSelectedId (ids[index], notify);
    }

    /** Picker -> host: the reader picked this id (never fired by setItems or
        setSelectedId(id, false)). */
    std::function<void (const juce::String&)> onChange;

    // === Component ========================================================
    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (! isEnabled() ? ebs::bgDark().darker (0.08f)
                      : hover   ? ebs::bgDark().brighter (0.07f)
                                : ebs::bgDark());
        g.fillRoundedRectangle (b, 5.0f);
        g.setColour (hover && isEnabled() ? ebs::accent().withAlpha (0.40f)
                                          : ebs::panelBorder());
        g.drawRoundedRectangle (b, 5.0f, 1.0f);

        auto row = getLocalBounds().reduced (7, 0);
        const auto chevronArea = row.removeFromRight (16);
        const auto text = displayText();
        g.setColour (! isEnabled() ? ebs::textDim().withAlpha (0.55f)
                                   : selectedIdValue.isNotEmpty() ? ebs::text()
                                                                  : ebs::textDim());
        g.setFont (ebs::fontHint());
        g.drawFittedText (text, row, juce::Justification::centredLeft, 1);
        g.setColour (! isEnabled() ? ebs::textDim().withAlpha (0.4f)
                                   : ebs::accent());
        const auto glyph = chevronArea.withSizeKeepingCentre (10, 10).toFloat();
        g.fillPath (ebs::IconButton::glyphPath (
            ebs::IconButton::Shape::chevronDown, glyph, true, false, 10.0f));
    }

    void mouseEnter (const juce::MouseEvent&) override
    {
        hover = true;
        repaint();
    }
    void mouseExit (const juce::MouseEvent&) override
    {
        hover = false;
        repaint();
    }
    void mouseDown (const juce::MouseEvent&) override
    {
        if (! isEnabled())
            return;                     // a disabled picker never opens
        if (openState)
            closePopup (false);
        else
            openPopup();
    }
    void enablementChanged() override
    {
        if (! isEnabled())
            closePopup (false);
        repaint();
    }

    /** Open the picker's OWN popup anchored to ANOTHER component. A menu
        button that shows the chosen model and effort (ModelMenuButton) embeds
        this list without drawing a second picker face, so the two surfaces
        share ONE list implementation. */
    void openPopupFor (juce::Component* anchor)
    {
        if (! isEnabled())
            return;
        anchorComponent = anchor;
        openPopup();
    }

    // === TEST SEAMS =======================================================
    // A headless gate cannot click a CallOutBox; these drive the SAME state and
    // dispatch the same code paths the real popup uses.
    void openForTest()            { if (isEnabled()) openPopup(); }
    void closeForTest()           { closePopup (false); }
    void setSearchForTest (const juce::String& text)
    {
        searchText = text;
        highlighted = 0;
        applyFilter();
        if (popup != nullptr)
            popup->repaint();
        repaint();
    }
    int visibleIdCountForTest() const   { return (int) filtered.size(); }
    juce::String idAtForTest (int index) const
    {
        return index >= 0 && index < (int) filtered.size()
                   ? ids[filtered[(size_t) index]] : juce::String();
    }
    juce::StringArray labelsForTest() const
    {
        juce::StringArray out;
        for (int index : filtered)
            if (index >= 0 && index < labels.size())
                out.add (labels[index]);
        return out;
    }
    void selectIndexForTest (int index)
    {
        if (index < 0 || index >= (int) filtered.size())
            return;
        pick (filtered[(size_t) index]);
    }
    void pressEnterForTest()            { pressEnter(); }
    bool popupOpenForTest() const       { return openState; }
    juce::String footerTextForTest() const { return footerText(); }
    /** Hover highlight seam: the SAME state the popup paints for the pointer
        row (mouseMove calls setHoveredRow, mouseExit clears it). */
    int hoveredIndexForTest() const     { return hoveredIndex; }
    void hoverRowForTest (int visibleIndex)
    {
        setHoveredRow (visibleIndex);
    }
    void clearHoverForTest()            { setHoveredRow (-1); }
    /** Rows eligible after the installed-only filter (the footer's Y). */
    int eligibleCountForTest() const    { return (int) eligible.size(); }
    /** The bottom "Manage models..." link: present only when the callback set. */
    bool manageLinkVisibleForTest() const { return onManageModels != nullptr; }
    void clickManageLinkForTest()       { activateManageLink(); }
    /** Hover state of that link (the popup paints it brighter + tinted). */
    bool manageLinkHoveredForTest() const { return manageHovered; }
    void setManageHoveredForTest (bool on) { setManageHovered (on); }
    /** Footer geometry seams: the link sits LEFT of the "X of Y" count on the
        SAME 18 px row (the gate asserts the two rectangles never overlap).
        Computed from the shared layout helper, so no window is needed. */
    juce::Rectangle<int> manageLinkBoundsForTest() const
    {
        juce::Rectangle<int> link, count;
        layoutFooterRow ({ 0, 0, juce::jmax (0, getWidth() - 8), kFooterRowH },
                         onManageModels != nullptr, link, count);
        return link;
    }
    juce::Rectangle<int> footerCountBoundsForTest() const
    {
        juce::Rectangle<int> link, count;
        layoutFooterRow ({ 0, 0, juce::jmax (0, getWidth() - 8), kFooterRowH },
                         onManageModels != nullptr, link, count);
        return count;
    }
    /** The named empty state the popup shows when nothing matches. */
    juce::String emptyStateForTest() const
    {
        return filtered.empty() ? juce::String ("no models available")
                                : juce::String();
    }

private:
    /** Width of `text` at the picker font, measured with GlyphArrangement (the
        Font::getStringWidth helper is deprecated in this JUCE build). */
    static float textWidthOf (const juce::String& text)
    {
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText (ebs::fontHint(), text, 0.0f, 0.0f);
        return glyphs.getBoundingBox (0, -1, true).getWidth();
    }

    static constexpr int kFooterRowH = 18;

    /** The ONE footer layout: "Manage models..." CENTRED on the row, "X of Y"
        right-aligned over the same 18 px row (author preference 2026-10-26;
        it used to sit on the left). Shared by the popup and the seams. */
    static void layoutFooterRow (juce::Rectangle<int> footerRow,
                                 bool manageVisible,
                                 juce::Rectangle<int>& linkOut,
                                 juce::Rectangle<int>& countOut)
    {
        linkOut = {};
        countOut = footerRow;
        if (! manageVisible || footerRow.isEmpty())
            return;
        const int linkW = juce::jmin (
            footerRow.getWidth() - 60,
            (int) std::ceil (textWidthOf ("Manage models...")) + 16);
        linkOut = footerRow.withSizeKeepingCentre (juce::jmax (0, linkW),
                                                   footerRow.getHeight());
        countOut = footerRow;
    }

    // === State ============================================================
    juce::StringArray ids, labels, details;
    juce::StringArray installedIds;   // eligibility set (installed-only mode)
    juce::String selectedIdValue;
    juce::String placeholder;
    juce::String searchText;
    std::vector<int> eligible;   // rows left after the installed-only filter
    std::vector<int> filtered;   // indices into ids, current search applied
    int highlighted = 0;
    int hoveredIndex = -1;       // visible row under the pointer (-1 = none)
    bool manageHovered = false;  // pointer over the bottom "Manage models..." link
    bool installedOnly = false;
    bool hover = false;
    bool openState = false;

    void applyFilter()
    {
        eligible.clear();
        filtered.clear();
        for (int i = 0; i < ids.size(); ++i)
        {
            // INSTALLED-ONLY: a row the caller did not mark is not eligible at
            // all - it is not listed, and it is not counted in the footer.
            if (installedOnly && ! installedIds.contains (ids[i]))
                continue;
            eligible.push_back (i);
        }
        const auto query = searchText.trim();
        for (const int i : eligible)
        {
            if (query.isNotEmpty())
            {
                const bool hit = ids[i].containsIgnoreCase (query)
                              || (i < labels.size()
                                      && labels[i].containsIgnoreCase (query))
                              || (i < details.size()
                                      && details[i].containsIgnoreCase (query));
                if (! hit)
                    continue;
            }
            filtered.push_back (i);
        }
        if (highlighted >= (int) filtered.size())
            highlighted = juce::jmax (0, (int) filtered.size() - 1);
        if (hoveredIndex >= (int) filtered.size())
            hoveredIndex = -1;
    }

    /** "X of Y": X = the rows listed right now (search applied), Y = the rows
        ELIGIBLE (installed-only applied, search NOT applied). The author saw
        "10 of 10" over 8 rows because Y used the raw setItems count. */
    juce::String footerText() const
    {
        return juce::String ((int) filtered.size()) + " of "
             + juce::String ((int) eligible.size());
    }

    void setHoveredRow (int visibleIndex)
    {
        const int clamped = visibleIndex >= 0 && visibleIndex < (int) filtered.size()
                                ? visibleIndex : -1;
        if (clamped == hoveredIndex)
            return;
        hoveredIndex = clamped;
        if (popup != nullptr)
            popup->repaint();
    }

    /** The popup's bottom link: close first (the box is modal), then ask the
        host - exactly like the frame's own link. */
    void activateManageLink()
    {
        if (onManageModels == nullptr)
            return;
        closePopup (false);
        onManageModels();
    }

    /** The link's hover state is owned by the picker (the popup only reports
        the pointer), so a test/caller can read it without the window. */
    void setManageHovered (bool on)
    {
        if (manageHovered == on)
            return;
        manageHovered = on;
        if (popup != nullptr)
        {
            popup->setMouseCursor (on ? juce::MouseCursor::PointingHandCursor
                                      : juce::MouseCursor::NormalCursor);
            popup->repaint();
        }
    }

    void pick (int itemIndex)
    {
        if (itemIndex < 0 || itemIndex >= ids.size())
            return;
        selectedIdValue = ids[itemIndex];
        repaint();
        closePopup (false);
        if (onChange != nullptr)
            onChange (selectedIdValue);
    }

    void pressEnter()
    {
        if (filtered.empty())
            return;
        const int index = juce::jlimit (0, (int) filtered.size() - 1, highlighted);
        pick (filtered[(size_t) index]);
    }

    // === The popup ========================================================
    /** The search field: a TextEditor subclass so Up/Down move the highlight
        before the editor's own newline handling sees the key. */
    struct SearchField : ebs::ClearableEditor
    {
        SearchableModelPicker* owner = nullptr;
        bool keyPressed (const juce::KeyPress& key) override
        {
            if (owner == nullptr)
                return juce::TextEditor::keyPressed (key);
            if (key == juce::KeyPress::upKey)
                return owner->moveHighlight (-1);
            if (key == juce::KeyPress::downKey)
                return owner->moveHighlight (1);
            if (key == juce::KeyPress::escapeKey)
            {
                owner->closePopup (false);
                return true;
            }
            if (key == juce::KeyPress::returnKey)
            {
                owner->pressEnter();
                return true;
            }
            return juce::TextEditor::keyPressed (key);
        }
    };

    /** The CallOutBox content: search field, filtered list (label + dim detail),
        "N of M" footer, named empty state. */
    struct PopupContent : juce::Component
    {
        SearchableModelPicker* owner = nullptr;
        SearchField search;
        juce::Rectangle<int> listArea, footerArea, manageLinkArea, footerCountArea;

        explicit PopupContent (SearchableModelPicker* o) : owner (o)
        {
            search.owner = o;
            search.setFont (ebs::fontHint());
            search.setColour (juce::TextEditor::textColourId, ebs::text());
            search.setColour (juce::TextEditor::backgroundColourId, ebs::bgDark());
            search.setColour (juce::TextEditor::outlineColourId, ebs::panelBorder());
            search.setTextToShowWhenEmpty ("Search models...", ebs::textDim());
            search.onTextChange = [this]
            {
                if (owner != nullptr)
                    owner->setSearchForTest (search.getText());
            };
            addAndMakeVisible (search);
            setSize (360, 26 + 8 * kRowH + 18);
        }

        ~PopupContent() override
        {
            if (owner != nullptr)
                owner->popupClosed (this);
        }

        static constexpr int kRowH = 24;
        static constexpr int kFooterH = 18;   // ONE row: link left, count right

        void resized() override
        {
            auto b = getLocalBounds().reduced (4);
            search.setBounds (b.removeFromTop (24));
            b.removeFromTop (4);
            footerArea = b.removeFromBottom (kFooterH);
            SearchableModelPicker::layoutFooterRow (
                footerArea, owner != nullptr && owner->onManageModels != nullptr,
                manageLinkArea, footerCountArea);
            listArea = b;
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (ebs::bgPanel());
            if (owner == nullptr)
                return;
            auto area = listArea;
            if (owner->filtered.empty())
            {
                g.setColour (ebs::textDim());
                g.setFont (ebs::fontHint());
                g.drawText ("no models available", area,
                            juce::Justification::centred, true);
            }
            else
            {
                // One highlight language for BOTH inputs: the pointer row wins
                // when the mouse is over the list, the keyboard row otherwise.
                const int activeRow = owner->hoveredIndex >= 0
                                          ? owner->hoveredIndex
                                          : owner->highlighted;
                for (int i = 0; i < (int) owner->filtered.size(); ++i)
                {
                    auto rowArea = area.removeFromTop (kRowH);
                    if (rowArea.isEmpty())
                        break;
                    const int idIndex = owner->filtered[(size_t) i];
                    if (i == activeRow)
                    {
                        g.setColour (ebs::accentSoft());
                        g.fillRect (rowArea);
                    }
                    // The SELECTED row keeps its own marker (a small accent rail)
                    // so "what a click selects" and "what is currently chosen"
                    // never read as the same thing.
                    if (owner->ids[idIndex] == owner->selectedIdValue)
                    {
                        g.setColour (ebs::accent());
                        g.fillRect (rowArea.removeFromLeft (3));
                    }
                    auto row = rowArea.reduced (6, 0);
                    juce::String detail;
                    if (idIndex < owner->details.size())
                        detail = owner->details[idIndex];
                    if (detail.isNotEmpty())
                    {
                        const int detailW = juce::jmin (
                            row.getWidth() / 2,
                            (int) std::ceil (textWidthOf (detail)) + 8);
                        g.setColour (ebs::textDim());
                        g.setFont (ebs::fontHint());
                        g.drawFittedText (detail, row.removeFromRight (detailW),
                                          juce::Justification::centredRight, 1);
                    }
                    g.setColour (ebs::text());
                    g.setFont (ebs::fontHint());
                    g.drawFittedText (idIndex < owner->labels.size()
                                          ? owner->labels[idIndex]
                                          : owner->ids[idIndex],
                                      row, juce::Justification::centredLeft, 1);
                }
            }
            g.setColour (ebs::textDim());
            g.setFont (ebs::fontHint());
            g.drawText (owner->footerText(), footerCountArea,
                        juce::Justification::centredRight, true);
            // Left-aligned link on the SAME footer row. It reads as a link
            // (accent ink) and gains a tinted hover pill + pointing hand, so
            // the author can SEE that it is clickable.
            if (owner->onManageModels != nullptr && ! manageLinkArea.isEmpty())
            {
                if (owner->manageHovered)
                {
                    g.setColour (ebs::accent().withAlpha (0.16f));
                    g.fillRoundedRectangle (manageLinkArea.toFloat(), 4.0f);
                }
                g.setColour (owner->manageHovered
                                 ? ebs::accent().brighter (0.15f)
                                 : ebs::accent());
                g.setFont (ebs::fontHint());
                g.drawText ("Manage models...", manageLinkArea.reduced (4, 0),
                            juce::Justification::centredLeft, true);
            }
        }

        void mouseMove (const juce::MouseEvent& e) override
        {
            if (owner == nullptr)
                return;
            const bool overLink = owner->onManageModels != nullptr
                               && ! manageLinkArea.isEmpty()
                               && manageLinkArea.contains (e.getPosition());
            owner->setManageHovered (overLink);
            if (! listArea.contains (e.getPosition()))
            {
                owner->setHoveredRow (-1);
                return;
            }
            owner->setHoveredRow (
                (e.getPosition().y - listArea.getY()) / kRowH);
        }

        void mouseExit (const juce::MouseEvent&) override
        {
            if (owner != nullptr)
            {
                owner->setHoveredRow (-1);
                owner->setManageHovered (false);
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (owner == nullptr)
                return;
            if (owner->onManageModels != nullptr && ! manageLinkArea.isEmpty()
                && manageLinkArea.contains (e.getPosition()))
            {
                owner->activateManageLink();
                return;
            }
            if (! listArea.contains (e.getPosition()))
                return;
            const int row = (e.getPosition().y - listArea.getY()) / kRowH;
            if (row >= 0 && row < (int) owner->filtered.size())
                owner->pick (owner->filtered[(size_t) row]);
        }
    };

    juce::Component::SafePointer<PopupContent> popup;
    juce::CallOutBox* callout = nullptr;
    juce::Component* anchorComponent = nullptr;   // openPopupFor(), not owned

    bool moveHighlight (int delta)
    {
        if (filtered.empty())
            return false;
        highlighted = juce::jlimit (0, (int) filtered.size() - 1,
                                    highlighted + delta);
        if (popup != nullptr)
            popup->repaint();
        return true;
    }

    void popupClosed (PopupContent* content)
    {
        if (popup == content)
            popup = nullptr;
        anchorComponent = nullptr;
        openState = false;
        callout = nullptr;
    }

    void openPopup()
    {
        if (openState)
            return;
        applyFilter();
        highlighted = 0;
        searchText.clear();
        openState = true;
        // The CallOutBox is a real top-level window: launch it only when the
        // anchor is on screen (the picker itself may be an invisible helper of
        // a menu button - openPopupFor() anchors to the button, not to it).
        const bool anchorShowing = anchorComponent != nullptr
                                       ? anchorComponent->isShowing()
                                       : isShowing();
        if (anchorShowing)
        {
            auto content = std::make_unique<PopupContent> (this);
            const int rows = juce::jmin (8, juce::jmax (1, (int) filtered.size()));
            content->setSize (juce::jmax (280, getWidth() - 2),
                              26 + rows * PopupContent::kRowH
                                  + PopupContent::kFooterH + 4);
            popup = content.get();
            const auto anchorBounds = anchorComponent != nullptr
                                          ? anchorComponent->getScreenBounds()
                                          : getScreenBounds();
            callout = &juce::CallOutBox::launchAsynchronously (
                std::move (content), anchorBounds, nullptr);
            juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<
                                                  SearchableModelPicker> (this)]
            {
                if (safe != nullptr && safe->popup != nullptr)
                    safe->popup->search.grabKeyboardFocus();
            });
        }
        repaint();
    }

    void closePopup (bool /*notify*/)
    {
        const bool wasOpen = openState;
        openState = false;
        if (callout != nullptr)
        {
            auto* box = callout;
            callout = nullptr;
            box->dismiss();
        }
        if (popup != nullptr)
            popup = nullptr;
        if (wasOpen)
            repaint();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SearchableModelPicker)
};

} // namespace ebs
#include "eiffelbs/ClearableEditor.h"
