// SearchableModelPicker.h
// eiffelbs-ui - a labelled picker that opens a FILTERABLE, GROUPED, SCROLLABLE
// list in a CallOut.
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
#pragma once
/*
 * SearchableModelPicker.h - ONE searchable model picker for the Engine frames
 * and the chat input's Model row (idea #162 phase 2, author decision 6=A:
 * "searchable model pickers in the Engine frames - audiocpp first").
 *
 * WHY: a flat juce::ComboBox does not scale to hundreds of audiocpp packages /
 * OT-serve models. The picker keeps a combo's FOOTPRINT (same row height, same
 * bgDark + panelBorder + chevron look) but opens a CallOutBox carrying a search
 * field, a filtered list and an "N of M" footer. Enter picks the first (or the
 * highlighted) match, Escape closes with NO selection, a click outside simply
 * dismisses it. A disabled picker never opens.
 *
 * GROUPING + SCROLL (author report 2026-10-26 - "la liste n'est plus scrollable
 * bien qu'il y ait plus de 90 modèles possibles", and the provider grouping was
 * lost to a second per-row column): setItems() takes an optional parallel
 * `groups` array, one provider label per model. A header row opens each run of
 * same-group models (the old chat list's language: uppercase, dim, never
 * selectable), typing a provider narrows to ITS rows, headers follow their
 * models (a header shows when at least one of its rows matches), the popup is a
 * real scrolling view - a scrollbar and the wheel move through every row, and
 * opening scrolls to the CURRENT model. Hosts that pass no groups get exactly
 * the old flat list.
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
 * Matching is case-insensitive over the label, the id, the detail line AND the
 * group name, so "audio8" and "q8" both narrow a long catalog. A setItems() that
 * no longer contains the selected id CLEARS the selection without firing onChange
 * (a host repopulating from a refresh must not be told the user picked something).
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
    /** `groups` is an optional parallel array: the provider label for each
        model, in ids order. When it covers every row, runs of same-group
        models get a header between them (never selectable) and typing a
        provider narrows to its rows. Pass no groups = the flat list as before. */
    void setItems (const juce::StringArray& newIds,
                   const juce::StringArray& newLabels,
                   const juce::StringArray& newDetails = {},
                   const juce::StringArray& newGroups = {})
    {
        ids = newIds;
        labels = newLabels;
        details = newDetails;
        groups = newGroups;
        // The selected id must still exist: a refresh that dropped it clears
        // the selection SILENTLY (the host is repopulating, not the user).
        if (selectedIdValue.isNotEmpty() && ! ids.contains (selectedIdValue))
            selectedIdValue.clear();
        searchText.clear();
        highlighted = 0;
        applyFilter();
        if (popup != nullptr)
        {
            popup->updateScroll();
            popup->repaint();
        }
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
        {
            popup->updateScroll();
            popup->repaint();
        }
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

    // === Grouping + scroll seams =========================================
    // The author's report was about the LIST the reader sees: headers per
    // provider, and every row reachable. These prove both headlessly - the
    // popup geometry needs no window for them (the open-time view is a pure
    // function of the filtered rows, and the scrollbar range follows).
    int displayRowCountForTest() const { return (int) display.size(); }
    int headerCountForTest() const
    {
        int n = 0;
        for (const auto& e : display)
            if (e.isHeader())
                ++n;
        return n;
    }
    bool rowIsHeaderForTest (int row) const
    {
        return row >= 0 && row < (int) display.size()
                 && display[(size_t) row].isHeader();
    }
    juce::String rowHeaderTextForTest (int row) const
    {
        if (row < 0 || row >= (int) display.size())
            return {};
        return display[(size_t) row].header;
    }
    /** Rows the popup shows at open (headers included), and how far its view
        can scroll below them: > 90 models MUST give a non-zero range, that is
        the whole complaint ("la liste n'est plus scrollable"). */
    int viewRowsForTest() const
    {
        return juce::jmin (PopupContent::kMaxViewRows,
                           juce::jmax (1, (int) display.size()));
    }
    int scrollRangeForTest() const
    {
        return juce::jmax (0, (int) display.size() - viewRowsForTest());
    }
    /** The row the keyboard highlight sits on right now. */
    int highlightedForTest() const
    {
        return filtered.empty() ? -1
                                : juce::jlimit (0, (int) filtered.size() - 1,
                                                highlighted);
    }
    /** The DISPLAY row (headers counted) of the keyboard highlight: opening
        with a current model lands ON that model's run. */
    int highlightDisplayRowForTest() const
    {
        return displayRowOfModel (highlightedForTest());
    }
    /** Offscreen render of the popup list - the SAME component the CallOutBox
        paints on screen, so the gate counts ink in header rows and can prove a
        90-model catalog is a real scrolling view. */
    void paintPopupForTest (juce::Graphics& g, int width)
    {
        PopupContent content (this);
        const int rows = viewRowsForTest();
        content.setSize (width, 26 + 4 + rows * PopupContent::kRowH
                            + PopupContent::kFooterH + 4);
        content.paintEntireComponent (g, true);
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

    /** One row of the popup list: a provider header (never picked) or one of
        its models - the old chat list's language, restored. */
    struct RowEntry
    {
        juce::String header;   // non-empty => provider header row for a run
        int idIndex = -1;      // index into ids, for model rows
        bool isHeader() const { return header.isNotEmpty(); }
    };

    // === State ============================================================
    juce::StringArray ids, labels, details, groups;
    juce::StringArray installedIds;   // eligibility set (installed-only mode)
    juce::String selectedIdValue;
    juce::String placeholder;
    juce::String searchText;
    std::vector<int> eligible;   // rows left after the installed-only filter
    std::vector<int> filtered;   // indices into ids, current search applied
    std::vector<RowEntry> display;   // filtered + header runs, popup order
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
        const bool grouped = groups.size() == ids.size();
        for (const int i : eligible)
        {
            if (query.isNotEmpty())
            {
                const bool hit = ids[i].containsIgnoreCase (query)
                              || (i < labels.size()
                                      && labels[i].containsIgnoreCase (query))
                              || (i < details.size()
                                      && details[i].containsIgnoreCase (query))
                              // Typing a provider narrows to ITS rows - the
                              // old chat list matched on the group name, and
                              // with grouping the provider is no longer shown
                              // per row, so it MUST stay searchable.
                              || (grouped && i < groups.size()
                                      && groups[i].containsIgnoreCase (query));
                if (! hit)
                    continue;
            }
            filtered.push_back (i);
        }
        // Headers follow their models: a header row opens each run of same-group
        // rows, and an empty group contributes nothing - it can never show over
        // an emptied run. Ungrouped hosts get the flat list unchanged.
        display.clear();
        juce::String lastGroup;   // run tracker for header placement
        for (const int i : filtered)
        {
            const bool hasGroup = grouped && i < groups.size()
                                   && groups[i].isNotEmpty();
            if (! hasGroup)
            {
                lastGroup = {};   // an ungrouped model resets the run tracker
                display.push_back (RowEntry{ {}, i });
                continue;
            }
            const auto g = groups[i];
            if (g != lastGroup)
            {
                display.push_back (RowEntry{ g, -1 });
                lastGroup = g;   // the header opens its run: a reader sees THEIR server first
            }
            display.push_back (RowEntry{ {}, i });
        }
        if (highlighted >= (int) filtered.size())
            highlighted = juce::jmax (0, (int) filtered.size() - 1);
        if (hoveredIndex >= (int) display.size())
            hoveredIndex = -1;
    }

    /** The DISPLAY row of the model at position `modelPos` inside filtered
        (-1 = not listed). Header-inclusive mapping for highlight + scroll. */
    int displayRowOfModel (int modelPos) const
    {
        if (modelPos < 0 || modelPos >= (int) filtered.size())
            return -1;
        const int idIndex = filtered[(size_t) modelPos];
        for (int r = 0; r < (int) display.size(); ++r)
            if (display[(size_t) r].idIndex == idIndex)
                return r;
        return -1;
    }

    /** "X of Y": X = the rows listed right now (search applied), Y = the rows
        ELIGIBLE (installed-only applied, search NOT applied). Headers are not
        models and count in neither. The author saw "10 of 10" over 8 rows
        because Y used the raw setItems count. */
    juce::String footerText() const
    {
        return juce::String ((int) filtered.size()) + " of "
             + juce::String ((int) eligible.size());
    }

    void setHoveredRow (int visibleIndex)
    {
        const int clamped = visibleIndex >= 0 && visibleIndex < (int) display.size()
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

    /** The CallOutBox content: search field, GROUPED + SCROLLABLE filtered
        list ("X of Y" footer, named empty state). The rows are painted at the
        picker's own rhythm; a scrollbar and the wheel move the window through
        every row so a 90-model catalog is fully reachable. */
    struct PopupContent : juce::Component,
                          private juce::ScrollBar::Listener
    {
        SearchableModelPicker* owner = nullptr;
        SearchField search;
        juce::Rectangle<int> listArea, footerArea, manageLinkArea,
                             footerCountArea, rowsBase;

        /** The scrollbar reports moves through onValueChange (juce::ScrollBar
            has NO valueChanged() to override in JUCE 8). */
        juce::ScrollBar scroll { true };   // vertical

        int offset = 0;        // display rows skipped above the top edge

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
            scroll.addListener (this);
            scroll.setRangeLimits (0.0, 0.0);
            scroll.setVisible (false);
            addAndMakeVisible (scroll);
            setSize (360, 26 + 4 + kMaxViewRows * kRowH + kFooterH + 4);
        }

        ~PopupContent() override
        {
            scroll.removeListener (this);
            if (owner != nullptr)
                owner->popupClosed (this);
        }

        void scrollBarMoved (juce::ScrollBar*, double newRangeStart) override
        {
            setScrollOffset ((int) (newRangeStart + 0.5));
        }

        static constexpr int kRowH = 24;
        static constexpr int kFooterH = 18;   // ONE row: link left, count right
        static constexpr int kMaxViewRows = 10;   // rows the popup shows at open
        static constexpr int kBarW = 12;          // scrollbar strip when content scrolls

        void resized() override
        {
            auto b = getLocalBounds().reduced (4);
            search.setBounds (b.removeFromTop (24));
            b.removeFromTop (4);
            footerArea = b.removeFromBottom (kFooterH);
            SearchableModelPicker::layoutFooterRow (
                footerArea, owner != nullptr && owner->onManageModels != nullptr,
                manageLinkArea, footerCountArea);
            rowsBase = b;
            updateScroll();
        }

        /** The scroll window is re-measured after every row change: range = the
            rows below the view, offset clamped into it, bar shown only when
            content actually scrolls. */
        void updateScroll()
        {
            const int total = owner != nullptr ? (int) owner->display.size() : 0;
            const int view = rowsBase.getHeight() / kRowH;
            const double upper = juce::jmax (0, total - view);
            offset = juce::jlimit (0, (int) upper, offset);
            scroll.setRangeLimits (0.0, upper);
            scroll.setCurrentRangeStart ((double) offset,
                                         juce::dontSendNotification);
            const bool showBar = upper > 0.0;
            scroll.setVisible (showBar);
            if (showBar)
            {
                auto strip = rowsBase;
                scroll.setBounds (strip.removeFromRight (kBarW));
                listArea = strip;
            }
            else
                listArea = rowsBase;
        }

        void setScrollOffset (int v)
        {
            const int total = owner != nullptr ? (int) owner->display.size() : 0;
            const int view = listArea.getHeight() / kRowH;
            const int clamped = juce::jlimit (0, juce::jmax (0, total - view), v);
            if (clamped == offset)
                return;
            offset = clamped;
            repaint();
        }

        /** Scroll the given DISPLAY row into the window (centre-biased): used
            by the keyboard and by OPEN, which lands on the current model - a
            reader opening a 90-model list expects to see THEIR model first. */
        void ensureRowVisible (int row)
        {
            const int total = owner != nullptr ? (int) owner->display.size() : 0;
            const int view = listArea.getHeight() / kRowH;
            const int upper = juce::jmax (0, total - view);
            if (row < 0 || row >= total)
                return;
            int target = offset;
            if (row < offset)
                target = row;
            else if (row >= offset + view)
                target = juce::jlimit (0, upper, row - view + 1);
            scroll.setCurrentRangeStart ((double) target);   // fires setScrollOffset
        }

        /** On open, park the window at the current model's row. */
        void moveToSelectedRow()
        {
            if (owner == nullptr)
                return;
            ensureRowVisible (owner->displayRowOfModel (owner->highlighted));
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
                                          : owner->displayRowOfModel (owner->highlighted);
                const int total = (int) owner->display.size();
                const int view = area.getHeight() / kRowH;
                for (int r = offset; r < total && r - offset < view; ++r)
                {
                    auto rowArea = area.removeFromTop (kRowH);
                    if (rowArea.isEmpty())
                        break;
                    const RowEntry& ent = owner->display[(size_t) r];
                    if (ent.isHeader())
                    {
                        // A provider header: uppercase, dim, never selectable
                        // (the old chat list - a reader scanning for THEIR server).
                        g.setColour (ebs::textDim());
                        g.setFont (ebs::fontHint());
                        g.drawText (ent.header.toUpperCase(),
                                    rowArea.reduced (6, 0),
                                    juce::Justification::centredLeft, true);
                        continue;
                    }
                    const int idIndex = ent.idIndex;
                    if (r == activeRow)
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
            // The row under the pointer counts from the TOP OF THE WINDOW, so
            // hover stays correct while the view is scrolled.
            owner->setHoveredRow (offset + (e.getPosition().y - listArea.getY()) / kRowH);
        }

        void mouseExit (const juce::MouseEvent&) override
        {
            if (owner != nullptr)
            {
                owner->setHoveredRow (-1);
                owner->setManageHovered (false);
            }
        }

        void mouseWheelMove (const juce::MouseEvent&,
                             const juce::MouseWheelDetails& e) override
        {
            const double upper = scroll.getRangeLimit().getEnd();
            if (owner == nullptr || upper <= 0.0)
                return;
            // The wheel moves the WINDOW, not the highlight: a reader scanning
            // a long catalog scrolls past whole provider runs.
            scroll.setCurrentRangeStart (juce::jlimit (
                0.0, upper,
                scroll.getCurrentRangeStart() - e.deltaY * 2.5));
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
            const int row = offset + (e.getPosition().y - listArea.getY()) / kRowH;
            if (row >= 0 && row < (int) owner->display.size()
                && ! owner->display[(size_t) row].isHeader())   // headers are not rows to pick
                owner->pick (owner->display[(size_t) row].idIndex);
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
        {
            popup->ensureRowVisible (displayRowOfModel (highlighted));
            popup->repaint();
        }
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
        searchText.clear();
        applyFilter();
        // The keyboard lands ON THE CURRENT MODEL when the reader opens the
        // list (the old chat behaviour, restored): scroll to its provider's run
        // instead of starting at row zero far from what is chosen. Without a
        // selection there is nothing to land on.
        highlighted = 0;
        if (selectedIdValue.isNotEmpty())
            for (int j = 0; j < (int) filtered.size(); ++j)
                if (ids[filtered[(size_t) j]] == selectedIdValue)
                    highlighted = j;
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
            const int rows = viewRowsForTest();   // jmin(kMaxViewRows, display size)
            content->setSize (juce::jmax (280, getWidth() - 2),
                              26 + 4 + rows * PopupContent::kRowH
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
                {
                    safe->popup->moveToSelectedRow();
                    safe->popup->search.grabKeyboardFocus();
                }
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
