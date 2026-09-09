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
        searchBox.onTextChange = [this]
        {
            applyProxy();
            if (onSearchChanged != nullptr)
                onSearchChanged (searchBox.getText());
        };

        addAndMakeVisible (viewBox);
        viewBox.onChange = [this]
        {
            activeView = viewBox.getSelectedItemIndex();
            applyProxy();
            if (onViewChanged != nullptr)
                onViewChanged (activeView);
        };

        addAndMakeVisible (table);
        table.setModel (this);
        table.setColour (juce::ListBox::backgroundColourId, bgPanel());
        table.setColour (juce::ListBox::outlineColourId, panelBorder());
        table.getHeader().setColour (
            juce::TableHeaderComponent::backgroundColourId, bgDark());
        table.getHeader().setColour (
            juce::TableHeaderComponent::textColourId, text());
        // Right-click on the HEADER opens OUR column menu (functional
        // content-aware auto-size) - see mouseDown below.
        table.getHeader().addMouseListener (this, false);
        // Grip affordance lives in transparent overlays (see below), NOT
        // in a viewport mouse listener: JUCE's RowComponents sit on top
        // and swallow hover before any listener sees it. The overlays are
        // repositioned on every scroll/resize (listWasScrolled hook).
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (event.eventComponent != &table.getHeader()
            || ! event.mods.isPopupMenu())
            return;
        auto& header = table.getHeader();
        const auto local = event.getEventRelativeTo (&header).position;
        const int col = header.getColumnIdAtX ((int) local.x);
        juce::PopupMenu menu;
        if (col > 0)
            menu.addItem (juce::PopupMenu::Item ("Auto-size this column")
                              .setAction ([this, col]
                                          { autoSizeColumn (col); }));
        menu.addItem (juce::PopupMenu::Item ("Auto-size all columns")
                          .setAction ([this] { autoSizeAllColumns(); }));
        // Show/hide chooser (restores the JUCE built-in entry that the
        // appearsOnColumnMenu flag used to provide).
        juce::PopupMenu showMenu;
        for (const auto& c : columns)
            showMenu.addItem (juce::PopupMenu::Item (c.title)
                                  .setTicked (header.isColumnVisible (c.id))
                                  .setAction ([this, c]
                                  {
                                      const bool on =
                                          table.getHeader().isColumnVisible (c.id);
                                      table.getHeader().setColumnVisible (c.id, ! on);
                                  }));
        menu.addSubMenu ("Display columns", showMenu);
        menu.showMenuAsync (juce::PopupMenu::Options()
                                .withTargetComponent (this)
                                .withMousePosition());
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
                              // Explicit visible|resizable|draggable(|sortable)
                              // WITHOUT appearsOnColumnMenu: JUCE's built-in
                              // header menu offers "Auto-size this column"
                              // but its implementation measures only the
                              // HEADER TITLE, never cell content - it reads
                              // as broken. We provide our own right-click
                              // header menu with a CONTENT-AWARE auto-size
                              // AND the show/hide column chooser.
                              (int) juce::TableHeaderComponent::visible
                                  | (int) juce::TableHeaderComponent::resizable
                                  | (int) juce::TableHeaderComponent::draggable
                                  | (c.sortable
                                     ? (int) juce::TableHeaderComponent::sortable : 0));
        applyProxy();
    }

    /** Content-aware auto-size: measures the widest CELL TEXT among the
     *  proxy-visible rows (plus the header title), clamped to the
     *  column's min/max. This is what JUCE's built-in "Auto-size" menu
     *  item pretends to do (it only measures the header title). */
    void autoSizeColumn (int columnId, bool includeHeader = true)
    {
        const Column* col = nullptr;
        for (const auto& c : columns)
            if (c.id == columnId) { col = &c; break; }
        if (col == nullptr)
            return;
        const auto font = ebs::fontBody();
        int best = 0;
        if (includeHeader)
            best = font.getStringWidth (col->title) + 22;
        for (const auto& r : visible)
        {
            auto it = r->cells.find (columnId);
            if (it == r->cells.end() || it->second.isEmpty())
                continue;
            best = juce::jmax (best, font.getStringWidth (it->second) + 16);
        }
        // maxWidth < 0 = unlimited: use a generous cap, NOT jlimit's upper
        // bound (jlimit (min, -1, best) returns -1 -> header clamps to the
        // MIN width - the exact "columns collapse" bug users saw).
        const int hi = col->maxWidth < 0 ? 4096 : col->maxWidth;
        table.getHeader().setColumnWidth (columnId,
                                          juce::jlimit (col->minWidth, hi, best));
        table.resized();
        repaint();
    }

    void autoSizeAllColumns()
    {
        for (const auto& c : columns)
            autoSizeColumn (c.id);
        repaint();
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
        // Painted rows cache their cell content: a full content update
        // plus an eager repaint so state changes (action glyphs, progress
        // bars) show WITHOUT waiting for the next unrelated repaint.
        // (Hosts that poll progress call this at several Hz; the cost is
        // one lightweight table repaint.)
        table.updateContent();
        table.repaint();
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

    // === State accessors (hosts persist sort/view/search) =====================

    int sortColumnId() const noexcept
    {
        return table.getHeader().getSortColumnId();
    }
    bool sortForwards() const noexcept
    {
        return table.getHeader().isSortedForwards();
    }
    int activeViewIndex() const noexcept { return activeView; }
    juce::String searchText() const { return searchBox.getText(); }

    // === Host callbacks =======================================================

    std::function<void (const juce::String& rowId)> onSelection;
    std::function<void (const juce::String& rowId)> onDoubleClick;
    std::function<void (const juce::String& rowId, int actionId)> onAction;
    std::function<void (int columnId, bool forwards)> onSortChanged;
    std::function<void (int viewIndex)> onViewChanged;
    std::function<void (const juce::String& text)> onSearchChanged;
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
        // Table geometry changed: re-glue the grip overlays to the rows.
        refreshGripOverlays();
    }

    // === Grip drag (transparent overlay per visible row) ====================
    // The overlay (see below) owns BOTH the hover cursor AND the drag
    // gesture: it sits ON TOP of JUCE's RowComponents, hit-tests ONLY the
    // grip slot rect, and forwards nothing else (selection/clicks on the
    // rest of the row behave exactly as before). The old viewport-listener
    // + setMouseCursor approaches are REMOVED: proven dead (rows on top
    // impose their own cursor, listeners never see hover).

    /** Arm a grip drag for a visible row (called by the overlay). */
    void armGripDrag (int visibleRow, const juce::MouseEvent&)
    {
        gripDragArmed = false;
        int gripSlot = -1;
        for (int s = 0; s < (int) actions.size(); ++s)
            if (actions[(size_t) s].shape == IconButton::Shape::grip)
            { gripSlot = s; break; }
        if (gripSlot < 0 || visibleRow < 0
            || visibleRow >= (int) visible.size())
            return;
        gripDragArmed = true;
        gripDragArmedRow = visibleRow;
        gripDragArmedSlot = gripSlot;
    }

    /** Continue a grip drag (called by the overlay). */
    void dragGrip (int visibleRow, const juce::MouseEvent& e)
    {
        juce::ignoreUnused (visibleRow);
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

    // (actionSlotAt REMOVED: geometry now comes from getCellPosition per
    // visible row in refreshGripOverlays - no manual row math.)

    bool hasGripAction() const noexcept
    {
        for (const auto& a : actions)
            if (a.shape == IconButton::Shape::grip)
                return true;
        return false;
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
            // Stable order: ties fall back to the stable row id so rows
            // with EQUAL sort keys (e.g. several "downloading NN%" /
            // "queued #N" statuses, or an empty Progress column) never
            // jump around on refresh. Without this, every progress tick
            // rebuilds the rows and std::sort freely permutes equal
            // keys - the downloading row visibly hops while it downloads.
            std::stable_sort (visible.begin(), visible.end(),
                [&] (const Row* a, const Row* b)
                {
                    int cmp = 0;
                    if (comparer != nullptr)
                        cmp = comparer (*a, *b, sortCol);
                    else
                        cmp = compareDefault (*a, *b, sortCol);
                    if (cmp == 0)
                        cmp = a->id.compareIgnoreCase (b->id);
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
        // Row geometry changed (new rows, sort, filter): rebuild the grip
        // overlays so cursor + drag stay glued to the grip slots.
        refreshGripOverlays();
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
            // (a real click, or a down+up coalesced into one with a zero
            // drag distance) triggers the action. A drag that STARTED on
            // a grip slot suppresses the click: the drag was already
            // handed to the OS on mouse-move (see the table listener).
            const auto cellRect = table.getCellPosition (columnId, rowNumber,
                                                         false);
            const int slot = (e.x - cellRect.getX()) / actionSlotPx;
            if (slot < 0 || slot >= (int) actions.size())
                return;
            if (gripDragArmed && gripDragArmedRow == rowNumber
                && gripDragArmedSlot == slot
                && actions[(size_t) slot].shape
                       == IconButton::Shape::grip)
                return;                        // was a grip drag: no click
            if (e.getDistanceFromDragStart() != 0
                || e.mouseWasDraggedSinceMouseDown())
                return;                        // dragged elsewhere: no click
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

    // === Grip cursor (transparent overlay per visible row) =================
    // Why an overlay: the rows are JUCE-owned RowComponents sitting ON TOP
    // of the viewport, so viewport/table mouse listeners never see hover
    // (proven: mouseEnter/mouseMove + setMouseCursor had zero effect), and
    // TableListBoxModel has NO getMouseCursorForRow hook (only the plain
    // ListBoxModel does - verified in juce_ListBox.h:179, C3668 proven).
    // Polling setMouseCursor on the viewport/table also failed: JUCE
    // queries the cursor on the TOP component (the RowComp, NormalCursor).
    // So each visible row gets a transparent MouseListener overlay carrying
    // the DraggingHandCursor over the grip slot rect. The overlay paints
    // NOTHING (fully transparent, non-opaque) and forwards clicks to the
    // table (cellClicked/action logic untouched) - it only owns the cursor
    // and the drag-arming gesture.
    struct GripOverlay : public juce::Component
    {
        DataList* owner = nullptr;
        int row = -1, gripX = 0, gripW = 0;   // grip slot rect (row-local)

        GripOverlay (DataList* o, int r) : owner (o), row (r)
        {
            setInterceptsMouseClicks (true, false);   // self only, not children
            setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        }

        // Hit-test: ONLY the grip slot rect is clickable (cursor shows);
        // everywhere else the overlay is invisible to the mouse and the
        // row underneath behaves exactly as before (selection, clicks...).
        bool hitTest (int x, int /*y*/) override
        {
            return x >= gripX && x < gripX + gripW;
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (owner != nullptr)
                owner->armGripDrag (row, e);
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (owner != nullptr)
                owner->dragGrip (row, e);
        }
    };

    /** Rebuild the grip overlays after any layout/row change: one overlay
        per visible row, positioned over the action cell's grip slot. */
    void refreshGripOverlays()
    {
        // Drop previous overlays (OwnedArray delete = remove from parent).
        gripOverlays.clear();
        if (! hasGripAction())
            return;
        const int rowH = table.getRowHeight();
        if (rowH <= 0)
            return;
        int gripSlot = -1;
        for (int s = 0; s < (int) actions.size(); ++s)
            if (actions[(size_t) s].shape == IconButton::Shape::grip)
            { gripSlot = s; break; }
        if (gripSlot < 0)
            return;
        for (int r = 0; r < (int) visible.size(); ++r)
        {
            const auto cell = table.getCellPosition (actionColumnId, r, true);
            if (cell.isEmpty())
                continue;                    // row off-screen: no overlay
            auto* ov = new GripOverlay (this, r);
            gripOverlays.add (ov);
            table.addAndMakeVisible (ov);    // on top of the rows
            ov->setBounds (cell);
            // Row-local grip rect: the slot slice of the action cell.
            ov->gripX = gripSlot * actionSlotPx;
            ov->gripW = actionSlotPx;
            ov->toFront (false);
        }
    }

    void listWasScrolled() override { refreshGripOverlays(); }

    // (Unused symmetry helper REMOVED: TableListBoxModel has no cursor
    // hook - C3668. The live path is pollGripCursor() above.)

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

    // === Test hooks (smoke test only) ===================================

public:
    int actionColumnIdForTest() const noexcept { return actionColumnId; }
    juce::TableListBoxModel* modelForTest() noexcept { return this; }
    juce::Rectangle<int> actionCellBoundsForTest (int visibleRow) const
    {
        return table.getCellPosition (actionColumnId, visibleRow, false);
    }

private:
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
    // Transparent per-row cursor/drag overlays (owned here, parented to
    // the table so they float above JUCE's RowComponents).
    juce::OwnedArray<GripOverlay> gripOverlays;

    juce::TextEditor searchBox;
    juce::ComboBox viewBox;
    juce::TableListBox table { "DataList", this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DataList)
};

} // namespace ebs
