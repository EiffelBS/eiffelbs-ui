// FilterableComboBox.h
// eiffelbs-ui - a juce::ComboBox-shaped control with a FILTER field and
// GROUP headers, for lists that outgrow a flat dropdown.
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// WHY: a juce::ComboBox cannot search and cannot group, and the app now has
// several long lists that are neither (the take lists of Stems/Voice/MIDI, the
// voice lists of TTS/Voice, the model pickers). This control keeps the
// ComboBox contract hosts already write against - addItem(text, id),
// setSelectedId(id), getSelectedId(), onChange - and delegates the POPUP to
// ebs::SearchableModelPicker, which already owns the proven behaviour:
//
//   * a filter field at the top of the list, an ebs::ClearableEditor, so the
//     cross appears as soon as there is text and clears the filter;
//   * group headers (parallel `group` per item, runs of the same group get one
//     header, typing a group name narrows to its rows);
//   * a real scrolling list, Enter picks, Escape closes with no selection;
//   * the "N of M" footer and case-insensitive matching over text AND group.
//
// The int item ids are the ComboBox convention; they are mapped to the
// picker's string ids internally, so a host can keep integer ids and swap a
// ComboBox for this control without rewriting its handlers.
//
// Typical use:
//
//     ebs::FilterableComboBox box;
//     box.addItemWithGroup ("take-001.wav", 1, "Speech");
//     box.addItemWithGroup ("take-002.wav", 2, "Singing");
//     box.setTextWhenNothingSelected ("Choose a take");
//     box.onChange = [&] { const int id = box.getSelectedId(); ... };
//
// ASCII source (repo convention).

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

#include "eiffelbs/SearchableModelPicker.h"

namespace ebs
{

class FilterableComboBox : public juce::Component,
                           public juce::SettableTooltipClient
{
public:
    FilterableComboBox()
    {
        addAndMakeVisible (picker);
        picker.onChange = [this] (const juce::String& id)
        {
            if (onChange != nullptr)
                onChange();
            juce::ignoreUnused (id);
        };
    }

    // === Data (ComboBox-shaped) ==========================================
    void addItem (const juce::String& text, int itemId)
    {
        addItemWithGroup (text, itemId, {});
    }

    /** Add one row under a group header. Rows must be added group by group to
        keep one contiguous run per header (SearchableModelPicker opens a
        header for each CHANGE of group, which is the documented contract). */
    void addItemWithGroup (const juce::String& text, int itemId,
                           const juce::String& group)
    {
        itemIds.add (itemId);
        itemTexts.add (text);
        itemGroups.add (group);
        rebuild();
    }

    /** ComboBox::clear(): drops every row and the selection, without notify. */
    void clear (juce::NotificationType notification = juce::dontSendNotification)
    {
        itemIds.clear();
        itemTexts.clear();
        itemGroups.clear();
        picker.setItems ({}, {});
        picker.setSelectedId ({}, false);
        if (notification != juce::dontSendNotification && onChange != nullptr)
            onChange();
        repaint();
    }

    int getNumItems() const noexcept { return itemIds.size(); }

    juce::String getItemText (int index) const
    {
        return juce::isPositiveAndBelow (index, itemIds.size())
                   ? itemTexts[index] : juce::String();
    }

    int getItemId (int index) const
    {
        return juce::isPositiveAndBelow (index, itemIds.size()) ? itemIds[index]
                                                                : 0;
    }

    int indexOfItemId (int itemId) const { return itemIds.indexOf (itemId); }

    void setSelectedId (int itemId,
                        juce::NotificationType notification
                            = juce::sendNotificationSync)
    {
        const juce::String key (itemId);
        picker.setSelectedId (itemIds.contains (itemId) ? key : juce::String(),
                              false);
        if (notification != juce::dontSendNotification && onChange != nullptr)
            onChange();
        repaint();
    }

    int getSelectedId() const
    {
        const auto id = picker.selectedId();
        return id.isEmpty() ? 0 : id.getIntValue();
    }

    void setSelectedItemIndex (int index,
                               juce::NotificationType notification
                                   = juce::sendNotificationSync)
    {
        setSelectedId (juce::isPositiveAndBelow (index, itemIds.size())
                           ? itemIds[index] : 0,
                       notification);
    }

    int getSelectedItemIndex() const { return indexOfItemId (getSelectedId()); }

    /** ComboBox's editable text is the selected row's label here: the filter
        lives in the popup, never in the closed control. */
    juce::String getText() const
    {
        const int index = getSelectedItemIndex();
        return index >= 0 ? itemTexts[index] : juce::String();
    }

    void setTextWhenNothingSelected (const juce::String& text)
    {
        picker.setTextWhenNothingSelected (text);
    }

    /** Optional "group by" pickers in the popup footer: `labels` are short
        segment names (Type / Model / Duration), `active` is the current one,
        `picked` is fired with the new index (the host rebuilds its groups). */
    void setGroupByOptions (const juce::StringArray& labels, int active,
                            std::function<void (int)> picked = {})
    {
        picker.setFooterOptions (labels, active, std::move (picked));
    }

    /** Open the filtered list programmatically (tests / keyboard helpers). */
    void showPopup() { picker.openForTest(); }

    /** The row count the popup would show for `filter` (tests). */
    int matchingRowsForTest (const juce::String& filter) const
    {
        int matches = 0;
        for (int i = 0; i < itemTexts.size(); ++i)
            if (itemTexts[i].containsIgnoreCase (filter)
                || itemGroups[i].containsIgnoreCase (filter))
                ++matches;
        return matches;
    }

    /** Fired on a user pick AND on a programmatic notification, exactly like
        juce::ComboBox::onChange (the host reads getSelectedId()). */
    std::function<void()> onChange;

    void resized() override { picker.setBounds (getLocalBounds()); }

    void paint (juce::Graphics& g) override { juce::ignoreUnused (g); }

private:
    void rebuild()
    {
        juce::StringArray ids, labels, groups;
        for (int i = 0; i < itemIds.size(); ++i)
        {
            ids.add (juce::String (itemIds[i]));
            labels.add (itemTexts[i]);
            groups.add (itemGroups[i]);
        }
        // Keep the current pick across a rebuild (a host appending rows must
        // not lose the selected take).
        const auto selected = picker.selectedId();
        picker.setItems (ids, labels, {}, groups);
        picker.setSelectedId (selected, false);
        repaint();
    }

    SearchableModelPicker picker;
    juce::Array<int>           itemIds;
    juce::StringArray          itemTexts, itemGroups;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FilterableComboBox)
};

} // namespace ebs
