// DataList.h
// eiffelbs-ui - generic filterable/sortable table list (EiffelBS design system).
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// A thin reusable shell over juce::TableListBox for homogeneous item lists
// that need named views, text search, sortable columns and per-row actions:
// take lists (multiple origins), model download lists (install button +
// progress + size + location), etc.
//
// Model: the host pushes plain Row values (id + per-column text); DataList
// owns the proxy pipeline view -> search -> sort and drives the table.
// Selection follows the stable row id, never the visible index.
//
// Cells are painted by the library (text + optional progress bar: an
// accent wash behind a text cell, or a dedicated Progress column with
// track + fill + percent when Row::progressIsBar is set). The host
// may supply per-cell COMPONENTS for special columns (e.g. a drag handle or
// an install button) through cellComponentProvider; action clicks on the
// leading action column route to onAction without any row component.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <eiffelbs/Theme.h>
#include <eiffelbs/Fonts.h>
#include <eiffelbs/IconButton.h>

#include <functional>
#include <algorithm>
#include <cstdlib>
#include <map>
#include <optional>
#include <vector>

namespace ebs
{

// Theme helpers live in ebs::Theme (Theme.h); the bare names below are
// used unqualified inside DataList (a member of namespace ebs).
using ebs::accent;
using ebs::bgDark;
using ebs::danger;
using ebs::panelBorder;
using ebs::textDim;

class DataList : public juce::Component,
                 private juce::TableListBoxModel
{
public:
    // === Data =============================================================

    struct Column
    {
        int id = 0;                 // unique, != 0
        juce::String title;
        int defaultWidth = 120;
        int minWidth = 40;
        int maxWidth = -1;          // -1 = unlimited
        bool sortable = true;
    };

    struct Row
    {
        juce::String id;            // stable identity (selection follows this)
        std::map<int, juce::String> cells;  // columnId -> text
        double progress = -1.0;     // >= 0: shown in progressColumnId
        int progressColumnId = 0;   // 0 = no progress cell
        // true = dedicated Progress-column look (track + fill + percent);
        // false = legacy accent wash behind the cell text.
        bool progressIsBar = false;
        juce::String tooltip;
    };

    struct View
    {
        juce::String name;
        // Empty match = accept all rows.
        std::function<bool (const Row&)> match;
    };

    struct Action
    {
        int actionId = 0;
        IconButton::Shape shape = IconButton::Shape::play;
        juce::String tooltip;
    };

    // === Setup ============================================================

    DataList()
    {
        addAndMakeVisible (searchBox);
        searchBox.setTextToShowWhenEmpty ("Search...", textDim());
        searchBox.setColour (juce::TextEditor::textColourId, text());
        searchBox.setColour (juce::TextEditor::backgroundColourId, bgDark());
        searchBox.setColour (juce::TextEditor::outlineColourId, panelBorder());
        searchBox.onTextChange = [this] { applyProxy(); };

        addAndMakeVisible (viewBox);
        viewBox.onChange = [this]
        {
            activeView = viewBox.getSelectedItemIndex();
            applyProxy();
        };

        addAndMakeVisible (table);
        table.setModel (this);
        table.setColour (juce::ListBox::backgroundColourId, bgPanel());
        table.setColour (juce::ListBox::outlineColourId, panelBorder());
        table.getHeader().setColour (
            juce::TableHeaderComponent::backgroundColourId, bgDark());
        table.getHeader().setColour (
            juce::TableHeaderComponent::textColourId, text());
        // Mouse listener on the table viewport: grip-slot drags + hand
        // cursor. The rows paint the grip glyph but own no component, so
        // the drag must start here: mouse-down on a grip slot arms it,
        // mouse-move past the dead zone fires onGripDrag (native OS file
        // drag, resolved AT DRAG TIME from the row id), which also
        // suppresses the pending action click (see cellClicked).
        table.getViewport()->addMouseListener (this, true);
    }

    /** Native file drag starting on a grip action slot: the host resolves
        the file list AT DRAG TIME from the row id (never captured), so
        recycled rows cannot drag a stale file. Return {} for no drag. */
    std::function<juce::StringArray (const juce::String& rowId)>
        gripFilesForRow;

    void setColumns (const std::vector<Column>& cols)
    {
        columns = cols;
        auto& header = table.getHeader();
        header.removeAllColumns();
        if (! actions.empty())
            header.addColumn ("", actionColumnId, actionWidth(),
                              actionWidth(), actionWidth(),
                              juce::TableHeaderComponent::notResizableOrSortable);
        for (const auto& c : columns)
            header.addColumn (c.title, c.id, c.defaultWidth,
                              c.minWidth, c.maxWidth,
                              c.sortable ? juce::TableHeaderComponent::defaultFlags
                                         : juce::TableHeaderComponent::notSortable);
        applyProxy();
    }

    void setViews (const std::vector<View>& v, int defaultIndex = 0)
    {
        views = v;
        viewBox.clear();
        for (int i = 0; i < (int) views.size(); ++i)
            viewBox.addItem (views[(size_t) i].name, i + 1);
        activeView = juce::jlimit (0, (int) views.size() - 1, defaultIndex);
        viewBox.setSelectedId (activeView + 1, juce::dontSendNotification);
        applyProxy();
    }

    void setRowActions (const std::vector<Action>& a)
    {
        actions = a;
        // Rebuild the header so the action column appears/disappears.
        setColumns (columns);
    }

    void setRows (const std::vector<Row>& r)
    {
        rows = r;
        applyProxy();
    }

    /** Custom per-cell components for special columns (drag handles,
        install buttons, ...). Return nullptr for library-painted cells.
        The returned component is owned by the table (JUCE recycling). */
    std::function<juce::Component* (int columnId, const juce::String& rowId,
                                    juce::Component* existing)> cellComponentProvider;

    /** Numeric-aware default: tries double comparison, falls back to
        case-insensitive text. Override for domain ordering. */
    std::function<int (const Row& a, const Row& b, int columnId)> comparer;

    /** Optional host-supplied glyph tint per action slot (e.g. red delete,
        gold favorite, green playing). Return transparent for the default. */
    std::function<juce::Colour (const juce::String& rowId, int actionId)>
        actionColour;

    /** Optional host-supplied filled/outline flag per action slot (star
        shape: filled = favourite). Default filled. */
    std::function<bool (const juce::String& rowId, int actionId)> actionFilled;

    /** Optional host-supplied SHAPE override per row+action (e.g. play
        becomes stop on the sounding row). Default = the declared shape. */
    std::function<IconButton::Shape (const juce::String& rowId, int actionId,
                                     IconButton::Shape declared)>
        actionShapeOverride;

    // === Toolbar visibility ===============================================

    void setShowSearch (bool on) { showSearch = on; resized(); }
    void setShowViews (bool on)  { showViews = on; resized(); }

    // === Selection ==========================================================

    void setMultipleSelectionEnabled (bool on)
    {
        table.setMultipleSelectionEnabled (on);
    }

    /** Select by stable id. Stays silent (same as programmatic
        ListBox selection with dontSendNotification). */
    void selectRowById (const juce::String& id, bool ensureVisible = false)
    {
        for (int i = 0; i < (int) visible.size(); ++i)
            if (visible[(size_t) i]->id == id)
            {
                table.selectRow (i, false, ensureVisible);
                return;
            }
        table.deselectAllRows();
    }

    /** Stable id of the row at a SOURCE index (pre-proxy position in the
        last setRows() vector), or "" when out of range. Lets hosts map a
        legacy absolute index (e.g. a playing-take scan) onto the proxy. */
    juce::String rowIdAtSourceIndex (int sourceIndex) const
    {
        return (sourceIndex >= 0 && sourceIndex < (int) rows.size())
            ? rows[(size_t) sourceIndex].id : juce::String();
    }

    /** Source index (last setRows() position) of a stable id, or -1. */
    int sourceIndexOf (const juce::String& id) const
    {
        for (int i = 0; i < (int) rows.size(); ++i)
            if (rows[(size_t) i].id == id)
                return i;
        return -1;
    }

    /** Row count of the underlying ListBox (visible rows scroll the same). */
    void setRowHeight (int h) { table.setRowHeight (h); }

    /** Force the table to rebuild visible components (state icons...). */
    void updateRows() { table.updateContent(); }

    /** Repaint one visible row without rebuilding components. */
    void repaintRow (int visibleIndex) { table.repaintRow (visibleIndex); }

    /** Visible index -> stable id ("" when out of range). */
    juce::String rowIdAt (int visibleIndex) const
    {
        return visibleRowId (visibleIndex);
    }

    juce::String selectedId() const
    {
        const int r = table.getSelectedRow();
        return (r >= 0 && r < (int) visible.size()) ? visible[(size_t) r]->id
                                                    : juce::String();
    }

    // === Host callbacks =======================================================

    std::function<void (const juce::String& rowId)> onSelection;
    std::function<void (const juce::String& rowId)> onDoubleClick;
    std::function<void (const juce::String& rowId, int actionId)> onAction;
    std::function<void (int columnId, bool forwards)> onSortChanged;
    /** Right-click on a row (e.g. context menu). Coordinates are local
        to the DataList. */
    std::function<void (const juce::String& rowId, juce::Point<int> pos)> onRightClick;

    /** Full refresh (e.g. progress ticks): re-runs the proxy + repaints. */
    void refresh() { applyProxy(); }

    void resized() override
    {
        auto b = getLocalBounds();
        if (showSearch || showViews)
        {
            auto bar = b.removeFromTop (30);
            bar.removeFromBottom (4);
            if (showViews)
                viewBox.setBounds (bar.removeFromLeft (170).reduced (0, 2));
            if (showSearch)
            {
                bar.removeFromLeft (6);
                searchBox.setBounds (bar.reduced (0, 2));
            }
        }
        else
        {
            searchBox.setBounds ({});
            viewBox.setBounds ({});
        }
        table.setBounds (b);
    }

    // === Grip drag (mouse listener on the table viewport) ===================

    void mouseMove (const juce::MouseEvent& e) override
    {
        updateGripCursor (e);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        gripDragArmed = false;
        if (const auto slot = actionSlotAt (e.getEventRelativeTo (&table)
                                                .getPosition()))
        {
            if (actions[(size_t) slot->slot].shape
                    == IconButton::Shape::grip)
            {
                gripDragArmed = true;
                gripDragArmedRow = slot->row;
                gripDragArmedSlot = slot->slot;
            }
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! gripDragArmed || gripFilesForRow == nullptr)
            return;
        if (e.getDistanceFromDragStart() < 8)
            return;                            // dead zone before the drag
        gripDragArmed = false;
        // Resolve AT DRAG TIME from the CURRENT row id (never captured).
        const int row = gripDragArmedRow;
        if (row < 0 || row >= (int) visible.size())
            return;
        const auto files = gripFilesForRow (visible[(size_t) row]->id);
        if (files.isEmpty())
            return;
        // allowMove = true: the DAW may import/move the file natively.
        // NOTE: blocks until the native OLE drag loop ends on Windows.
        juce::DragAndDropContainer::performExternalDragDropOfFiles (
            files, true, this);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        // Keep the armed state until cellClicked (mouse-up) consumed it:
        // clearing here would re-enable the click after a real drag.
    }

    // === Test hooks (also used by the smoke test) =============================

    int visibleRowCount() const noexcept { return (int) visible.size(); }
    juce::String visibleRowId (int index) const
    {
        return (index >= 0 && index < (int) visible.size())
            ? visible[(size_t) index]->id : juce::String();
    }
    void setSearchText (const juce::String& t)
    {
        searchBox.setText (t, juce::dontSendNotification);
        applyProxy();
    }
    void selectView (int index)
    {
        activeView = juce::jlimit (0, (int) views.size() - 1, index);
        viewBox.setSelectedId (activeView + 1, juce::dontSendNotification);
        applyProxy();
    }
    void sortBy (int columnId, bool forwards)
    {
        pendingSelectedId = selectedId();
        table.getHeader().setSortColumnId (columnId, forwards);
        // setSortColumnId notifies asynchronously (tableSortOrderChanged ->
        // sortOrderChanged -> applyProxy); mirror it NOW so headless and
        // synchronous callers observe the new order immediately.
        applyProxy();
    }

private:
    static constexpr int actionColumnId = 0x5da7; // "data" leet; id 0 forbidden
    static constexpr int actionSlotPx = 24;

    int actionWidth() const noexcept { return (int) actions.size() * actionSlotPx; }

    struct ActionSlot { int row = -1; int slot = -1; };

    /** Viewport position -> action slot, or nullopt when not on one.
        Rows painted by the library own no component, so geometry comes
        from the header layout + the row height. */
    std::optional<ActionSlot> actionSlotAt (juce::Point<int> tablePos) const
    {
        if (actions.empty())
            return std::nullopt;
        const int rowH = table.getRowHeight();
        if (rowH <= 0)
            return std::nullopt;
        const int firstVisible = table.getRowContainingPosition (0, tablePos.y);
        if (firstVisible < 0 || firstVisible >= (int) visible.size())
            return std::nullopt;
        const auto cellRect = table.getCellPosition (actionColumnId,
                                                     firstVisible, true);
        if (cellRect.isEmpty() || ! cellRect.contains (tablePos))
            return std::nullopt;
        const int slot = (tablePos.x - cellRect.getX()) / actionSlotPx;
        if (slot < 0 || slot >= (int) actions.size())
            return std::nullopt;
        return ActionSlot { firstVisible, slot };
    }

    void updateGripCursor (const juce::MouseEvent& e)
    {
        auto pos = e.getEventRelativeTo (&table).getPosition();
        bool overGrip = false;
        if (const auto slot = actionSlotAt (pos))
            overGrip = actions[(size_t) slot->slot].shape
                       == IconButton::Shape::grip;
        table.setMouseCursor (overGrip ? juce::MouseCursor::DraggingHandCursor
                                       : juce::MouseCursor::NormalCursor);
    }

    void applyProxy()
    {
        visible.clear();
        const juce::String query = searchBox.getText().trim().toLowerCase();
        for (auto& r : rows)
        {
            if (activeView >= 0 && activeView < (int) views.size()
                && views[(size_t) activeView].match
                && ! views[(size_t) activeView].match (r))
                continue;
            if (query.isNotEmpty())
            {
                bool hit = false;
                for (const auto& [col, text] : r.cells)
                    if (text.toLowerCase().contains (query)) { hit = true; break; }
                if (! hit && ! r.id.toLowerCase().contains (query))
                    continue;
            }
            visible.push_back (&r);
        }
        const int sortCol = table.getHeader().getSortColumnId();
        if (sortCol != 0 && sortCol != actionColumnId)
        {
            const bool fwd = table.getHeader().isSortedForwards();
            std::sort (visible.begin(), visible.end(),
                [&] (const Row* a, const Row* b)
                {
                    int cmp = 0;
                    if (comparer != nullptr)
                        cmp = comparer (*a, *b, sortCol);
                    else
                        cmp = compareDefault (*a, *b, sortCol);
                    return fwd ? (cmp < 0) : (cmp > 0);
                });
        }
        // Restore the selection on the same id when it is still visible.
        const juce::String keep = pendingSelectedId.isNotEmpty()
            ? pendingSelectedId : selectedId();
        table.updateContent();
        pendingSelectedId.clear();
        if (keep.isNotEmpty())
            selectRowByIdSilent (keep);
    }

    static int compareDefault (const Row& a, const Row& b, int columnId)
    {
        const auto textFor = [columnId] (const Row& r)
        {
            auto it = r.cells.find (columnId);
            return it != r.cells.end() ? it->second : juce::String();
        };
        const juce::String ta = textFor (a), tb = textFor (b);
        // Numeric-aware: full-string doubles compare numerically, anything
        // else falls back to case-insensitive text.
        char* endA = nullptr; char* endB = nullptr;
        const double va = std::strtod (ta.toRawUTF8(), &endA);
        const double vb = std::strtod (tb.toRawUTF8(), &endB);
        if (ta.isNotEmpty() && endA != nullptr && *endA == '\0'
            && tb.isNotEmpty() && endB != nullptr && *endB == '\0')
        {
            if (va < vb) return -1;
            if (va > vb) return 1;
            return 0;
        }
        return ta.compareIgnoreCase (tb);
    }

    void selectRowByIdSilent (const juce::String& id)
    {
        for (int i = 0; i < (int) visible.size(); ++i)
            if (visible[(size_t) i]->id == id)
            {
                table.selectRow (i, false, false);
                return;
            }
    }

    // --- TableListBoxModel ---

    int getNumRows() override { return (int) visible.size(); }

    void paintRowBackground (juce::Graphics& g, int rowNumber, int w, int h,
                             bool rowIsSelected) override
    {
        g.fillAll (rowIsSelected ? accentSoft() : bgPanel());
        juce::ignoreUnused (rowNumber, w, h);
    }

    void paintCell (juce::Graphics& g, int rowNumber, int columnId,
                    int w, int h, bool /*selected*/) override
    {
        if (rowNumber < 0 || rowNumber >= (int) visible.size())
            return;
        const Row& r = *visible[(size_t) rowNumber];

        if (columnId == actionColumnId)
        {
            // Per-row action buttons (hit-tested in cellClicked).
            const juce::String rowId = r.id;
            int x = 2;
            for (const auto& a : actions)
            {
                juce::Rectangle<float> box ((float) x + 2.0f, 2.0f, 20.0f,
                                            (float) h - 4.0f);
                juce::Colour tint;
                if (actionColour != nullptr)
                    tint = actionColour (rowId, a.actionId);
                auto shape = a.shape;
                if (actionShapeOverride != nullptr)
                    shape = actionShapeOverride (rowId, a.actionId, shape);
                const bool filled = actionFilled != nullptr
                    ? actionFilled (rowId, a.actionId) : true;
                paintActionGlyph (g, shape, box,
                                  tint.isTransparent() ? textDim() : tint,
                                  filled);
                x += actionSlotPx;
            }
            juce::ignoreUnused (w);
            return;
        }

        auto it = r.cells.find (columnId);
        const juce::String text = it != r.cells.end() ? it->second
                                                      : juce::String();
        if (r.progress >= 0.0 && columnId == r.progressColumnId)
        {
            // Dedicated Progress-column look: an inset track with a filled
            // portion + the percent text on top (reads as a progress bar,
            // not as a tinted text cell).
            if (r.progressIsBar)
            {
                const float frac = juce::jlimit (0.0f, 1.0f,
                                                 (float) r.progress);
                const auto track = juce::Rectangle<float> (
                    6.0f, (float) h * 0.5f - 5.0f,
                    (float) w - 12.0f, 10.0f);
                g.setColour (bgDark());
                g.fillRoundedRectangle (track, 5.0f);
                g.setColour (panelBorder());
                g.drawRoundedRectangle (track, 5.0f, 1.0f);
                if (frac > 0.0f)
                {
                    g.setColour (accent());
                    g.fillRoundedRectangle (
                        track.withWidth (track.getWidth() * frac), 5.0f);
                }
                g.setColour (ebs::text());
                g.setFont (ebs::fontHint());
                g.drawText (juce::String (juce::roundToInt (frac * 100.0f))
                                + "%",
                            6, 0, w - 12, h,
                            juce::Justification::centred, true);
                return;
            }
            const float frac = juce::jlimit (0.0f, 1.0f, (float) r.progress);
            g.setColour (accent().withAlpha (0.30f));
            g.fillRect (0, 0, (int) (w * frac), h);
        }
        g.setColour (ebs::text());
        g.setFont (ebs::fontBody());
        g.drawText (text, 6, 0, w - 12, h,
                    juce::Justification::centredLeft, true);
    }

    void paintActionGlyph (juce::Graphics& g, IconButton::Shape shape,
                           juce::Rectangle<float> box, juce::Colour colour,
                           bool filledStar = true)
    {
        // Glyphs stay pixel-identical to IconButton widgets through the
        // shared renderer (SVG-sourced lock/wand/refresh/hand included).
        g.setColour (colour);
        const auto glyphBox = box.withSizeKeepingCentre (10.0f, 10.0f);
        g.fillPath (IconButton::glyphPath (shape, glyphBox, filledStar,
                                           false, 10.0f));
    }

    juce::Component* refreshComponentForCell (int rowNumber, int columnId,
        bool /*isRowSelected*/, juce::Component* existing) override
    {
        if (cellComponentProvider == nullptr
            || rowNumber < 0 || rowNumber >= (int) visible.size())
        {
            delete existing;
            return nullptr;
        }
        return cellComponentProvider (columnId, visible[(size_t) rowNumber]->id,
                                      existing);
    }

    void cellClicked (int rowNumber, int columnId,
                      const juce::MouseEvent& e) override
    {
        if (rowNumber < 0 || rowNumber >= (int) visible.size())
            return;
        const juce::String rowId = visible[(size_t) rowNumber]->id;
        if (e.mods.isRightButtonDown() && onRightClick != nullptr)
        {
            table.selectRow (rowNumber, false, false);
            onRightClick (rowId, e.getEventRelativeTo (this).getPosition());
            return;
        }
        if (columnId == actionColumnId)
        {
            // Which action slot? Coordinates are relative to the row.
            // cellClicked fires on mouse-down AND mouse-up: only the UP
            // (a real click) triggers the action. A drag that STARTED on a
            // grip slot suppresses the click: the drag was already handed
            // to the OS on mouse-move (see the table mouse listener).
            const auto cellRect = table.getCellPosition (columnId, rowNumber,
                                                         false);
            const int slot = (e.x - cellRect.getX()) / actionSlotPx;
            if (slot < 0 || slot >= (int) actions.size())
                return;
            if (e.getDistanceFromDragStart() == 0
                && ! e.mouseWasDraggedSinceMouseDown())
                return;                        // mouse-down: select only
            if (gripDragArmed && gripDragArmedRow == rowNumber
                && gripDragArmedSlot == slot
                && onAction != nullptr
                && actions[(size_t) slot].shape
                       == IconButton::Shape::grip)
                return;                        // was a grip drag: no click
            if (onAction != nullptr)
                onAction (rowId, actions[(size_t) slot].actionId);
            return;
        }
        table.selectRowsBasedOnModifierKeys (rowNumber, e.mods, false);
    }

    void cellDoubleClicked (int rowNumber, int /*columnId*/,
                            const juce::MouseEvent&) override
    {
        if (rowNumber >= 0 && rowNumber < (int) visible.size()
            && onDoubleClick != nullptr)
            onDoubleClick (visible[(size_t) rowNumber]->id);
    }

    void selectedRowsChanged (int lastRowSelected) override
    {
        pendingSelectedId.clear();
        if (onSelection != nullptr && lastRowSelected >= 0
            && lastRowSelected < (int) visible.size())
            onSelection (visible[(size_t) lastRowSelected]->id);
    }

    void sortOrderChanged (int newSortColumnId, bool isForwards) override
    {
        pendingSelectedId = selectedId();
        applyProxy();
        if (onSortChanged != nullptr)
            onSortChanged (newSortColumnId, isForwards);
    }

    juce::String getCellTooltip (int rowNumber, int columnId) override
    {
        if (rowNumber < 0 || rowNumber >= (int) visible.size())
            return {};
        const Row& r = *visible[(size_t) rowNumber];
        if (columnId == actionColumnId)
        {
            // Action slot under the cursor? Coordinates unknown here, so
            // report all slot tooltips joined (JUCE has no per-pixel hook).
            juce::String t;
            for (const auto& a : actions)
            {
                if (a.tooltip.isNotEmpty())
                {
                    if (t.isNotEmpty()) t << " | ";
                    t << a.tooltip;
                }
            }
            return t.isNotEmpty() ? t : r.tooltip;
        }
        if (r.tooltip.isNotEmpty())
            return r.tooltip;
        auto it = r.cells.find (columnId);
        return it != r.cells.end() ? it->second : juce::String();
    }

    // === Members ===

    std::vector<Column> columns;
    std::vector<View> views;
    std::vector<Action> actions;
    std::vector<Row> rows;
    std::vector<const Row*> visible;
    int activeView = 0;
    bool showSearch = true, showViews = true;
    juce::String pendingSelectedId; // selection kept across re-sorts
    // Grip-drag arming (mouse-down slot -> mouse-move past dead zone).
    bool gripDragArmed = false;
    int gripDragArmedRow = -1, gripDragArmedSlot = -1;

    juce::TextEditor searchBox;
    juce::ComboBox viewBox;
    juce::TableListBox table { "DataList", this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DataList)
};

} // namespace ebs
