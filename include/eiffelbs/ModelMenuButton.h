// ModelMenuButton.h
// eiffelbs-ui - the ONE "model + effort" chevron: a quiet button whose label is
// the current model, with the current effort trailing it in dim type, sized to
// its CONTENT. Clicking opens the same two-row menu the chat input has always
// had (Model / Effort); the Model row opens the shared searchable model list,
// so a picker and a menu button can never drift apart.
//
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// WHY THIS EXISTS: the assistant chat input drew its own chevron, its own
// two-row callout and a filterable ListBox; the "Generate with AI" dialog drew
// the same chevron from a combined "model - effort" string, ellipsized it, and
// lost the effort (author report 2026-10-25). One component, one list, one
// label layout: the model may ellipsize, the effort stays readable, and every
// surface shows the same two facts.
//
// THE WIDTH IS THE COMPONENT'S OWN: `widthForContent()` is what a layout asks
// for. A host that stretches it across a full row is the layout that hides the
// effort behind an ellipsis; `setMaxWidth()` caps it where the host must.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "eiffelbs/Theme.h"
#include "eiffelbs/Fonts.h"
#include "eiffelbs/MenuChevronButton.h"
#include "eiffelbs/SearchableModelPicker.h"

#include <functional>

namespace ebs
{

class ModelMenuButton : public juce::Component
{
public:
    ModelMenuButton()
    {
        face.setTooltip ("Choose the model and how much it thinks");
        face.onClick = [this] { showMenu(); };
        addAndMakeVisible (face);

        // The picker is a HELPER: it owns the shared list logic (search,
        // filtering, hover, footer) and opens its popup anchored to the face.
        // It is never painted itself - the face is the visible control.
        picker.setVisible (false);
        picker.onChange = [this] (const juce::String& id)
        {
            selectedModelIdValue = id;
            face.setButtonText (labelForModel (id));
            resized();
            if (onModelChosen != nullptr)
                onModelChosen (id);
        };
        addChildComponent (picker);
        face.setButtonText ("server default");
    }

    // === The two facts the face shows ====================================
    /** The model name: left, may ellipsize when a max width forces it. */
    void setModelText (const juce::String& text)
    {
        modelText = text;
        face.setButtonText (modelText.isNotEmpty() ? modelText
                                                   : juce::String ("server default"));
        resized();
    }
    juce::String getModelText() const { return modelText; }

    /** The effort word: right, dim, ALWAYS readable (the fact a plain combined
        label loses first when the text is clipped). */
    void setEffortText (const juce::String& text)
    {
        effortText = text;
        face.setTrailingText (effortText);
        resized();
    }
    juce::String getEffortText() const { return effortText; }

    // === The model list (the shared searchable picker) ===================
    void setModels (const juce::StringArray& ids,
                    const juce::StringArray& labels,
                    const juce::StringArray& details = {})
    {
        modelIds = ids;
        modelLabels = labels;
        picker.setItems (ids, labels, details);
        if (selectedModelIdValue.isNotEmpty())
            face.setButtonText (labelForModel (selectedModelIdValue));
    }
    void setSelectedModelId (const juce::String& id)
    {
        selectedModelIdValue = id;
        picker.setSelectedId (id, false);
        face.setButtonText (labelForModel (id));
        resized();
    }
    juce::String getSelectedModelId() const { return selectedModelIdValue; }
    std::function<void (const juce::String&)> onModelChosen;

    // === The effort list =================================================
    void setEfforts (const juce::StringArray& ids,
                     const juce::StringArray& labels)
    {
        effortIds = ids;
        effortLabels = labels;
    }
    void setSelectedEffortId (const juce::String& id)
    {
        selectedEffortIdValue = id;
        for (int i = 0; i < effortIds.size(); ++i)
            if (effortIds[i] == id)
                setEffortText (effortLabels[i]);
    }
    juce::String getSelectedEffortId() const { return selectedEffortIdValue; }
    std::function<void (const juce::String&)> onEffortChosen;

    /** A host that needs the row filled may cap the width; 0 = no cap. */
    void setMaxWidth (int maxPixels) { maxWidth = juce::jmax (0, maxPixels); }
    int getMaxWidth() const { return maxWidth; }

    /** The width this button needs to show both facts - the number a layout
        must use instead of the row's full width. */
    int widthForContent() const
    {
        const int natural = face.widthForLabel();
        return maxWidth > 0 ? juce::jmin (natural, maxWidth) : natural;
    }

    /** The face's height (the button has no chrome of its own). */
    int heightForContent() const { return 26; }

    void resized() override
    {
        face.setBounds (getLocalBounds());
        picker.setBounds (getLocalBounds());   // helper only: anchor fallback
    }

    // === The menu ========================================================
    /** Open the two-row Model / Effort menu. Public so a host may open it from
        its own affordance and the gate can drive the same path headlessly. */
    void showMenu()
    {
        juce::PopupMenu menu;
        menu.addItem (1, "Model: " + (modelText.isNotEmpty()
                                          ? modelText
                                          : juce::String ("server default")));
        menu.addItem (2, "Effort: " + (effortText.isNotEmpty()
                                           ? effortText
                                           : juce::String ("Auto")));
        menuOpen = true;
        menu.showMenuAsync (
            juce::PopupMenu::Options()
                .withTargetComponent (&face)
                .withTargetScreenArea (face.getScreenBounds())
                .withMinimumWidth (juce::jmax (170, widthForContent())),
            [safe = juce::Component::SafePointer<ModelMenuButton> (this)]
            (int result)
            {
                if (safe != nullptr)
                    safe->onMenuResult (result);
            });
    }

    // === TEST SEAMS ======================================================
    // The smoke test renders offscreen: it proves geometry, text and painting,
    // and it can drive the menu result through the SAME handler a click runs.
    int menuItemCountForTest() const { return 2; }
    juce::String menuItemTextForTest (int row) const
    {
        return row == 0
            ? "Model: " + (modelText.isNotEmpty() ? modelText
                                                  : juce::String ("server default"))
            : "Effort: " + (effortText.isNotEmpty() ? effortText
                                                    : juce::String ("Auto"));
    }
    void chooseMenuResultForTest (int row) { onMenuResult (row + 1); }
    /** Drive the shared picker's list the way the popup would. */
    void pickerOpenForTest() { picker.openForTest(); }
    int pickerVisibleCountForTest() const { return picker.visibleIdCountForTest(); }
    void pickerSearchForTest (const juce::String& t)
    {
        picker.setSearchForTest (t);
    }

private:
    juce::String labelForModel (const juce::String& id) const
    {
        for (int i = 0; i < modelIds.size(); ++i)
            if (modelIds[i] == id)
                return i < modelLabels.size() ? modelLabels[i] : id;
        return id;
    }

    void onMenuResult (int result)
    {
        menuOpen = false;
        if (result == 1 && ! modelIds.isEmpty())
            picker.openPopupFor (&face);
        else if (result == 2)
            showEffortMenu();
    }

    void showEffortMenu()
    {
        if (effortIds.isEmpty())
            return;
        juce::PopupMenu menu;
        for (int i = 0; i < effortIds.size(); ++i)
            menu.addItem (i + 1, effortLabels[i], true,
                          effortIds[i] == selectedEffortIdValue);
        menu.showMenuAsync (
            juce::PopupMenu::Options()
                .withTargetComponent (&face)
                .withTargetScreenArea (face.getScreenBounds()),
            [safe = juce::Component::SafePointer<ModelMenuButton> (this)]
            (int result)
            {
                if (safe == nullptr || result <= 0
                    || result > safe->effortIds.size())
                    return;
                const auto id = safe->effortIds[result - 1];
                safe->selectedEffortIdValue = id;
                safe->setEffortText (safe->effortLabels[result - 1]);
                if (safe->onEffortChosen != nullptr)
                    safe->onEffortChosen (id);
            });
    }

    MenuChevronButton face;
    SearchableModelPicker picker;
    juce::String modelText, effortText, selectedModelIdValue,
        selectedEffortIdValue;
    juce::StringArray modelIds, modelLabels, effortIds, effortLabels;
    int maxWidth = 0;
    bool menuOpen = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModelMenuButton)
};

} // namespace ebs
